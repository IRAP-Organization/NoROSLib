"""trajectory_msgs msg types, importable the way ROS does it.

    from irap_noroslib.trajectory_msgs.msg import JointTrajectory, JointTrajectoryPoint

These are the very same classes as `irap_noroslib.msg.JointTrajectory` -- just
re-exported under the familiar ROS package path.
"""
from ...msg import (
    JointTrajectory,
    JointTrajectoryPoint,
    MultiDOFJointTrajectory,
    MultiDOFJointTrajectoryPoint,
)

__all__ = [
    "JointTrajectory",
    "JointTrajectoryPoint",
    "MultiDOFJointTrajectory",
    "MultiDOFJointTrajectoryPoint",
]
