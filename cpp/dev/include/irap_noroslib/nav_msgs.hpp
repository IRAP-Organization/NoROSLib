// nav_msgs.hpp — nav_msgs message types (TYPE/MD5 match real ROS).
#pragma once
#include "irap_noroslib/message.hpp"
#include "irap_noroslib/std_msgs.hpp"
#include "irap_noroslib/geometry_msgs.hpp"

namespace nav_msgs {
using irap_noroslib::Reader;
using irap_noroslib::Writer;

struct MapMetaData {
  static constexpr const char* TYPE = "nav_msgs/MapMetaData";
  static constexpr const char* MD5 = "10cfc8a2818024d3248802c00c95f11b";
  static constexpr const char* DEFINITION =
      "time map_load_time\nfloat32 resolution\nuint32 width\nuint32 height\ngeometry_msgs/Pose origin\n";
  uint32_t map_load_time_sec = 0, map_load_time_nsec = 0;
  float resolution = 0;
  uint32_t width = 0, height = 0;
  geometry_msgs::Pose origin;
  void write(Writer& w) const {
    w.u32(map_load_time_sec); w.u32(map_load_time_nsec);
    w.f32(resolution); w.u32(width); w.u32(height); origin.write(w);
  }
  static MapMetaData read(Reader& r) {
    MapMetaData m; m.map_load_time_sec = r.u32(); m.map_load_time_nsec = r.u32();
    m.resolution = r.f32(); m.width = r.u32(); m.height = r.u32(); m.origin = geometry_msgs::Pose::read(r);
    return m;
  }
  std::vector<uint8_t> serialize() const { Writer w; write(w); return w.b; }
  static MapMetaData deserialize(const std::vector<uint8_t>& b) { Reader r(b); return read(r); }
};

struct Odometry {
  static constexpr const char* TYPE = "nav_msgs/Odometry";
  static constexpr const char* MD5 = "cd5e73d190d741a2f92e81eda573aca7";
  static constexpr const char* DEFINITION =
      "std_msgs/Header header\nstring child_frame_id\ngeometry_msgs/PoseWithCovariance pose\n"
      "geometry_msgs/TwistWithCovariance twist\n";
  std_msgs::Header header;
  std::string child_frame_id;
  geometry_msgs::PoseWithCovariance pose;
  geometry_msgs::TwistWithCovariance twist;
  std::vector<uint8_t> serialize() const {
    Writer w; header.write(w); w.str(child_frame_id); pose.write(w); twist.write(w); return w.b;
  }
  static Odometry deserialize(const std::vector<uint8_t>& b) {
    Reader r(b); Odometry m; m.header = std_msgs::Header::read(r); m.child_frame_id = r.str();
    m.pose = geometry_msgs::PoseWithCovariance::read(r); m.twist = geometry_msgs::TwistWithCovariance::read(r);
    return m;
  }
};

struct Path {
  static constexpr const char* TYPE = "nav_msgs/Path";
  static constexpr const char* MD5 = "6227e2b7e9cce15051f669a5e197bbf7";
  static constexpr const char* DEFINITION =
      "std_msgs/Header header\ngeometry_msgs/PoseStamped[] poses\n";
  std_msgs::Header header;
  std::vector<geometry_msgs::PoseStamped> poses;
  std::vector<uint8_t> serialize() const {
    Writer w; header.write(w); w.u32((uint32_t)poses.size()); for (auto& p : poses) p.write(w); return w.b;
  }
  static Path deserialize(const std::vector<uint8_t>& b) {
    Reader r(b); Path m; m.header = std_msgs::Header::read(r);
    uint32_t n = r.u32(); m.poses.resize(n); for (auto& p : m.poses) p = geometry_msgs::PoseStamped::read(r);
    return m;
  }
};

struct OccupancyGrid {
  static constexpr const char* TYPE = "nav_msgs/OccupancyGrid";
  static constexpr const char* MD5 = "3381f2d731d4076ec5c71b0759edbe4e";
  static constexpr const char* DEFINITION =
      "std_msgs/Header header\nnav_msgs/MapMetaData info\nint8[] data\n";
  std_msgs::Header header;
  MapMetaData info;
  std::vector<int8_t> data;
  std::vector<uint8_t> serialize() const {
    Writer w; header.write(w); info.write(w);
    w.u32((uint32_t)data.size()); for (int8_t v : data) w.i8(v); return w.b;
  }
  static OccupancyGrid deserialize(const std::vector<uint8_t>& b) {
    Reader r(b); OccupancyGrid m; m.header = std_msgs::Header::read(r); m.info = MapMetaData::read(r);
    uint32_t n = r.u32(); m.data.resize(n); for (auto& v : m.data) v = r.i8(); return m;
  }
};

struct GridCells {
  static constexpr const char* TYPE = "nav_msgs/GridCells";
  static constexpr const char* MD5 = "b9e4f5df6d28e272ebde00a3994830f5";
  static constexpr const char* DEFINITION =
      "std_msgs/Header header\nfloat32 cell_width\nfloat32 cell_height\ngeometry_msgs/Point[] cells\n";
  std_msgs::Header header;
  float cell_width = 0, cell_height = 0;
  std::vector<geometry_msgs::Point> cells;
  std::vector<uint8_t> serialize() const {
    Writer w; header.write(w); w.f32(cell_width); w.f32(cell_height);
    w.u32((uint32_t)cells.size()); for (auto& c : cells) c.write(w); return w.b;
  }
  static GridCells deserialize(const std::vector<uint8_t>& b) {
    Reader r(b); GridCells m; m.header = std_msgs::Header::read(r);
    m.cell_width = r.f32(); m.cell_height = r.f32();
    uint32_t n = r.u32(); m.cells.resize(n); for (auto& c : m.cells) c = geometry_msgs::Point::read(r);
    return m;
  }
};

}  // namespace nav_msgs
