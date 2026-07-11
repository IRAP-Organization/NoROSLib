"""Little-endian read/write primitives matching ROS wire serialization.

ROS serialization, in full: little-endian, no padding; a string or variable-length
array is prefixed with a uint32 length; a fixed-length array is just its elements
back to back; fields are written in `.msg` declaration order.
"""
import struct


class BufferWriter:
    """Append primitives to a growing bytearray (ROS little-endian layout)."""

    __slots__ = ("buf",)

    def __init__(self):
        self.buf = bytearray()

    def pack(self, fmt, *vals):
        self.buf += struct.pack("<" + fmt, *vals)

    def string(self, s):
        if isinstance(s, str):
            s = s.encode("utf-8")
        self.buf += struct.pack("<I", len(s))
        self.buf += s

    def raw_bytes(self, b):
        # uint8[] / char[] variable array: length prefix + raw bytes.
        if isinstance(b, str):
            b = b.encode("utf-8")
        self.buf += struct.pack("<I", len(b))
        self.buf += bytes(b)

    def getvalue(self):
        return bytes(self.buf)


class BufferReader:
    """Read primitives from a bytes buffer, tracking an offset."""

    __slots__ = ("buf", "off")

    def __init__(self, buf):
        self.buf = buf
        self.off = 0

    def unpack(self, fmt):
        fmt = "<" + fmt
        n = struct.calcsize(fmt)
        if self.off + n > len(self.buf):
            raise ValueError("short read: need %d at offset %d of %d"
                             % (n, self.off, len(self.buf)))
        vals = struct.unpack_from(fmt, self.buf, self.off)
        self.off += n
        return vals

    def scalar(self, fmt):
        return self.unpack(fmt)[0]

    def string(self):
        (n,) = self.unpack("I")
        if self.off + n > len(self.buf):
            raise ValueError("short string read")
        s = self.buf[self.off:self.off + n]
        self.off += n
        return s.decode("utf-8", "replace")

    def raw_bytes(self):
        (n,) = self.unpack("I")
        if self.off + n > len(self.buf):
            raise ValueError("short bytes read")
        b = bytes(self.buf[self.off:self.off + n])
        self.off += n
        return b
