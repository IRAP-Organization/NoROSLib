"""Parse ROS `.msg` text into a spec, compute its md5sum, and serialize/
deserialize any message generically from that spec.

This is what makes "add your own message" a one-liner: give noros the `.msg`
text and it derives the md5sum (via the exact ROS algorithm) and the on-wire
codec for you. No code generation, no ROS installed.
"""
import hashlib
import struct

from ._serialization import BufferReader, BufferWriter

# builtin scalar type -> (struct code, python default). uint8/char and their
# arrays get special-cased as raw bytes in the codec below.
_PRIMITIVE = {
    "bool": ("?", False),
    "int8": ("b", 0), "byte": ("b", 0),
    "uint8": ("B", 0), "char": ("B", 0),
    "int16": ("h", 0), "uint16": ("H", 0),
    "int32": ("i", 0), "uint32": ("I", 0),
    "int64": ("q", 0), "uint64": ("Q", 0),
    "float32": ("f", 0.0), "float64": ("d", 0.0),
    "string": (None, ""),
    "time": (None, (0, 0)), "duration": (None, (0, 0)),
}
# builtin type names as ROS normalizes them for md5 (byte->int8 etc. are NOT
# renamed for md5; only Header is special). Keep as-is.
_BUILTIN = set(_PRIMITIVE) | {"Header"}


class Field:
    __slots__ = ("base_type", "name", "is_array", "array_len", "is_constant",
                 "const_value")

    def __init__(self, base_type, name, is_array=False, array_len=None,
                 is_constant=False, const_value=None):
        self.base_type = base_type      # e.g. "float64", "geometry_msgs/Point"
        self.name = name
        self.is_array = is_array
        self.array_len = array_len      # int for fixed [N], None for variable []
        self.is_constant = is_constant
        self.const_value = const_value

    @property
    def is_builtin(self):
        return self.base_type in _BUILTIN


def _resolve_type(base_type, pkg_context):
    """Normalize a field type to its full 'pkg/Type' name for lookups.

    'Header' -> 'std_msgs/Header'; a bare 'Point' inside package 'geometry_msgs'
    -> 'geometry_msgs/Point'; anything already 'pkg/Type' or builtin is unchanged.
    """
    if base_type == "Header":
        return "std_msgs/Header"
    if base_type in _PRIMITIVE:
        return base_type
    if "/" in base_type:
        return base_type
    return "%s/%s" % (pkg_context, base_type) if pkg_context else base_type


def parse_msg_text(text):
    """Parse `.msg` text into a list of Field (constants and fields, in order)."""
    fields = []
    for raw in text.splitlines():
        line = raw.split("#", 1)[0].strip()
        if not line:
            continue
        # "type name" or "type name=value" (constant) or "type name = value"
        parts = line.split(None, 1)
        if len(parts) != 2:
            continue
        type_tok, rest = parts
        if "=" in rest and _is_constant_type(type_tok):
            name, value = rest.split("=", 1)
            fields.append(Field(type_tok, name.strip(), is_constant=True,
                                const_value=value.strip()))
            continue
        name = rest.strip()
        base, is_array, alen = _split_array(type_tok)
        fields.append(Field(base, name, is_array, alen))
    return fields


def _is_constant_type(type_tok):
    # constants are only defined for builtin non-array scalar types
    return type_tok in _PRIMITIVE and type_tok not in ("time", "duration")


def _split_array(type_tok):
    if type_tok.endswith("]"):
        base, _, br = type_tok.partition("[")
        inner = br[:-1]
        return base, True, (int(inner) if inner else None)
    return type_tok, False, None


