"""std_msgs msg types, importable the way ROS does it.

    from irap_noroslib.std_msgs.msg import Bool, Byte

These are the very same classes as `irap_noroslib.msg.Bool` -- just
re-exported under the familiar ROS package path.
"""
from ...msg import (
    Bool,
    Byte,
    Char,
    ColorRGBA,
    Duration,
    Empty,
    Float32,
    Float64,
    Header,
    Int16,
    Int32,
    Int64,
    Int8,
    String,
    Time,
    UInt16,
    UInt32,
    UInt64,
    UInt8,
)

__all__ = [
    "Bool",
    "Byte",
    "Char",
    "ColorRGBA",
    "Duration",
    "Empty",
    "Float32",
    "Float64",
    "Header",
    "Int16",
    "Int32",
    "Int64",
    "Int8",
    "String",
    "Time",
    "UInt16",
    "UInt32",
    "UInt64",
    "UInt8",
]
