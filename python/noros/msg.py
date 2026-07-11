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


# -- geometry_msgs (composite types used by nav_msgs / trajectory_msgs) ------
Transform = define_message("geometry_msgs/Transform", """
geometry_msgs/Vector3 translation
geometry_msgs/Quaternion rotation
""")
TransformStamped = define_message("geometry_msgs/TransformStamped", """
std_msgs/Header header
string child_frame_id
geometry_msgs/Transform transform
""")
Wrench = define_message("geometry_msgs/Wrench", """
geometry_msgs/Vector3 force
geometry_msgs/Vector3 torque
""")
Accel = define_message("geometry_msgs/Accel", """
geometry_msgs/Vector3 linear
geometry_msgs/Vector3 angular
""")
PoseWithCovariance = define_message("geometry_msgs/PoseWithCovariance", """
geometry_msgs/Pose pose
float64[36] covariance
""")
TwistWithCovariance = define_message("geometry_msgs/TwistWithCovariance", """
geometry_msgs/Twist twist
float64[36] covariance
""")
Polygon = define_message("geometry_msgs/Polygon", """
geometry_msgs/Point32[] points
""")
PoseArray = define_message("geometry_msgs/PoseArray", """
std_msgs/Header header
geometry_msgs/Pose[] poses
""")

# -- sensor_msgs (common sensor feeds) --------------------------------------
RegionOfInterest = define_message("sensor_msgs/RegionOfInterest", """
uint32 x_offset
uint32 y_offset
uint32 height
uint32 width
bool do_rectify
""")
Imu = define_message("sensor_msgs/Imu", """
std_msgs/Header header
geometry_msgs/Quaternion orientation
float64[9] orientation_covariance
geometry_msgs/Vector3 angular_velocity
float64[9] angular_velocity_covariance
geometry_msgs/Vector3 linear_acceleration
float64[9] linear_acceleration_covariance
""")
LaserScan = define_message("sensor_msgs/LaserScan", """
std_msgs/Header header
float32 angle_min
float32 angle_max
float32 angle_increment
float32 time_increment
float32 scan_time
float32 range_min
float32 range_max
float32[] ranges
float32[] intensities
""")
JointState = define_message("sensor_msgs/JointState", """
std_msgs/Header header
string[] name
float64[] position
float64[] velocity
float64[] effort
""")
NavSatStatus = define_message("sensor_msgs/NavSatStatus", """
int8 STATUS_NO_FIX=-1
int8 STATUS_FIX=0
int8 STATUS_SBAS_FIX=1
int8 STATUS_GBAS_FIX=2
int8 status
uint16 SERVICE_GPS=1
uint16 SERVICE_GLONASS=2
uint16 SERVICE_COMPASS=4
uint16 SERVICE_GALILEO=8
uint16 service
""")
NavSatFix = define_message("sensor_msgs/NavSatFix", """
std_msgs/Header header
sensor_msgs/NavSatStatus status
float64 latitude
float64 longitude
float64 altitude
float64[9] position_covariance
uint8 COVARIANCE_TYPE_UNKNOWN=0
uint8 COVARIANCE_TYPE_APPROXIMATED=1
uint8 COVARIANCE_TYPE_DIAGONAL_KNOWN=2
uint8 COVARIANCE_TYPE_KNOWN=3
uint8 position_covariance_type
""")
Range = define_message("sensor_msgs/Range", """
std_msgs/Header header
uint8 ULTRASOUND=0
uint8 INFRARED=1
uint8 radiation_type
float32 field_of_view
float32 min_range
float32 max_range
float32 range
""")
Temperature = define_message("sensor_msgs/Temperature", """
std_msgs/Header header
float64 temperature
float64 variance
""")
MagneticField = define_message("sensor_msgs/MagneticField", """
std_msgs/Header header
geometry_msgs/Vector3 magnetic_field
float64[9] magnetic_field_covariance
""")
CameraInfo = define_message("sensor_msgs/CameraInfo", """
std_msgs/Header header
uint32 height
uint32 width
string distortion_model
float64[] D
float64[9] K
float64[9] R
float64[12] P
uint32 binning_x
uint32 binning_y
sensor_msgs/RegionOfInterest roi
""")

# -- diagnostic_msgs --------------------------------------------------------
KeyValue = define_message("diagnostic_msgs/KeyValue", """
string key
string value
""")
DiagnosticStatus = define_message("diagnostic_msgs/DiagnosticStatus", """
byte OK=0
byte WARN=1
byte ERROR=2
byte STALE=3
byte level
string name
string message
string hardware_id
diagnostic_msgs/KeyValue[] values
""")
DiagnosticArray = define_message("diagnostic_msgs/DiagnosticArray", """
std_msgs/Header header
diagnostic_msgs/DiagnosticStatus[] status
""")

# -- nav_msgs ---------------------------------------------------------------
MapMetaData = define_message("nav_msgs/MapMetaData", """
time map_load_time
float32 resolution
uint32 width
uint32 height
geometry_msgs/Pose origin
""")
Odometry = define_message("nav_msgs/Odometry", """
std_msgs/Header header
string child_frame_id
geometry_msgs/PoseWithCovariance pose
geometry_msgs/TwistWithCovariance twist
""")
Path = define_message("nav_msgs/Path", """
std_msgs/Header header
geometry_msgs/PoseStamped[] poses
""")
OccupancyGrid = define_message("nav_msgs/OccupancyGrid", """
std_msgs/Header header
nav_msgs/MapMetaData info
int8[] data
""")
GridCells = define_message("nav_msgs/GridCells", """
std_msgs/Header header
float32 cell_width
float32 cell_height
geometry_msgs/Point[] cells
""")

# -- trajectory_msgs --------------------------------------------------------
JointTrajectoryPoint = define_message("trajectory_msgs/JointTrajectoryPoint", """
float64[] positions
float64[] velocities
float64[] accelerations
float64[] effort
duration time_from_start
""")
JointTrajectory = define_message("trajectory_msgs/JointTrajectory", """
std_msgs/Header header
string[] joint_names
trajectory_msgs/JointTrajectoryPoint[] points
""")
MultiDOFJointTrajectoryPoint = define_message("trajectory_msgs/MultiDOFJointTrajectoryPoint", """
geometry_msgs/Transform[] transforms
geometry_msgs/Twist[] velocities
geometry_msgs/Twist[] accelerations
duration time_from_start
""")
MultiDOFJointTrajectory = define_message("trajectory_msgs/MultiDOFJointTrajectory", """
std_msgs/Header header
string[] joint_names
trajectory_msgs/MultiDOFJointTrajectoryPoint[] points
""")


def get(full_type):
    """Return the registered class for 'pkg/Type', or None."""
    return get_message_class(full_type)
