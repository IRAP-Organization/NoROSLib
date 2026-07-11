// geometry_msgs.hpp — geometry_msgs message types (TYPE/MD5 match real ROS).
#pragma once
#include "noros/message.hpp"
#include "noros/std_msgs.hpp"

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
  std::vector<uint8_t> serialize() const { Writer w; linear.write(w); angular.write(w); return w.b; }
  static Twist deserialize(const std::vector<uint8_t>& b) {
    Reader r(b); Twist m; m.linear = Vector3::read(r); m.angular = Vector3::read(r); return m;
  }
};

struct Pose {
  static constexpr const char* TYPE = "geometry_msgs/Pose";
  static constexpr const char* MD5 = "e45d45a5a1ce597b249e23fb30fc871f";
  static constexpr const char* DEFINITION =
      "geometry_msgs/Point position\ngeometry_msgs/Quaternion orientation\n";
  Point position;
  Quaternion orientation;
  std::vector<uint8_t> serialize() const { Writer w; position.write(w); orientation.write(w); return w.b; }
  static Pose deserialize(const std::vector<uint8_t>& b) {
    Reader r(b); Pose m; m.position = Point::read(r); m.orientation = Quaternion::read(r); return m;
  }
};

struct PoseStamped {
  static constexpr const char* TYPE = "geometry_msgs/PoseStamped";
  static constexpr const char* MD5 = "d3812c3cbc69362b77dc0b19b345f8f5";
  static constexpr const char* DEFINITION =
      "std_msgs/Header header\ngeometry_msgs/Pose pose\n";
  std_msgs::Header header;
  Pose pose;
  std::vector<uint8_t> serialize() const {
    Writer w; header.write(w); pose.position.write(w); pose.orientation.write(w); return w.b;
  }
  static PoseStamped deserialize(const std::vector<uint8_t>& b) {
    Reader r(b); PoseStamped m; m.header = std_msgs::Header::read(r);
    m.pose.position = Point::read(r); m.pose.orientation = Quaternion::read(r); return m;
  }
};

}  // namespace geometry_msgs