class MsgSpec:
    """A parsed message type: name, fields, md5, and a generic codec."""

    def __init__(self, full_type, text, registry):
        self.type = full_type                       # "pkg/Type"
        self.pkg = full_type.split("/")[0] if "/" in full_type else ""
        self.text = text.strip("\n")
        self.registry = registry
        self.fields = parse_msg_text(text)
        self._md5 = None

    # -- md5sum (exact ROS gentools algorithm) ------------------------------
    def md5_text(self):
        """The pre-hash md5 text: constants first, then fields with complex
        types replaced by their md5. Hashing this gives the message md5; a
        service md5 hashes request.md5_text() + response.md5_text()."""
        consts, decls = [], []
        for f in self.fields:
            full = _resolve_type(f.base_type, self.pkg)
            if f.is_constant:
                consts.append("%s %s=%s" % (f.base_type, f.name, f.const_value))
            elif f.base_type in _PRIMITIVE:
                # true builtin scalar/array; Header is NOT primitive -> handled
                # below so a bare `Header` field is replaced by its md5 (the ROS
                # rule), exactly like the fully-qualified std_msgs/Header.
                decls.append("%s %s" % (self._array_suffix(f, f.base_type), f.name))
            else:
                sub_md5 = self.registry.get_spec(full).compute_md5()
                decls.append("%s %s" % (sub_md5, f.name))
        return "\n".join(consts + decls)

    def compute_md5(self):
        if self._md5 is None:
            self._md5 = hashlib.md5(self.md5_text().encode("utf-8")).hexdigest()
        return self._md5

    @staticmethod
    def _array_suffix(f, typename):
        if not f.is_array:
            return typename
        return "%s[%s]" % (typename, f.array_len if f.array_len is not None else "")

    # -- generic serialization ---------------------------------------------
    def serialize(self, msg, w=None):
        top = w is None
        if top:
            w = BufferWriter()
        for f in self.fields:
            if f.is_constant:
                continue
            val = getattr(msg, f.name)
            self._write_field(w, f, val)
        return w.getvalue() if top else None

    def deserialize(self, buf):
        r = buf if isinstance(buf, BufferReader) else BufferReader(buf)
        out = self.registry.new_instance(self.type)
        for f in self.fields:
            if f.is_constant:
                continue
            setattr(out, f.name, self._read_field(r, f))
        return out

    def _write_field(self, w, f, val):
        if f.is_array:
            self._write_array(w, f, val)
        else:
            self._write_scalar(w, f.base_type, val)

    def _read_field(self, r, f):
        if f.is_array:
            return self._read_array(r, f)
        return self._read_scalar(r, f.base_type)

    def _write_array(self, w, f, val):
        # uint8[]/char[] as raw bytes fast path
        if f.base_type in ("uint8", "char", "int8", "byte"):
            code = _PRIMITIVE[f.base_type][0]
            if f.base_type in ("uint8", "char"):
                if f.array_len is None:
                    w.raw_bytes(val)
                else:
                    w.buf += bytes(val)[:f.array_len].ljust(f.array_len, b"\x00")
                return
            # int8/byte typed array
            if f.array_len is None:
                w.pack("I", len(val))
            w.buf += struct.pack("<%d%s" % (len(val), code), *val)
            return
        if f.array_len is None:
            w.pack("I", len(val))
        for el in val:
            self._write_scalar(w, f.base_type, el)

    def _read_array(self, r, f):
        n = f.array_len if f.array_len is not None else r.scalar("I")
        if f.base_type in ("uint8", "char"):
            b = r.buf[r.off:r.off + n]
            r.off += n
            return bytes(b)
        return [self._read_scalar(r, f.base_type) for _ in range(n)]

    def _write_scalar(self, w, base_type, val):
        if base_type == "string":
            w.string(val)
        elif base_type in ("time", "duration"):
            secs, nsecs = val
            w.pack("II", secs & 0xFFFFFFFF, nsecs & 0xFFFFFFFF)
        elif base_type in _PRIMITIVE:
            w.pack(_PRIMITIVE[base_type][0], val)
        else:  # nested message
            full = _resolve_type(base_type, self.pkg)
            self.registry.get_spec(full).serialize(val, w)

    def _read_scalar(self, r, base_type):
        if base_type == "string":
            return r.string()
        if base_type in ("time", "duration"):
            return tuple(r.unpack("II"))
        if base_type in _PRIMITIVE:
            return r.scalar(_PRIMITIVE[base_type][0])
        full = _resolve_type(base_type, self.pkg)
        return self.registry.get_spec(full).deserialize(r)

    def default_value(self, f):
        if f.is_array:
            if f.base_type in ("uint8", "char") and f.array_len is None:
                return b""
            if f.array_len is None:
                return []
            if f.base_type in ("uint8", "char"):
                return b"\x00" * f.array_len
            d = self._scalar_default(f.base_type)
            return [d for _ in range(f.array_len)]
        return self._scalar_default(f.base_type)

    def _scalar_default(self, base_type):
        if base_type in _PRIMITIVE:
            return _PRIMITIVE[base_type][1]
        full = _resolve_type(base_type, self.pkg)
        return self.registry.new_instance(full)
