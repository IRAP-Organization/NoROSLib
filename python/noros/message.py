"""Message types for noros.

A message type is nothing but its ROS `.msg` text. Register it and noros derives
everything else -- the md5sum (via the exact ROS algorithm), the on-wire codec,
and the concatenated message-definition string ROS puts in the connection header.

Define your own type in one call::

    from noros.message import define_message
    Pose2D = define_message("my_pkg/Pose2D", '''
        float64 x
        float64 y
        float64 theta
    ''')
    m = Pose2D(x=1.0, y=2.0)
    m.md5sum()          # -> computed automatically

Types may nest other registered types by name (e.g. a field
``geometry_msgs/Point position``) -- register the dependency first.
"""
from ._msgspec import MsgSpec, _resolve_type

_SEP = "=" * 80


class _Registry:
    """Maps 'pkg/Type' -> (MsgSpec, Message subclass). Global by default."""

    def __init__(self):
        self._specs = {}
        self._classes = {}

    def register(self, full_type, text):
        spec = MsgSpec(full_type, text, self)
        self._specs[full_type] = spec
        cls = _make_class(full_type, spec)
        self._classes[full_type] = cls
        return cls

    def get_spec(self, full_type):
        if full_type not in self._specs:
            raise KeyError("unknown message type %r (register it first)" % full_type)
        return self._specs[full_type]

    def get_class(self, full_type):
        return self._classes.get(full_type)

    def has(self, full_type):
        return full_type in self._specs

    def new_instance(self, full_type):
        return self._classes[full_type]()

    def full_definition(self, full_type):
        """The connection-header 'message_definition': the type's own text
        followed by every dependency's text, each behind a MSG: separator."""
        main = self.get_spec(full_type)
        blocks = [main.text]
        for dep in self._ordered_deps(full_type):
            blocks.append("%s\nMSG: %s\n%s" % (_SEP, dep, self.get_spec(dep).text))
        return "\n".join(blocks) + "\n"

    def _ordered_deps(self, full_type):
        seen, order = set(), []

        def walk(t):
            spec = self._specs.get(t)
            if spec is None:
                return
            for f in spec.fields:
                if f.is_constant or f.is_builtin:
                    continue
                dep = _resolve_type(f.base_type, spec.pkg)
                if dep not in seen:
                    seen.add(dep)
                    order.append(dep)
                    walk(dep)
        walk(full_type)
        return order


registry = _Registry()


class Message(object):
    """Base for all message instances. Subclasses are built by define_message.

    Set fields via keyword args or attribute assignment::

        m = String(data="hi")
        m.data = "bye"
    """
    _type = None
    _spec = None

    def __init__(self, **kw):
        for f in self._spec.fields:
            if f.is_constant:
                continue
            setattr(self, f.name, kw.pop(f.name) if f.name in kw
                    else self._spec.default_value(f))
        if kw:
            raise TypeError("%s got unexpected field(s): %s"
                            % (self._type, ", ".join(kw)))

    # class-level metadata (ROS-compatible names) --------------------------
    @classmethod
    def md5sum(cls):
        return cls._spec.compute_md5()

    @classmethod
    def data_type(cls):
        return cls._type

    @classmethod
    def message_definition(cls):
        return registry.full_definition(cls._type)

    # wire codec -----------------------------------------------------------
    def serialize(self):
        return self._spec.serialize(self)

    @classmethod
    def deserialize(cls, buf):
        return cls._spec.deserialize(buf)

    def __repr__(self):
        parts = []
        for f in self._spec.fields:
            if f.is_constant:
                continue
            v = getattr(self, f.name)
            if isinstance(v, (bytes, bytearray)) and len(v) > 32:
                v = "<%d bytes>" % len(v)
            parts.append("%s=%r" % (f.name, v))
        return "%s(%s)" % (self._type.split("/")[-1], ", ".join(parts))

    def __eq__(self, other):
        if not isinstance(other, Message) or other._type != self._type:
            return NotImplemented
        return all(getattr(self, f.name) == getattr(other, f.name)
                   for f in self._spec.fields if not f.is_constant)


def _make_class(full_type, spec):
    name = full_type.split("/")[-1]
    attrs = {"_type": full_type, "_spec": spec}
    # expose constants as class attributes, ROS-style
    for f in spec.fields:
        if f.is_constant:
            attrs[f.name] = _coerce_const(f)
    return type(str(name), (Message,), attrs)


def _coerce_const(f):
    v = f.const_value
    if f.base_type in ("float32", "float64"):
        return float(v)
    if f.base_type == "string":
        return v
    if f.base_type == "bool":
        return bool(int(v))
    try:
        return int(v)
    except ValueError:
        return v


def define_message(full_type, text):
    """Register a message type from its `.msg` text and return the class.

    Idempotent: re-registering the same type returns the existing class.
    """
    if "/" not in full_type:
        raise ValueError("message type must be 'pkg/Type', got %r" % full_type)
    return registry.register(full_type, text)


def get_message_class(full_type):
    """Look up a previously registered message class, or None."""
    return registry.get_class(full_type)
