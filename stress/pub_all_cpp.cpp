// noros C++ stress publisher: advertise EVERY built-in catalog struct + the
// real custom message (noros_stress_msgs/CustomData), publish latched to a real
// roscore. A rospy verifier then decodes each with the genuine ROS class.
#include "noros.hpp"
#include <cstdlib>
#include <string>
#include <vector>

// ---- the custom message, matching the catkin-built noros_stress_msgs/CustomData
static const char* SEP =
    "================================================================================\n";
struct CustomData {
  static constexpr const char* TYPE = "noros_stress_msgs/CustomData";
  static constexpr const char* MD5 = "90f507711f5fc7a674b7527eafdf210d";
  static const std::string DEFINITION;  // defined out-of-line below
  std_msgs::Header header;
  int32_t id = 0;
  std::vector<double> samples;
  std::string label;
  std::vector<uint8_t> blob;
  geometry_msgs::Point location;
  bool valid = false;
  std::vector<uint8_t> serialize() const {
    noros::Writer w;
    header.write(w);
    w.i32(id);
    w.u32((uint32_t)samples.size());
    for (double s : samples) w.f64(s);
    w.str(label);
    w.u32((uint32_t)blob.size());
    for (uint8_t x : blob) w.u8(x);
    location.write(w);
    w.u8(valid ? 1 : 0);
    return w.b;
  }
  static CustomData deserialize(const std::vector<uint8_t>& b) {
    noros::Reader r(b);
    CustomData m;
    m.header = std_msgs::Header::read(r);
    m.id = r.i32();
    uint32_t n = r.u32();
    for (uint32_t i = 0; i < n; ++i) m.samples.push_back(r.f64());
    m.label = r.str();
    uint32_t bn = r.u32();
    for (uint32_t i = 0; i < bn; ++i) m.blob.push_back(r.u8());
    m.location = geometry_msgs::Point::read(r);
    m.valid = r.u8() != 0;
    return m;
  }
};
const std::string CustomData::DEFINITION =
    std::string("std_msgs/Header header\nint32 id\nfloat64[] samples\nstring label\n"
                "uint8[] blob\ngeometry_msgs/Point location\nbool valid\n") +
    SEP + "MSG: std_msgs/Header\nuint32 seq\ntime stamp\nstring frame_id\n" +
    SEP + "MSG: geometry_msgs/Point\nfloat64 x\nfloat64 y\nfloat64 z\n";

