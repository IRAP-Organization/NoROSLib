"""geometry_msgs msg types, importable the way ROS does it.

    from irap_noroslib.geometry_msgs.msg import Accel, Point

These are the very same classes as `irap_noroslib.msg.Accel` -- just
re-exported under the familiar ROS package path.
"""
from ...msg import (
    Accel,
    Point,
    Point32,
    Polygon,
    Pose,
    PoseArray,
    PoseStamped,
    PoseWithCovariance,
    Quaternion,
    Transform,
    TransformStamped,
    Twist,
    TwistStamped,
    TwistWithCovariance,
    Vector3,
    Wrench,
)

__all__ = [
    "Accel",
    "Point",
    "Point32",
    "Polygon",
    "Pose",
    "PoseArray",
    "PoseStamped",
    "PoseWithCovariance",
    "Quaternion",
    "Transform",
    "TransformStamped",
    "Twist",
    "TwistStamped",
    "TwistWithCovariance",
    "Vector3",
    "Wrench",
]
