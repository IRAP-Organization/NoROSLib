"""Deterministic, recursive populator for any noros message, driven by its spec.

Sentinels (checked by the real-ROS verifier):
  int/uint  -> 7        float32/64 -> 1.5      bool -> True
  string    -> "noros"  time/dur   -> (12345, 678)
  variable arrays       -> length 3            uint8[]/char[] -> b"\\x01\\x02\\x03"
Nested messages recurse. Fixed-size arrays keep their declared length.
"""
INT_SENTINEL = 7
FLOAT_SENTINEL = 1.5
STR_SENTINEL = "noros"
TIME_SENTINEL = (12345, 678)

_INTS = {"int8", "byte", "uint8", "char", "int16", "uint16",
         "int32", "uint32", "int64", "uint64"}


def _scalar(spec, base_type):
    if base_type == "string":
        return STR_SENTINEL
    if base_type in ("time", "duration"):
        return TIME_SENTINEL
    if base_type == "bool":
        return True
    if base_type in ("float32", "float64"):
        return FLOAT_SENTINEL
    if base_type in _INTS:
        return INT_SENTINEL
    # nested message
    from noros._msgspec import _resolve_type
    full = _resolve_type(base_type, spec.pkg)
    sub = spec.registry.get_spec(full)
    inst = spec.registry.new_instance(full)
    populate(inst, sub)
    return inst


def populate(msg, spec=None):
    """Fill every non-constant field of `msg` in place; returns msg."""
    if spec is None:
        spec = msg._spec
    for f in spec.fields:
        if f.is_constant:
            continue
        if f.is_array:
            if f.base_type in ("uint8", "char"):
                n = f.array_len if f.array_len is not None else 3
                setattr(msg, f.name, bytes(range(1, n + 1)))
            else:
                n = f.array_len if f.array_len is not None else 3
                setattr(msg, f.name, [_scalar(spec, f.base_type) for _ in range(n)])
        else:
            setattr(msg, f.name, _scalar(spec, f.base_type))
    return msg
