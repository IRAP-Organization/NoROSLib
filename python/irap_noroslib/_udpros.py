"""ROS UDPROS transport: the 8-byte per-datagram header, DATA0/DATAN framing on
send, and in-order reassembly on receive.

All header integers are LITTLE-ENDIAN. A whole ROS message travels as
[4B LE ros-len][body] split across a DATA0 block + N-1 DATAN blocks.
Verified against roscpp transport_udp.
"""
import socket
import struct

OP_DATA0 = 0   # first (or only) datagram of a message
OP_DATAN = 1   # continuation datagram
OP_PING = 2
OP_ERR = 3

HEADER_SIZE = 8
DEFAULT_MAX_DATAGRAM = 1500


def encode_header(conn_id, op, message_id, block):
    return struct.pack("<IBBH", conn_id, op, message_id, block)


def decode_header(buf):
    if len(buf) < HEADER_SIZE:
        return None
    conn_id, op, message_id, block = struct.unpack_from("<IBBH", buf, 0)
    return conn_id, op, message_id, block


def send_stream(sock, dst, conn_id, message_id, stream, max_datagram_size):
    """Send `stream` (already [4B len][body]) to dst as DATA0/DATAN datagrams.
    message_id must be nonzero and identical for all blocks of this message."""
    if max_datagram_size <= HEADER_SIZE:
        max_datagram_size = DEFAULT_MAX_DATAGRAM
    max_payload = max_datagram_size - HEADER_SIZE
    total_blocks = 1 if not stream else (len(stream) + max_payload - 1) // max_payload
    off = 0
    for block in range(total_blocks):
        chunk = stream[off:off + max_payload]
        if block == 0:
            hdr = encode_header(conn_id, OP_DATA0, message_id, total_blocks)
        else:
            hdr = encode_header(conn_id, OP_DATAN, message_id, block)
        sock.sendto(hdr + chunk, dst)
        off += len(chunk)


class Receiver:
    """Reassembles UDPROS datagrams for one connection. roscpp streams strictly
    in order; we mirror that (reset on DATA0, require contiguous next block)."""

    def __init__(self):
        self._in_progress = False
        self._message_id = 0
        self._total_blocks = 0
        self._next_block = 0
        self._buf = bytearray()

    def feed(self, datagram):
        """Feed one datagram. Returns the raw ROS body (4B len prefix stripped)
        on completing a message, else None."""
        h = decode_header(datagram)
        if h is None:
            return None
        conn_id, op, message_id, block = h
        payload = datagram[HEADER_SIZE:]

        if op == OP_DATA0:
            self._in_progress = True
            self._message_id = message_id
            self._total_blocks = block if block else 1
            self._next_block = 1
            self._buf = bytearray(payload)
            if self._total_blocks <= 1:
                return self._complete()
            return None

        if op == OP_DATAN:
            if (not self._in_progress or message_id != self._message_id
                    or block != self._next_block):
                self._in_progress = False   # out of order / lost -> drop message
                return None
            self._buf += payload
            self._next_block += 1
            if self._next_block >= self._total_blocks:
                return self._complete()
            return None

        return None   # PING / ERR / unknown

    def _complete(self):
        self._in_progress = False
        if len(self._buf) < 4:
            return None
        (rlen,) = struct.unpack_from("<I", self._buf, 0)
        if rlen + 4 != len(self._buf):
            return None    # inconsistent; drop
        return bytes(self._buf[4:])


def resolve_ipv4(host, port):
    """(host, port) with host as numeric IP or hostname -> (ip, port) tuple."""
    try:
        socket.inet_aton(host)
        return host, port
    except OSError:
        return socket.gethostbyname(host), port
