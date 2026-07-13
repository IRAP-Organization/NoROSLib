"""TCPROS transport: connection-header encode/decode, message framing, and the
md5-mismatch error parsing that powers irap_noroslib's automatic md5 discovery.

See http://wiki.ros.org/ROS/TCPROS and ROS/Connection%20Header.
"""
import re
import socket
import struct

# "... but our version has [pkg/Type/<md5hex>]. Dropping connection."
_ERR_RE = re.compile(r"our version has \[([^\]]+)\]")


def encode_header(fields):
    """Encode a connection header: [4B len][repeated [4B len]['k=v']]."""
    body = bytearray()
    for k, v in fields.items():
        kv = ("%s=%s" % (k, v)).encode("utf-8")
        body += struct.pack("<I", len(kv)) + kv
    return struct.pack("<I", len(body)) + bytes(body)


def encode_header_fields(fields):
    """Encode ONLY the fields ([4B len]['k=v']...) with NO leading total length —
    the fields-only blob UDPROS requestTopic negotiation carries."""
    body = bytearray()
    for k, v in fields.items():
        kv = ("%s=%s" % (k, v)).encode("utf-8")
        body += struct.pack("<I", len(kv)) + kv
    return bytes(body)


def decode_header(blob):
    """Decode a header body (without the leading 4B total length) into a dict."""
    fields = {}
    off, n = 0, len(blob)
    while off + 4 <= n:
        (flen,) = struct.unpack_from("<I", blob, off)
        off += 4
        if off + flen > n:
            break
        kv = blob[off:off + flen].decode("utf-8", "replace")
        off += flen
        k, _, v = kv.partition("=")
        fields[k] = v
    return fields


def recv_exactly(sock, n):
    """Read exactly n bytes or raise ConnectionError on early close."""
    chunks = []
    got = 0
    while got < n:
        b = sock.recv(n - got)
        if not b:
            raise ConnectionError("peer closed after %d/%d bytes" % (got, n))
        chunks.append(b)
        got += len(b)
    return b"".join(chunks)


def read_header(sock):
    """Read a length-prefixed connection header from a socket -> dict."""
    (total,) = struct.unpack("<I", recv_exactly(sock, 4))
    if total == 0:
        return {}
    return decode_header(recv_exactly(sock, total))


def write_header(sock, fields):
    sock.sendall(encode_header(fields))


def frame_message(body):
    """A TCPROS message on the wire: [4B LE length][body]."""
    return struct.pack("<I", len(body)) + body


def read_message(sock):
    """Read one length-prefixed message body, or None on clean EOF."""
    hdr = b""
    while len(hdr) < 4:
        b = sock.recv(4 - len(hdr))
        if not b:
            return None
        hdr += b
    (n,) = struct.unpack("<I", hdr)
    return recv_exactly(sock, n) if n else b""


def frame_service_response(ok, body):
    """A service reply on the wire: [1B ok][4B LE length][body]."""
    return struct.pack("<BI", 1 if ok else 0, len(body)) + body


def read_service_response(sock):
    """Read a service reply -> (ok: bool, body: bytes)."""
    ok = recv_exactly(sock, 1)[0]
    (n,) = struct.unpack("<I", recv_exactly(sock, 4))
    body = recv_exactly(sock, n) if n else b""
    return bool(ok), body


def make_service_client_header(caller_id, service, md5sum, persistent=False):
    h = {"callerid": caller_id, "service": service, "md5sum": md5sum}
    if persistent:
        h["persistent"] = "1"
    return h


def make_service_server_header(caller_id, md5sum, service_type,
                               request_type, response_type):
    return {
        "callerid": caller_id,
        "md5sum": md5sum,
        "type": service_type,
        "request_type": request_type,
        "response_type": response_type,
    }


def parse_type_md5_from_error(error_text):
    """From a publisher's md5-mismatch error, extract its REAL (type, md5).

    Matches the '... our version has [pkg/Type/<md5>]. Dropping connection.'
    bracket. Returns (type, md5) or (None, None).
    """
    m = _ERR_RE.search(error_text or "")
    if not m:
        return None, None
    inside = m.group(1)              # "pkg/Type/<md5hex>"
    idx = inside.rfind("/")
    if idx < 0:
        return None, None
    return inside[:idx], inside[idx + 1:]


def make_subscriber_header(caller_id, topic, md5sum, type_name, tcp_nodelay=True):
    h = {
        "callerid": caller_id,
        "topic": topic,
        "md5sum": md5sum,
        "type": type_name,
    }
    if tcp_nodelay:
        h["tcp_nodelay"] = "1"
    return h


def make_publisher_header(caller_id, topic, md5sum, type_name,
                          message_definition="", latching=False):
    return {
        "callerid": caller_id,
        "topic": topic,
        "md5sum": md5sum,
        "type": type_name,
        "message_definition": message_definition,
        "latching": "1" if latching else "0",
    }


def set_nodelay(sock):
    try:
        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    except OSError:
        pass
