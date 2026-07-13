"""std_srvs srv types, importable the way ROS does it.

    from irap_noroslib.std_srvs.srv import Empty, SetBool

These are the very same classes as `irap_noroslib.srv.Empty` -- just
re-exported under the familiar ROS package path.
"""
from ...srv import (
    Empty,
    SetBool,
    Trigger,
)

__all__ = [
    "Empty",
    "SetBool",
    "Trigger",
]
