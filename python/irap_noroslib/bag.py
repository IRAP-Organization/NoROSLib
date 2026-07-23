"""Read and write ROS bag files (format v2.0) -- no ROS installed.

A bag is ROS's on-disk container for recorded messages. This implements the
real, documented v2.0 layout (http://wiki.ros.org/Bags/Format/2.0), so a bag
written here opens in genuine `rosbag info` / `rosbag play` / `rqt_bag`, and a
bag recorded by real ROS reads back here.

    with BagWriter("out.bag") as w:
        conn = w.connection("/chatter", "std_msgs/String", md5, definition)
        w.write(conn, secs, nsecs, body_bytes)

    for topic, conn, t, body in BagReader("out.bag").read_messages():
        ...

Only the uncompressed encoding is written (compression="none"); a compressed
bag written elsewhere still reads, chunk by chunk, if bz2/lz4 is available.
"""
import bz2
import struct
import time

# Record op codes (the 'op' header field, one byte).
OP_MSG_DATA = 0x02
OP_BAG_HEADER = 0x03
OP_IDX_DATA = 0x04
OP_CHUNK = 0x05
OP_CHUNK_INFO = 0x06
OP_CONNECTION = 0x07

_CHUNK_THRESHOLD = 768 * 1024      # rosbag's default chunk size


# ------------------------------------------------------------ header codec ----
def _enc_field(name, value_bytes):
    field = name.encode("ascii") + b"=" + value_bytes
    return struct.pack("<I", len(field)) + field


def encode_header_fields(fields):
    """fields: list of (name, value_bytes) -> the raw field sequence, no outer
    length. This is exactly a connection record's DATA payload."""
    return b"".join(_enc_field(n, v) for n, v in fields)


def encode_header(fields):
    """A length-prefixed header block: <4-byte len><fields>. Used for the HEADER
    part of every record (the outer data_len delimits DATA, so DATA payloads that
    happen to be headers -- connections -- use encode_header_fields instead)."""
    body = encode_header_fields(fields)
    return struct.pack("<I", len(body)) + body


def _decode_header_bytes(blob):
    """blob is the header body (no outer length). -> {name: value_bytes}."""
    out = {}
    i = 0
    while i < len(blob):
        (flen,) = struct.unpack_from("<I", blob, i)
        i += 4
        field = blob[i:i + flen]
        i += flen
        eq = field.index(b"=")
        out[field[:eq].decode("ascii")] = field[eq + 1:]
    return out


def _u8(v):
    return struct.pack("<B", v)


def _u32(v):
    return struct.pack("<I", v)


def _u64(v):
    return struct.pack("<Q", v)


def _time(secs, nsecs):
    return struct.pack("<II", secs, nsecs)


# --------------------------------------------------------------- connection ----
class _Conn:
    def __init__(self, conn_id, topic, type_name, md5, definition, callerid="", latching=""):
        self.id = conn_id
        self.topic = topic
        self.type = type_name
        self.md5 = md5
        self.definition = definition
        self.callerid = callerid
        self.latching = latching
        self.count = 0

    def header_data(self):
        """The connection header stored in the record's DATA field."""
        fields = [
            ("topic", self.topic.encode("utf-8")),
            ("type", self.type.encode("utf-8")),
            ("md5sum", self.md5.encode("ascii")),
            ("message_definition", self.definition.encode("utf-8")),
        ]
        if self.callerid:
            fields.append(("callerid", self.callerid.encode("utf-8")))
        if self.latching:
            fields.append(("latching", self.latching.encode("ascii")))
        return encode_header_fields(fields)


