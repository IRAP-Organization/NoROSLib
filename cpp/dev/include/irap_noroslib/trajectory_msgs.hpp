// trajectory_msgs.hpp — trajectory_msgs message types (TYPE/MD5 match real ROS).
#pragma once
#include "irap_noroslib/message.hpp"
#include "irap_noroslib/std_msgs.hpp"
#include "irap_noroslib/geometry_msgs.hpp"

namespace trajectory_msgs {
using irap_noroslib::Reader;
using irap_noroslib::Writer;

struct JointTrajectoryPoint {
  static constexpr const char* TYPE = "trajectory_msgs/JointTrajectoryPoint";
  static constexpr const char* MD5 = "f3cd1e1c4d320c79d6985c904ae5dcd3";
  static constexpr const char* DEFINITION =
      "float64[] positions\nfloat64[] velocities\nfloat64[] accelerations\nfloat64[] effort\n"
      "duration time_from_start\n";
  std::vector<double> positions, velocities, accelerations, effort;
  int32_t time_from_start_sec = 0, time_from_start_nsec = 0;   // ROS duration
  void write(Writer& w) const {
    w.u32((uint32_t)positions.size()); for (double v : positions) w.f64(v);
    w.u32((uint32_t)velocities.size()); for (double v : velocities) w.f64(v);
    w.u32((uint32_t)accelerations.size()); for (double v : accelerations) w.f64(v);
    w.u32((uint32_t)effort.size()); for (double v : effort) w.f64(v);
    w.i32(time_from_start_sec); w.i32(time_from_start_nsec);
  }
  static JointTrajectoryPoint read(Reader& r) {
    JointTrajectoryPoint m;
    uint32_t n = r.u32(); m.positions.resize(n); for (auto& v : m.positions) v = r.f64();
    n = r.u32(); m.velocities.resize(n); for (auto& v : m.velocities) v = r.f64();
    n = r.u32(); m.accelerations.resize(n); for (auto& v : m.accelerations) v = r.f64();
    n = r.u32(); m.effort.resize(n); for (auto& v : m.effort) v = r.f64();
    m.time_from_start_sec = r.i32(); m.time_from_start_nsec = r.i32();
    return m;
  }
  std::vector<uint8_t> serialize() const { Writer w; write(w); return w.b; }
  static JointTrajectoryPoint deserialize(const std::vector<uint8_t>& b) { Reader r(b); return read(r); }
};

struct JointTrajectory {
  static constexpr const char* TYPE = "trajectory_msgs/JointTrajectory";
  static constexpr const char* MD5 = "65b4f94a94d1ed67169da35a02f33d3f";
  static constexpr const char* DEFINITION =
      "std_msgs/Header header\nstring[] joint_names\ntrajectory_msgs/JointTrajectoryPoint[] points\n";
  std_msgs::Header header;
  std::vector<std::string> joint_names;
  std::vector<JointTrajectoryPoint> points;
  std::vector<uint8_t> serialize() const {
    Writer w; header.write(w);
    w.u32((uint32_t)joint_names.size()); for (auto& s : joint_names) w.str(s);
    w.u32((uint32_t)points.size()); for (auto& p : points) p.write(w);
    return w.b;
  }
  static JointTrajectory deserialize(const std::vector<uint8_t>& b) {
    Reader r(b); JointTrajectory m; m.header = std_msgs::Header::read(r);
    uint32_t n = r.u32(); m.joint_names.resize(n); for (auto& s : m.joint_names) s = r.str();
    n = r.u32(); m.points.resize(n); for (auto& p : m.points) p = JointTrajectoryPoint::read(r);
    return m;
  }
};

struct MultiDOFJointTrajectoryPoint {
  static constexpr const char* TYPE = "trajectory_msgs/MultiDOFJointTrajectoryPoint";
  static constexpr const char* MD5 = "3ebe08d1abd5b65862d50e09430db776";
  static constexpr const char* DEFINITION =
      "geometry_msgs/Transform[] transforms\ngeometry_msgs/Twist[] velocities\n"
      "geometry_msgs/Twist[] accelerations\nduration time_from_start\n";
  std::vector<geometry_msgs::Transform> transforms;
  std::vector<geometry_msgs::Twist> velocities, accelerations;
  int32_t time_from_start_sec = 0, time_from_start_nsec = 0;
  void write(Writer& w) const {
    w.u32((uint32_t)transforms.size()); for (auto& t : transforms) t.write(w);
    w.u32((uint32_t)velocities.size()); for (auto& t : velocities) t.write(w);
    w.u32((uint32_t)accelerations.size()); for (auto& t : accelerations) t.write(w);
    w.i32(time_from_start_sec); w.i32(time_from_start_nsec);
  }
  static MultiDOFJointTrajectoryPoint read(Reader& r) {
    MultiDOFJointTrajectoryPoint m;
    uint32_t n = r.u32(); m.transforms.resize(n); for (auto& t : m.transforms) t = geometry_msgs::Transform::read(r);
    n = r.u32(); m.velocities.resize(n); for (auto& t : m.velocities) t = geometry_msgs::Twist::read(r);
    n = r.u32(); m.accelerations.resize(n); for (auto& t : m.accelerations) t = geometry_msgs::Twist::read(r);
    m.time_from_start_sec = r.i32(); m.time_from_start_nsec = r.i32();
    return m;
  }
  std::vector<uint8_t> serialize() const { Writer w; write(w); return w.b; }
  static MultiDOFJointTrajectoryPoint deserialize(const std::vector<uint8_t>& b) { Reader r(b); return read(r); }
};

struct MultiDOFJointTrajectory {
  static constexpr const char* TYPE = "trajectory_msgs/MultiDOFJointTrajectory";
  static constexpr const char* MD5 = "ef145a45a5f47b77b7f5cdde4b16c942";
  static constexpr const char* DEFINITION =
      "std_msgs/Header header\nstring[] joint_names\n"
      "trajectory_msgs/MultiDOFJointTrajectoryPoint[] points\n";
  std_msgs::Header header;
  std::vector<std::string> joint_names;
  std::vector<MultiDOFJointTrajectoryPoint> points;
  std::vector<uint8_t> serialize() const {
    Writer w; header.write(w);
    w.u32((uint32_t)joint_names.size()); for (auto& s : joint_names) w.str(s);
    w.u32((uint32_t)points.size()); for (auto& p : points) p.write(w);
    return w.b;
  }
  static MultiDOFJointTrajectory deserialize(const std::vector<uint8_t>& b) {
    Reader r(b); MultiDOFJointTrajectory m; m.header = std_msgs::Header::read(r);
    uint32_t n = r.u32(); m.joint_names.resize(n); for (auto& s : m.joint_names) s = r.str();
    n = r.u32(); m.points.resize(n); for (auto& p : m.points) p = MultiDOFJointTrajectoryPoint::read(r);
    return m;
  }
};

}  // namespace trajectory_msgs
