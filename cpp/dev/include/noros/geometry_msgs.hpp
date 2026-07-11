// geometry_msgs.hpp — geometry_msgs message types (TYPE/MD5 match real ROS).
#pragma once
#include "noros/message.hpp"
#include "noros/std_msgs.hpp"

#include <array>

namespace geometry_msgs {
using noros::Reader;
using noros::Writer;

struct Vector3 {
  static constexpr const char* TYPE = "geometry_msgs/Vector3";
  static constexpr const char* MD5 = "4a842b65f413084dc2b10fb484ea7f17";
  static constexpr const char* DEFINITION = "float64 x\nfloat64 y\nfloat64 z\n";
  double x = 0, y = 0, z = 0;
  void write(Writer& w) const { w.f64(x); w.f64(y); w.f64(z); }
  static Vector3 read(Reader& r) { Vector3 v; v.x = r.f64(); v.y = r.f64(); v.z = r.f64(); return v; }
  std::vector<uint8_t> serialize() const { Writer w; write(w); return w.b; }
  static Vector3 deserialize(const std::vector<uint8_t>& b) { Reader r(b); return read(r); }
};

struct Point {
  static constexpr const char* TYPE = "geometry_msgs/Point";
  static constexpr const char* MD5 = "4a842b65f413084dc2b10fb484ea7f17";
  static constexpr const char* DEFINITION = "float64 x\nfloat64 y\nfloat64 z\n";
  double x = 0, y = 0, z = 0;
  void write(Writer& w) const { w.f64(x); w.f64(y); w.f64(z); }
  static Point read(Reader& r) { Point v; v.x = r.f64(); v.y = r.f64(); v.z = r.f64(); return v; }
  std::vector<uint8_t> serialize() const { Writer w; write(w); return w.b; }
  static Point deserialize(const std::vector<uint8_t>& b) { Reader r(b); return read(r); }
};

struct Point32 {
  static constexpr const char* TYPE = "geometry_msgs/Point32";
  static constexpr const char* MD5 = "cc153912f1453b708d221682bc23d9ac";
  static constexpr const char* DEFINITION = "float32 x\nfloat32 y\nfloat32 z\n";
  float x = 0, y = 0, z = 0;
  void write(Writer& w) const { w.f32(x); w.f32(y); w.f32(z); }
  static Point32 read(Reader& r) { Point32 v; v.x = r.f32(); v.y = r.f32(); v.z = r.f32(); return v; }
  std::vector<uint8_t> serialize() const { Writer w; write(w); return w.b; }
  static Point32 deserialize(const std::vector<uint8_t>& b) { Reader r(b); return read(r); }
};

struct Quaternion {
  static constexpr const char* TYPE = "geometry_msgs/Quaternion";
  static constexpr const char* MD5 = "a779879fadf0160734f906b8c19c7004";
  static constexpr const char* DEFINITION = "float64 x\nfloat64 y\nfloat64 z\nfloat64 w\n";
  double x = 0, y = 0, z = 0, w = 1;
  void write(Writer& wr) const { wr.f64(x); wr.f64(y); wr.f64(z); wr.f64(w); }
  static Quaternion read(Reader& r) { Quaternion q; q.x = r.f64(); q.y = r.f64(); q.z = r.f64(); q.w = r.f64(); return q; }
  std::vector<uint8_t> serialize() const { Writer wr; write(wr); return wr.b; }
  static Quaternion deserialize(const std::vector<uint8_t>& b) { Reader r(b); return read(r); }
};

struct Twist {
  static constexpr const char* TYPE = "geometry_msgs/Twist";
  static constexpr const char* MD5 = "9f195f881246fdfa2798d1d3eebca84a";
  static constexpr const char* DEFINITION =
      "geometry_msgs/Vector3 linear\ngeometry_msgs/Vector3 angular\n";
  Vector3 linear, angular;
  void write(Writer& w) const { linear.write(w); angular.write(w); }
  static Twist read(Reader& r) { Twist m; m.linear = Vector3::read(r); m.angular = Vector3::read(r); return m; }
  std::vector<uint8_t> serialize() const { Writer w; write(w); return w.b; }
  static Twist deserialize(const std::vector<uint8_t>& b) { Reader r(b); return read(r); }
};

struct Accel {
  static constexpr const char* TYPE = "geometry_msgs/Accel";
  static constexpr const char* MD5 = "9f195f881246fdfa2798d1d3eebca84a";
  static constexpr const char* DEFINITION =
      "geometry_msgs/Vector3 linear\ngeometry_msgs/Vector3 angular\n";
  Vector3 linear, angular;
  void write(Writer& w) const { linear.write(w); angular.write(w); }
  static Accel read(Reader& r) { Accel m; m.linear = Vector3::read(r); m.angular = Vector3::read(r); return m; }
  std::vector<uint8_t> serialize() const { Writer w; write(w); return w.b; }
  static Accel deserialize(const std::vector<uint8_t>& b) { Reader r(b); return read(r); }
};

struct Wrench {
  static constexpr const char* TYPE = "geometry_msgs/Wrench";
  static constexpr const char* MD5 = "4f539cf138b23283b520fd271b567936";
  static constexpr const char* DEFINITION =
      "geometry_msgs/Vector3 force\ngeometry_msgs/Vector3 torque\n";
  Vector3 force, torque;
  void write(Writer& w) const { force.write(w); torque.write(w); }
  static Wrench read(Reader& r) { Wrench m; m.force = Vector3::read(r); m.torque = Vector3::read(r); return m; }
  std::vector<uint8_t> serialize() const { Writer w; write(w); return w.b; }
  static Wrench deserialize(const std::vector<uint8_t>& b) { Reader r(b); return read(r); }
};

struct Pose {
  static constexpr const char* TYPE = "geometry_msgs/Pose";
  static constexpr const char* MD5 = "e45d45a5a1ce597b249e23fb30fc871f";
  static constexpr const char* DEFINITION =
      "geometry_msgs/Point position\ngeometry_msgs/Quaternion orientation\n";
  Point position;
  Quaternion orientation;
  void write(Writer& w) const { position.write(w); orientation.write(w); }
  static Pose read(Reader& r) { Pose m; m.position = Point::read(r); m.orientation = Quaternion::read(r); return m; }
  std::vector<uint8_t> serialize() const { Writer w; write(w); return w.b; }
  static Pose deserialize(const std::vector<uint8_t>& b) { Reader r(b); return read(r); }
};

struct PoseStamped {
  static constexpr const char* TYPE = "geometry_msgs/PoseStamped";
  static constexpr const char* MD5 = "d3812c3cbc69362b77dc0b19b345f8f5";
  static constexpr const char* DEFINITION =
      "std_msgs/Header header\ngeometry_msgs/Pose pose\n";
  std_msgs::Header header;
  Pose pose;
  void write(Writer& w) const { header.write(w); pose.write(w); }
  static PoseStamped read(Reader& r) { PoseStamped m; m.header = std_msgs::Header::read(r); m.pose = Pose::read(r); return m; }
  std::vector<uint8_t> serialize() const { Writer w; write(w); return w.b; }
  static PoseStamped deserialize(const std::vector<uint8_t>& b) { Reader r(b); return read(r); }
};

struct TwistStamped {
  static constexpr const char* TYPE = "geometry_msgs/TwistStamped";
  static constexpr const char* MD5 = "98d34b0043a2093cf9d9345ab6eef12e";
  static constexpr const char* DEFINITION =
      "std_msgs/Header header\ngeometry_msgs/Twist twist\n";
  std_msgs::Header header;
  Twist twist;
  void write(Writer& w) const { header.write(w); twist.write(w); }
  static TwistStamped read(Reader& r) { TwistStamped m; m.header = std_msgs::Header::read(r); m.twist = Twist::read(r); return m; }
  std::vector<uint8_t> serialize() const { Writer w; write(w); return w.b; }
  static TwistStamped deserialize(const std::vector<uint8_t>& b) { Reader r(b); return read(r); }
};

struct PoseArray {
  static constexpr const char* TYPE = "geometry_msgs/PoseArray";
  static constexpr const char* MD5 = "916c28c5764443f268b296bb671b9d97";
  static constexpr const char* DEFINITION =
      "std_msgs/Header header\ngeometry_msgs/Pose[] poses\n";
  std_msgs::Header header;
  std::vector<Pose> poses;
  void write(Writer& w) const { header.write(w); w.u32((uint32_t)poses.size()); for (auto& p : poses) p.write(w); }
  static PoseArray read(Reader& r) {
    PoseArray m; m.header = std_msgs::Header::read(r);
    uint32_t n = r.u32(); m.poses.resize(n); for (auto& p : m.poses) p = Pose::read(r); return m;
  }
  std::vector<uint8_t> serialize() const { Writer w; write(w); return w.b; }
  static PoseArray deserialize(const std::vector<uint8_t>& b) { Reader r(b); return read(r); }
};

struct Polygon {
  static constexpr const char* TYPE = "geometry_msgs/Polygon";
  static constexpr const char* MD5 = "cd60a26494a087f577976f0329fa120e";
  static constexpr const char* DEFINITION = "geometry_msgs/Point32[] points\n";
  std::vector<Point32> points;
  void write(Writer& w) const { w.u32((uint32_t)points.size()); for (auto& p : points) p.write(w); }
  static Polygon read(Reader& r) { Polygon m; uint32_t n = r.u32(); m.points.resize(n); for (auto& p : m.points) p = Point32::read(r); return m; }
  std::vector<uint8_t> serialize() const { Writer w; write(w); return w.b; }
  static Polygon deserialize(const std::vector<uint8_t>& b) { Reader r(b); return read(r); }
};

struct Transform {
  static constexpr const char* TYPE = "geometry_msgs/Transform";
  static constexpr const char* MD5 = "ac9eff44abf714214112b05d54a3cf9b";
  static constexpr const char* DEFINITION =
      "geometry_msgs/Vector3 translation\ngeometry_msgs/Quaternion rotation\n";
  Vector3 translation;
  Quaternion rotation;
  void write(Writer& w) const { translation.write(w); rotation.write(w); }
  static Transform read(Reader& r) { Transform m; m.translation = Vector3::read(r); m.rotation = Quaternion::read(r); return m; }
  std::vector<uint8_t> serialize() const { Writer w; write(w); return w.b; }
  static Transform deserialize(const std::vector<uint8_t>& b) { Reader r(b); return read(r); }
};

struct TransformStamped {
  static constexpr const char* TYPE = "geometry_msgs/TransformStamped";
  static constexpr const char* MD5 = "b5764a33bfeb3588febc2682852579b0";
  static constexpr const char* DEFINITION =
      "std_msgs/Header header\nstring child_frame_id\ngeometry_msgs/Transform transform\n";
  std_msgs::Header header;
  std::string child_frame_id;
  Transform transform;
  void write(Writer& w) const { header.write(w); w.str(child_frame_id); transform.write(w); }
  static TransformStamped read(Reader& r) {
    TransformStamped m; m.header = std_msgs::Header::read(r);
    m.child_frame_id = r.str(); m.transform = Transform::read(r); return m;
  }
  std::vector<uint8_t> serialize() const { Writer w; write(w); return w.b; }
  static TransformStamped deserialize(const std::vector<uint8_t>& b) { Reader r(b); return read(r); }
};

struct PoseWithCovariance {
  static constexpr const char* TYPE = "geometry_msgs/PoseWithCovariance";
  static constexpr const char* MD5 = "c23e848cf1b7533a8d7c259073a97e6f";
  static constexpr const char* DEFINITION =
      "geometry_msgs/Pose pose\nfloat64[36] covariance\n";
  Pose pose;
  std::array<double, 36> covariance{};
  void write(Writer& w) const { pose.write(w); for (double v : covariance) w.f64(v); }
  static PoseWithCovariance read(Reader& r) {
    PoseWithCovariance m; m.pose = Pose::read(r); for (auto& v : m.covariance) v = r.f64(); return m;
  }
  std::vector<uint8_t> serialize() const { Writer w; write(w); return w.b; }
  static PoseWithCovariance deserialize(const std::vector<uint8_t>& b) { Reader r(b); return read(r); }
};

struct TwistWithCovariance {
  static constexpr const char* TYPE = "geometry_msgs/TwistWithCovariance";
  static constexpr const char* MD5 = "1fe8a28e6890a4cc3ae4c3ca5c7d82e6";
  static constexpr const char* DEFINITION =
      "geometry_msgs/Twist twist\nfloat64[36] covariance\n";
  Twist twist;
  std::array<double, 36> covariance{};
  void write(Writer& w) const { twist.write(w); for (double v : covariance) w.f64(v); }
  static TwistWithCovariance read(Reader& r) {
    TwistWithCovariance m; m.twist = Twist::read(r); for (auto& v : m.covariance) v = r.f64(); return m;
  }
  std::vector<uint8_t> serialize() const { Writer w; write(w); return w.b; }
  static TwistWithCovariance deserialize(const std::vector<uint8_t>& b) { Reader r(b); return read(r); }
};

}  // namespace geometry_msgs