# ------------------------------------------------------------------ writer ----
class BagWriter:
    """Write a valid, real-ROS-readable v2.0 bag."""

    def __init__(self, path):
        self.path = path
        self.f = open(path, "wb")
        self.f.write(b"#ROSBAG V2.0\n")
        # Reserve the bag-header record; rewritten at close with real offsets.
        self._bagheader_pos = self.f.tell()
        self.f.write(self._bag_header_record(0, 0, 0))
        self._conns = {}                 # topic -> _Conn
        self._conn_by_id = {}
        self._chunk_buf = []             # (conn_id, secs, nsecs, body)
        self._chunk_bytes = 0
        self._chunk_infos = []           # (chunk_pos, start, end, {conn: count})
        self._start_time = None
        self._end_time = None

    # -- public -----------------------------------------------------------
    def connection(self, topic, type_name, md5, definition, callerid="", latching=""):
        c = self._conns.get(topic)
        if c is None:
            cid = len(self._conns)
            c = _Conn(cid, topic, type_name, md5, definition, callerid, latching)
            self._conns[topic] = c
            self._conn_by_id[cid] = c
        return c

    def write(self, conn, secs, nsecs, body):
        self._chunk_buf.append((conn.id, secs, nsecs, body))
        self._chunk_bytes += len(body)
        conn.count += 1
        t = (secs, nsecs)
        if self._start_time is None or t < self._start_time:
            self._start_time = t
        if self._end_time is None or t > self._end_time:
            self._end_time = t
        if self._chunk_bytes >= _CHUNK_THRESHOLD:
            self._flush_chunk()

    def close(self):
        if self.f is None:
            return
        self._flush_chunk()
        index_pos = self.f.tell()
        # File-level connection records, then chunk-info records.
        for c in self._conn_by_id.values():
            self._write_record(
                [("op", _u8(OP_CONNECTION)), ("conn", _u32(c.id)),
                 ("topic", c.topic.encode("utf-8"))], c.header_data())
        for chunk_pos, start, end, counts in self._chunk_infos:
            data = b"".join(_u32(cid) + _u32(n) for cid, n in counts.items())
            self._write_record(
                [("op", _u8(OP_CHUNK_INFO)), ("ver", _u32(1)),
                 ("chunk_pos", _u64(chunk_pos)),
                 ("start_time", _time(*start)), ("end_time", _time(*end)),
                 ("count", _u32(len(counts)))], data)
        # Rewrite the bag header now that we know the offsets.
        self.f.seek(self._bagheader_pos)
        self.f.write(self._bag_header_record(index_pos, len(self._conns),
                                             len(self._chunk_infos)))
        self.f.close()
        self.f = None

    def __enter__(self):
        return self

    def __exit__(self, *a):
        self.close()

    # -- internals --------------------------------------------------------
    def _write_record(self, header_fields, data):
        self.f.write(encode_header(header_fields))
        self.f.write(_u32(len(data)))
        self.f.write(data)

    def _bag_header_record(self, index_pos, conn_count, chunk_count):
        header = encode_header([
            ("op", _u8(OP_BAG_HEADER)),
            ("index_pos", _u64(index_pos)),
            ("conn_count", _u32(conn_count)),
            ("chunk_count", _u32(chunk_count)),
        ])
        # rosbag pads the bag-header record's DATA so the whole record is >=4096
        # bytes, leaving room to rewrite the counts in place.
        pad_len = 4096 - len(header) - 4
        if pad_len < 0:
            pad_len = 0
        return header + _u32(pad_len) + (b" " * pad_len)

    def _flush_chunk(self):
        if not self._chunk_buf:
            return
        chunk_pos = self.f.tell()
        # Build the uncompressed chunk data: connection records (first sight in
        # this chunk) interleaved with message-data records; record per-conn
        # offsets for the index.
        data = bytearray()
        seen = set()
        index = {}                       # conn_id -> [(secs, nsecs, offset)]
        counts = {}
        for cid, secs, nsecs, body in self._chunk_buf:
            c = self._conn_by_id[cid]
            if cid not in seen:
                seen.add(cid)
                data += self._record_bytes(
                    [("op", _u8(OP_CONNECTION)), ("conn", _u32(cid)),
                     ("topic", c.topic.encode("utf-8"))], c.header_data())
            offset = len(data)
            data += self._record_bytes(
                [("op", _u8(OP_MSG_DATA)), ("conn", _u32(cid)),
                 ("time", _time(secs, nsecs))], body)
            index.setdefault(cid, []).append((secs, nsecs, offset))
            counts[cid] = counts.get(cid, 0) + 1
        # Chunk record: compression=none, size=uncompressed length.
        self._write_record(
            [("op", _u8(OP_CHUNK)), ("compression", b"none"),
             ("size", _u32(len(data)))], bytes(data))
        # Index records: one per connection in this chunk.
        for cid, entries in index.items():
            idx = b"".join(_time(s, n) + _u32(off) for s, n, off in entries)
            self._write_record(
                [("op", _u8(OP_IDX_DATA)), ("ver", _u32(1)),
                 ("conn", _u32(cid)), ("count", _u32(len(entries)))], idx)
        # Remember chunk info for the file tail.
        starts = min((s, n) for cid, s, n, _o in
                     ((cid, s, n, o) for cid in index for (s, n, o) in index[cid]))
        ends = max((s, n) for cid in index for (s, n, o) in index[cid])
        self._chunk_infos.append((chunk_pos, starts, ends, counts))
        self._chunk_buf = []
        self._chunk_bytes = 0

    @staticmethod
    def _record_bytes(header_fields, data):
        return encode_header(header_fields) + _u32(len(data)) + data


# ------------------------------------------------------------------ reader ----
class BagMessage:
    __slots__ = ("topic", "conn", "secs", "nsecs", "data")

    def __init__(self, topic, conn, secs, nsecs, data):
        self.topic = topic
        self.conn = conn
        self.secs = secs
        self.nsecs = nsecs
        self.data = data

    @property
    def t(self):
        return self.secs + self.nsecs * 1e-9


