"""actionlib_msgs msg types, importable the way ROS does it.

    from irap_noroslib.actionlib_msgs.msg import GoalID, GoalStatus

These are the very same classes as `irap_noroslib.msg.GoalID` -- just
re-exported under the familiar ROS package path.
"""
from ...msg import (
    GoalID,
    GoalStatus,
    GoalStatusArray,
)

__all__ = [
    "GoalID",
    "GoalStatus",
    "GoalStatusArray",
]
