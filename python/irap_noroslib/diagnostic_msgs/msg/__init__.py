"""diagnostic_msgs msg types, importable the way ROS does it.

    from irap_noroslib.diagnostic_msgs.msg import DiagnosticArray, DiagnosticStatus

These are the very same classes as `irap_noroslib.msg.DiagnosticArray` -- just
re-exported under the familiar ROS package path.
"""
from ...msg import (
    DiagnosticArray,
    DiagnosticStatus,
    KeyValue,
)

__all__ = [
    "DiagnosticArray",
    "DiagnosticStatus",
    "KeyValue",
]
