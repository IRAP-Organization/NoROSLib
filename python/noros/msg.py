"""Built-in ROS message types, defined the same way a user defines their own:
by registering `.msg` text. Every md5sum here is derived by noros and matches
`rosmsg md5 <type>` exactly, so these interoperate with real ROS nodes.

    from noros.msg import String, Twist
    from noros import msg
    msg.String                       # attribute access
    msg.get("nav_msgs/Odometry")     # or by full name (if registered)
"""
from .message import define_message, get_message_class

# -- std_msgs ---------------------------------------------------------------
Header = define_message("std_msgs/Header", """
uint32 seq
time stamp
string frame_id
""")

Bool = define_message("std_msgs/Bool", "bool data\n")
Byte = define_message("std_msgs/Byte", "byte data\n")
Char = define_message("std_msgs/Char", "char data\n")
Int8 = define_message("std_msgs/Int8", "int8 data\n")
UInt8 = define_message("std_msgs/UInt8", "uint8 data\n")
Int16 = define_message("std_msgs/Int16", "int16 data\n")
UInt16 = define_message("std_msgs/UInt16", "uint16 data\n")
Int32 = define_message("std_msgs/Int32", "int32 data\n")
UInt32 = define_message("std_msgs/UInt32", "uint32 data\n")
Int64 = define_message("std_msgs/Int64", "int64 data\n")
UInt64 = define_message("std_msgs/UInt64", "uint64 data\n")
Float32 = define_message("std_msgs/Float32", "float32 data\n")
Float64 = define_message("std_msgs/Float64", "float64 data\n")
String = define_message("std_msgs/String", "string data\n")
Empty = define_message("std_msgs/Empty", "\n")
Time = define_message("std_msgs/Time", "time data\n")
Duration = define_message("std_msgs/Duration", "duration data\n")
ColorRGBA = define_message("std_msgs/ColorRGBA", """
float32 r
float32 g
float32 b
float32 a
""")

# -- geometry_msgs ----------------------------------------------------------
Vector3 = define_message("geometry_msgs/Vector3", """
float64 x
float64 y
float64 z
""")
Point = define_message("geometry_msgs/Point", """
float64 x
float64 y
float64 z
""")
Point32 = define_message("geometry_msgs/Point32", """
float32 x
float32 y
float32 z
""")
Quaternion = define_message("geometry_msgs/Quaternion", """
float64 x
float64 y
float64 z
float64 w
""")
Pose = define_message("geometry_msgs/Pose", """
geometry_msgs/Point position
geometry_msgs/Quaternion orientation
""")
PoseStamped = define_message("geometry_msgs/PoseStamped", """
std_msgs/Header header
geometry_msgs/Pose pose
""")
Twist = define_message("geometry_msgs/Twist", """
geometry_msgs/Vector3 linear
geometry_msgs/Vector3 angular
""")
TwistStamped = define_message("geometry_msgs/TwistStamped", """
std_msgs/Header header
geometry_msgs/Twist twist
""")

# -- sensor_msgs ------------------------------------------------------------
PointField = define_message("sensor_msgs/PointField", """
uint8 INT8=1
uint8 UINT8=2
uint8 INT16=3
uint8 UINT16=4
uint8 INT32=5
uint8 UINT32=6
uint8 FLOAT32=7
uint8 FLOAT64=8
string name
uint32 offset
uint8 datatype
uint32 count
""")
Image = define_message("sensor_msgs/Image", """
std_msgs/Header header
uint32 height
uint32 width
string encoding
uint8 is_bigendian
uint32 step
uint8[] data
""")
CompressedImage = define_message("sensor_msgs/CompressedImage", """
std_msgs/Header header
string format
uint8[] data
""")
PointCloud2 = define_message("sensor_msgs/PointCloud2", """
std_msgs/Header header
uint32 height
uint32 width
sensor_msgs/PointField[] fields
bool is_bigendian
uint32 point_step
uint32 row_step
uint8[] data
bool is_dense
""")


# -- actionlib_msgs (needed by the action layer) ----------------------------
GoalID = define_message("actionlib_msgs/GoalID", """
time stamp
string id
""")
GoalStatus = define_message("actionlib_msgs/GoalStatus", """
actionlib_msgs/GoalID goal_id
uint8 status
uint8 PENDING=0
uint8 ACTIVE=1
uint8 PREEMPTED=2
uint8 SUCCEEDED=3
uint8 ABORTED=4
uint8 REJECTED=5
uint8 PREEMPTING=6
uint8 RECALLING=7
uint8 RECALLED=8
uint8 LOST=9
string text
""")
GoalStatusArray = define_message("actionlib_msgs/GoalStatusArray", """
std_msgs/Header header
actionlib_msgs/GoalStatus[] status_list
""")


def get(full_type):
    """Return the registered class for 'pkg/Type', or None."""
    return get_message_class(full_type)