class BagReader:
    """Read a v2.0 bag: iterate messages, or summarise it."""

    def __init__(self, path):
        self.path = path
        self.f = open(path, "rb")
        line = self.f.readline()
        if not line.startswith(b"#ROSBAG V2.0"):
            raise ValueError("%s is not a ROS bag v2.0 file" % path)
        self._conns = {}                 # id -> dict(topic,type,md5,definition)
        self._read_bag_header()

    def _read_record(self):
        raw = self.f.read(4)
        if len(raw) < 4:
            return None, None
        (hlen,) = struct.unpack("<I", raw)
        header = _decode_header_bytes(self.f.read(hlen))
        (dlen,) = struct.unpack("<I", self.f.read(4))
        data = self.f.read(dlen)
        return header, data

    def _read_bag_header(self):
        pos = self.f.tell()
        header, _data = self._read_record()
        self._conn_count = struct.unpack("<I", header["conn_count"])[0]
        self._chunk_count = struct.unpack("<I", header["chunk_count"])[0]
        self._index_pos = struct.unpack("<Q", header["index_pos"])[0]
        self._data_start = self.f.tell()
        self.f.seek(pos)                 # leave positioned at bag header

    def _parse_connection(self, header, data):
        cid = struct.unpack("<I", header["conn"])[0]
        fields = _decode_header_bytes(data)
        self._conns[cid] = {
            "topic": header.get("topic", fields.get("topic", b"")).decode("utf-8"),
            "type": fields.get("type", b"").decode("utf-8"),
            "md5": fields.get("md5sum", b"").decode("ascii"),
            "definition": fields.get("message_definition", b"").decode("utf-8", "replace"),
            "callerid": fields.get("callerid", b"").decode("utf-8", "replace"),
        }
        return cid

    def read_messages(self):
        """Yield BagMessage in file order (chunk by chunk, message by message)."""
        self.f.seek(self._data_start)
        while True:
            header, data = self._read_record()
            if header is None:
                break
            op = header["op"][0]
            if op == OP_CONNECTION:
                self._parse_connection(header, data)
            elif op == OP_CHUNK:
                yield from self._read_chunk(header, data)
            elif op in (OP_IDX_DATA, OP_CHUNK_INFO):
                pass                      # tail index; messages already yielded
            elif op == OP_MSG_DATA:       # (bags without chunks are legal)
                yield self._msg_from(header, data)
            # once we reach the file-level connection/index tail we could stop,
            # but reading to EOF is simplest and robust.

    def _read_chunk(self, header, data):
        comp = header.get("compression", b"none").decode("ascii")
        if comp == "bz2":
            data = bz2.decompress(data)
        elif comp == "lz4":
            try:
                import lz4.frame
                data = lz4.frame.decompress(data)
            except Exception as e:  # noqa
                raise ValueError("lz4-compressed bag needs the lz4 package: %s" % e)
        elif comp != "none":
            raise ValueError("unknown chunk compression %r" % comp)
        i = 0
        while i < len(data):
            (hlen,) = struct.unpack_from("<I", data, i)
            i += 4
            rhdr = _decode_header_bytes(data[i:i + hlen])
            i += hlen
            (dlen,) = struct.unpack_from("<I", data, i)
            i += 4
            rdata = data[i:i + dlen]
            i += dlen
            op = rhdr["op"][0]
            if op == OP_CONNECTION:
                self._parse_connection(rhdr, rdata)
            elif op == OP_MSG_DATA:
                yield self._msg_from(rhdr, rdata)

    def _msg_from(self, header, data):
        cid = struct.unpack("<I", header["conn"])[0]
        secs, nsecs = struct.unpack("<II", header["time"])
        topic = self._conns.get(cid, {}).get("topic", "?")
        return BagMessage(topic, cid, secs, nsecs, data)

    def connections(self):
        """Read the file-level connection records (fast; no chunk scan)."""
        if not self._conns:
            self.f.seek(self._index_pos)
            for _ in range(self._conn_count):
                header, data = self._read_record()
                if header is None:
                    break
                if header["op"][0] == OP_CONNECTION:
                    self._parse_connection(header, data)
        return self._conns

    def chunk_infos(self):
        """(chunk_pos, start, end, {conn: count}) for each chunk, from the tail."""
        self.connections()
        infos = []
        self.f.seek(self._index_pos)
        while True:
            header, data = self._read_record()
            if header is None:
                break
            if header["op"][0] != OP_CHUNK_INFO:
                continue
            start = struct.unpack("<II", header["start_time"])
            end = struct.unpack("<II", header["end_time"])
            chunk_pos = struct.unpack("<Q", header["chunk_pos"])[0]
            counts = {}
            for j in range(0, len(data), 8):
                cid, n = struct.unpack_from("<II", data, j)
                counts[cid] = n
            infos.append((chunk_pos, start, end, counts))
        return infos

    def close(self):
        if self.f:
            self.f.close()
            self.f = None
