"""nav_msgs msg types, importable the way ROS does it.

    from irap_noroslib.nav_msgs.msg import GridCells, MapMetaData

These are the very same classes as `irap_noroslib.msg.GridCells` -- just
re-exported under the familiar ROS package path.
"""
from ...msg import (
    GridCells,
    MapMetaData,
    OccupancyGrid,
    Odometry,
    Path,
)

__all__ = [
    "GridCells",
    "MapMetaData",
    "OccupancyGrid",
    "Odometry",
    "Path",
]