// Publish a default-constructed message of every type (proves md5/wire for all).
#define PUB_DEFAULT(NS, T)                                                  \
  {                                                                         \
    static noros::Publisher<NS::T> p("/stress/cpp/" #T, true);             \
    NS::T m;                                                                \
    p.publish(m);                                                           \
  }
// Publish a scalar-wrapper message with `.data` set to a sentinel value.
#define PUB_DATA(NS, T, VAL)                                                \
  {                                                                         \
    static noros::Publisher<NS::T> p("/stress/cpp/" #T, true);             \
    NS::T m;                                                                \
    m.data = (VAL);                                                         \
    p.publish(m);                                                           \
  }

int main() {
  const char* mu = std::getenv("ROS_MASTER_URI");
  const char* hn = std::getenv("ROS_HOSTNAME");
  noros::set_master_uri(mu ? mu : "http://localhost:11311");
  noros::set_hostname(hn ? hn : "localhost");
  noros::init_node("noros_stress_pub_cpp");

  // Rich, sentinel-populated publishers (spot-checked by the verifier).
  static noros::Publisher<std_msgs::String> p_str("/stress/cpp/String", true);
  static noros::Publisher<std_msgs::Int64> p_i64("/stress/cpp/Int64", true);
  static noros::Publisher<std_msgs::Int32> p_i32("/stress/cpp/Int32", true);
  static noros::Publisher<std_msgs::Float64> p_f64("/stress/cpp/Float64", true);
  static noros::Publisher<std_msgs::Float32> p_f32("/stress/cpp/Float32", true);
  static noros::Publisher<std_msgs::Bool> p_bool("/stress/cpp/Bool", true);
  static noros::Publisher<sensor_msgs::Imu> p_imu("/stress/cpp/Imu", true);
  static noros::Publisher<sensor_msgs::JointState> p_js("/stress/cpp/JointState", true);
  static noros::Publisher<nav_msgs::Odometry> p_odom("/stress/cpp/Odometry", true);
  static noros::Publisher<CustomData> p_custom("/stress/cpp/CustomData", true);

  noros::loginfo("C++ stress publisher advertising full catalog + custom");
  std::printf("PUB_READY\n"); std::fflush(stdout);

  noros::Rate rate(10);
  while (noros::ok()) {
    // -- rich ones --
    { std_msgs::String m; m.data = "noros"; p_str.publish(m); }
    { std_msgs::Int64 m; m.data = 7; p_i64.publish(m); }
    { std_msgs::Int32 m; m.data = 7; p_i32.publish(m); }
    { std_msgs::Float64 m; m.data = 1.5; p_f64.publish(m); }
    { std_msgs::Float32 m; m.data = 1.5f; p_f32.publish(m); }
    { std_msgs::Bool m; m.data = true; p_bool.publish(m); }
    { sensor_msgs::Imu m; m.orientation.x = 1.5; m.orientation.w = 1.5;
      m.angular_velocity.z = 1.5; m.linear_acceleration.x = 1.5; p_imu.publish(m); }
    { sensor_msgs::JointState m; m.name = {"noros"}; m.position = {1.5};
      m.header.frame_id = "noros"; p_js.publish(m); }
    { nav_msgs::Odometry m; m.child_frame_id = "noros"; m.header.frame_id = "noros";
      m.pose.pose.position.x = 1.5; p_odom.publish(m); }
    { CustomData m; m.header.frame_id = "cpp"; m.id = 7; m.samples = {1.5, 2.5, 3.5};
      m.label = "noros"; m.blob = {1, 2, 3}; m.location.x = 1.5; m.valid = true;
      p_custom.publish(m); }

    // -- every remaining catalog type, default-constructed (md5/wire proof) --
    PUB_DEFAULT(std_msgs, Header)
    PUB_DEFAULT(std_msgs, ColorRGBA)
    PUB_DATA(std_msgs, Int8, 7)
    PUB_DATA(std_msgs, Int16, 7)
    PUB_DATA(std_msgs, UInt8, 7)
    PUB_DATA(std_msgs, UInt16, 7)
    PUB_DATA(std_msgs, UInt32, 7)
    PUB_DATA(std_msgs, UInt64, 7)
    PUB_DATA(std_msgs, Byte, 7)
    PUB_DATA(std_msgs, Char, 7)
    PUB_DEFAULT(std_msgs, Empty)
    PUB_DEFAULT(std_msgs, Time)
    PUB_DEFAULT(std_msgs, Duration)
    PUB_DEFAULT(geometry_msgs, TwistStamped)
    PUB_DEFAULT(sensor_msgs, PointField)
    PUB_DEFAULT(geometry_msgs, Vector3)
    PUB_DEFAULT(geometry_msgs, Point)
    PUB_DEFAULT(geometry_msgs, Point32)
    PUB_DEFAULT(geometry_msgs, Quaternion)
    PUB_DEFAULT(geometry_msgs, Twist)
    PUB_DEFAULT(geometry_msgs, Accel)
    PUB_DEFAULT(geometry_msgs, Wrench)
    PUB_DEFAULT(geometry_msgs, Pose)
    PUB_DEFAULT(geometry_msgs, PoseStamped)
    PUB_DEFAULT(geometry_msgs, PoseArray)
    PUB_DEFAULT(geometry_msgs, Polygon)
    PUB_DEFAULT(geometry_msgs, Transform)
    PUB_DEFAULT(geometry_msgs, TransformStamped)
    PUB_DEFAULT(geometry_msgs, PoseWithCovariance)
    PUB_DEFAULT(geometry_msgs, TwistWithCovariance)
    PUB_DEFAULT(sensor_msgs, Image)
    PUB_DEFAULT(sensor_msgs, CompressedImage)
    PUB_DEFAULT(sensor_msgs, PointCloud2)
    PUB_DEFAULT(sensor_msgs, RegionOfInterest)
    PUB_DEFAULT(sensor_msgs, LaserScan)
    PUB_DEFAULT(sensor_msgs, NavSatStatus)
    PUB_DEFAULT(sensor_msgs, NavSatFix)
    PUB_DEFAULT(sensor_msgs, Range)
    PUB_DEFAULT(sensor_msgs, Temperature)
    PUB_DEFAULT(sensor_msgs, MagneticField)
    PUB_DEFAULT(sensor_msgs, CameraInfo)
    PUB_DEFAULT(nav_msgs, MapMetaData)
    PUB_DEFAULT(nav_msgs, Path)
    PUB_DEFAULT(nav_msgs, OccupancyGrid)
    PUB_DEFAULT(nav_msgs, GridCells)
    PUB_DEFAULT(diagnostic_msgs, KeyValue)
    PUB_DEFAULT(diagnostic_msgs, DiagnosticStatus)
    PUB_DEFAULT(diagnostic_msgs, DiagnosticArray)
    PUB_DEFAULT(trajectory_msgs, JointTrajectoryPoint)
    PUB_DEFAULT(trajectory_msgs, JointTrajectory)
    PUB_DEFAULT(trajectory_msgs, MultiDOFJointTrajectoryPoint)
    PUB_DEFAULT(trajectory_msgs, MultiDOFJointTrajectory)
    PUB_DEFAULT(actionlib_msgs, GoalID)
    PUB_DEFAULT(actionlib_msgs, GoalStatus)
    PUB_DEFAULT(actionlib_msgs, GoalStatusArray)
    rate.sleep();
  }
  return 0;
}
