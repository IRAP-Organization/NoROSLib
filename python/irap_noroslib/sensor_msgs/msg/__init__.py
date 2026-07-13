"""sensor_msgs msg types, importable the way ROS does it.

    from irap_noroslib.sensor_msgs.msg import CameraInfo, CompressedImage

These are the very same classes as `irap_noroslib.msg.CameraInfo` -- just
re-exported under the familiar ROS package path.
"""
from ...msg import (
    CameraInfo,
    CompressedImage,
    Image,
    Imu,
    JointState,
    LaserScan,
    MagneticField,
    NavSatFix,
    NavSatStatus,
    PointCloud2,
    PointField,
    Range,
    RegionOfInterest,
    Temperature,
)

__all__ = [
    "CameraInfo",
    "CompressedImage",
    "Image",
    "Imu",
    "JointState",
    "LaserScan",
    "MagneticField",
    "NavSatFix",
    "NavSatStatus",
    "PointCloud2",
    "PointField",
    "Range",
    "RegionOfInterest",
    "Temperature",
]
