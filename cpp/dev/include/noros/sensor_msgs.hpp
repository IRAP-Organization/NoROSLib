// sensor_msgs.hpp — sensor_msgs message types (TYPE/MD5 match real ROS).
#pragma once
#include "noros/message.hpp"
#include "noros/std_msgs.hpp"
#include "noros/geometry_msgs.hpp"

#include <array>

namespace sensor_msgs {
using noros::Reader;
using noros::Writer;

// sensor_msgs/Image : Header header, uint32 height,width, string encoding,
//                     uint8 is_bigendian, uint32 step, uint8[] data
struct Image {
  static constexpr const char* TYPE = "sensor_msgs/Image";
  static constexpr const char* MD5 = "060021388200f6f0f447d0fcd9c64743";
  static constexpr const char* DEFINITION =
      "std_msgs/Header header\nuint32 height\nuint32 width\nstring encoding\n"
      "uint8 is_bigendian\nuint32 step\nuint8[] data\n";
  std_msgs::Header header;
  uint32_t height = 0, width = 0;
  std::string encoding = "bgr8";
  uint8_t is_bigendian = 0;
  uint32_t step = 0;
  std::vector<uint8_t> data;
  std::vector<uint8_t> serialize() const {
    Writer w; header.write(w); w.u32(height); w.u32(width); w.str(encoding);
    w.u8(is_bigendian); w.u32(step ? step : width * 3); w.bytes(data); return w.b;
  }
  static Image deserialize(const std::vector<uint8_t>& b) {
    Reader r(b); Image m; m.header = std_msgs::Header::read(r);
    m.height = r.u32(); m.width = r.u32(); m.encoding = r.str();
    m.is_bigendian = r.u8(); m.step = r.u32(); m.data = r.bytes(); return m;
  }
};

// sensor_msgs/CompressedImage : Header header, string format, uint8[] data
struct CompressedImage {
  static constexpr const char* TYPE = "sensor_msgs/CompressedImage";
  static constexpr const char* MD5 = "8f7a12909da2c9d3332d540a0977563f";
  static constexpr const char* DEFINITION =
      "std_msgs/Header header\nstring format\nuint8[] data\n";
  std_msgs::Header header;
  std::string format = "jpeg";
  std::vector<uint8_t> data;
  std::vector<uint8_t> serialize() const {
    Writer w; header.write(w); w.str(format); w.bytes(data); return w.b;
  }
  static CompressedImage deserialize(const std::vector<uint8_t>& b) {
    Reader r(b); CompressedImage m; m.header = std_msgs::Header::read(r);
    m.format = r.str(); m.data = r.bytes(); return m;
  }
};

struct PointField {
  static constexpr const char* TYPE = "sensor_msgs/PointField";
  static constexpr const char* MD5 = "268eacb2962780ceac86cbd17e328150";
  static constexpr const char* DEFINITION =
      "uint8 INT8=1\nuint8 UINT8=2\nuint8 INT16=3\nuint8 UINT16=4\nuint8 INT32=5\n"
      "uint8 UINT32=6\nuint8 FLOAT32=7\nuint8 FLOAT64=8\n"
      "string name\nuint32 offset\nuint8 datatype\nuint32 count\n";
  enum { INT8 = 1, UINT8, INT16, UINT16, INT32, UINT32, FLOAT32, FLOAT64 };
  std::string name;
  uint32_t offset = 0;
  uint8_t datatype = FLOAT32;
  uint32_t count = 1;
  void write(Writer& w) const { w.str(name); w.u32(offset); w.u8(datatype); w.u32(count); }
  static PointField read(Reader& r) {
    PointField f; f.name = r.str(); f.offset = r.u32(); f.datatype = r.u8(); f.count = r.u32(); return f;
  }
  std::vector<uint8_t> serialize() const { Writer w; write(w); return w.b; }
  static PointField deserialize(const std::vector<uint8_t>& b) { Reader r(b); return read(r); }
};

// sensor_msgs/PointCloud2
struct PointCloud2 {
  static constexpr const char* TYPE = "sensor_msgs/PointCloud2";
  static constexpr const char* MD5 = "1158d486dd51d683ce2f1be655c3c181";
  static constexpr const char* DEFINITION =
      "std_msgs/Header header\nuint32 height\nuint32 width\n"
      "sensor_msgs/PointField[] fields\nbool is_bigendian\nuint32 point_step\n"
      "uint32 row_step\nuint8[] data\nbool is_dense\n";
  std_msgs::Header header;
  uint32_t height = 1, width = 0;
  std::vector<PointField> fields;
  uint8_t is_bigendian = 0;
  uint32_t point_step = 0, row_step = 0;
  std::vector<uint8_t> data;
  uint8_t is_dense = 1;
  std::vector<uint8_t> serialize() const {
    Writer w; header.write(w); w.u32(height); w.u32(width);
    w.u32(static_cast<uint32_t>(fields.size()));
    for (const auto& f : fields) f.write(w);
    w.u8(is_bigendian); w.u32(point_step); w.u32(row_step); w.bytes(data); w.u8(is_dense);
    return w.b;
  }
  static PointCloud2 deserialize(const std::vector<uint8_t>& b) {
    Reader r(b); PointCloud2 m; m.header = std_msgs::Header::read(r);
    m.height = r.u32(); m.width = r.u32();
    uint32_t nf = r.u32();
    for (uint32_t i = 0; i < nf; ++i) m.fields.push_back(PointField::read(r));
    m.is_bigendian = r.u8(); m.point_step = r.u32(); m.row_step = r.u32();
    m.data = r.bytes(); m.is_dense = r.u8(); return m;
  }
};

struct RegionOfInterest {
  static constexpr const char* TYPE = "sensor_msgs/RegionOfInterest";
  static constexpr const char* MD5 = "bdb633039d588fcccb441a4d43ccfe09";
  static constexpr const char* DEFINITION =
      "uint32 x_offset\nuint32 y_offset\nuint32 height\nuint32 width\nbool do_rectify\n";
  uint32_t x_offset = 0, y_offset = 0, height = 0, width = 0;
  bool do_rectify = false;
  void write(Writer& w) const { w.u32(x_offset); w.u32(y_offset); w.u32(height); w.u32(width); w.boolean(do_rectify); }
  static RegionOfInterest read(Reader& r) {
    RegionOfInterest m; m.x_offset = r.u32(); m.y_offset = r.u32(); m.height = r.u32();
    m.width = r.u32(); m.do_rectify = r.boolean(); return m;
  }
  std::vector<uint8_t> serialize() const { Writer w; write(w); return w.b; }
  static RegionOfInterest deserialize(const std::vector<uint8_t>& b) { Reader r(b); return read(r); }
};

struct Imu {
  static constexpr const char* TYPE = "sensor_msgs/Imu";
  static constexpr const char* MD5 = "6a62c6daae103f4ff57a132d6f95cec2";
  static constexpr const char* DEFINITION =
      "std_msgs/Header header\ngeometry_msgs/Quaternion orientation\nfloat64[9] orientation_covariance\n"
      "geometry_msgs/Vector3 angular_velocity\nfloat64[9] angular_velocity_covariance\n"
      "geometry_msgs/Vector3 linear_acceleration\nfloat64[9] linear_acceleration_covariance\n";
  std_msgs::Header header;
  geometry_msgs::Quaternion orientation;
  std::array<double, 9> orientation_covariance{};
  geometry_msgs::Vector3 angular_velocity;
  std::array<double, 9> angular_velocity_covariance{};
  geometry_msgs::Vector3 linear_acceleration;
  std::array<double, 9> linear_acceleration_covariance{};
  std::vector<uint8_t> serialize() const {
    Writer w; header.write(w); orientation.write(w);
    for (double v : orientation_covariance) w.f64(v);
    angular_velocity.write(w); for (double v : angular_velocity_covariance) w.f64(v);
    linear_acceleration.write(w); for (double v : linear_acceleration_covariance) w.f64(v);
    return w.b;
  }
  static Imu deserialize(const std::vector<uint8_t>& b) {
    Reader r(b); Imu m; m.header = std_msgs::Header::read(r);
    m.orientation = geometry_msgs::Quaternion::read(r);
    for (auto& v : m.orientation_covariance) v = r.f64();
    m.angular_velocity = geometry_msgs::Vector3::read(r);
    for (auto& v : m.angular_velocity_covariance) v = r.f64();
    m.linear_acceleration = geometry_msgs::Vector3::read(r);
    for (auto& v : m.linear_acceleration_covariance) v = r.f64();
    return m;
  }
};

struct LaserScan {
  static constexpr const char* TYPE = "sensor_msgs/LaserScan";
  static constexpr const char* MD5 = "90c7ef2dc6895d81024acba2ac42f369";
  static constexpr const char* DEFINITION =
      "std_msgs/Header header\nfloat32 angle_min\nfloat32 angle_max\nfloat32 angle_increment\n"
      "float32 time_increment\nfloat32 scan_time\nfloat32 range_min\nfloat32 range_max\n"
      "float32[] ranges\nfloat32[] intensities\n";
  std_msgs::Header header;
  float angle_min = 0, angle_max = 0, angle_increment = 0;
  float time_increment = 0, scan_time = 0, range_min = 0, range_max = 0;
  std::vector<float> ranges, intensities;
  std::vector<uint8_t> serialize() const {
    Writer w; header.write(w); w.f32(angle_min); w.f32(angle_max); w.f32(angle_increment);
    w.f32(time_increment); w.f32(scan_time); w.f32(range_min); w.f32(range_max);
    w.u32((uint32_t)ranges.size()); for (float v : ranges) w.f32(v);
    w.u32((uint32_t)intensities.size()); for (float v : intensities) w.f32(v);
    return w.b;
  }
  static LaserScan deserialize(const std::vector<uint8_t>& b) {
    Reader r(b); LaserScan m; m.header = std_msgs::Header::read(r);
    m.angle_min = r.f32(); m.angle_max = r.f32(); m.angle_increment = r.f32();
    m.time_increment = r.f32(); m.scan_time = r.f32(); m.range_min = r.f32(); m.range_max = r.f32();
    uint32_t n = r.u32(); m.ranges.resize(n); for (auto& v : m.ranges) v = r.f32();
    n = r.u32(); m.intensities.resize(n); for (auto& v : m.intensities) v = r.f32();
    return m;
  }
};

struct JointState {
  static constexpr const char* TYPE = "sensor_msgs/JointState";
  static constexpr const char* MD5 = "3066dcd76a6cfaef579bd0f34173e9fd";
  static constexpr const char* DEFINITION =
      "std_msgs/Header header\nstring[] name\nfloat64[] position\nfloat64[] velocity\nfloat64[] effort\n";
  std_msgs::Header header;
  std::vector<std::string> name;
  std::vector<double> position, velocity, effort;
  std::vector<uint8_t> serialize() const {
    Writer w; header.write(w);
    w.u32((uint32_t)name.size()); for (auto& s : name) w.str(s);
    w.u32((uint32_t)position.size()); for (double v : position) w.f64(v);
    w.u32((uint32_t)velocity.size()); for (double v : velocity) w.f64(v);
    w.u32((uint32_t)effort.size()); for (double v : effort) w.f64(v);
    return w.b;
  }
  static JointState deserialize(const std::vector<uint8_t>& b) {
    Reader r(b); JointState m; m.header = std_msgs::Header::read(r);
    uint32_t n = r.u32(); m.name.resize(n); for (auto& s : m.name) s = r.str();
    n = r.u32(); m.position.resize(n); for (auto& v : m.position) v = r.f64();
    n = r.u32(); m.velocity.resize(n); for (auto& v : m.velocity) v = r.f64();
    n = r.u32(); m.effort.resize(n); for (auto& v : m.effort) v = r.f64();
    return m;
  }
};

struct NavSatStatus {
  static constexpr const char* TYPE = "sensor_msgs/NavSatStatus";
  static constexpr const char* MD5 = "331cdbddfa4bc96ffc3b9ad98900a54c";
  static constexpr const char* DEFINITION =
      "int8 STATUS_NO_FIX=-1\nint8 STATUS_FIX=0\nint8 STATUS_SBAS_FIX=1\nint8 STATUS_GBAS_FIX=2\n"
      "int8 status\nuint16 SERVICE_GPS=1\nuint16 SERVICE_GLONASS=2\nuint16 SERVICE_COMPASS=4\n"
      "uint16 SERVICE_GALILEO=8\nuint16 service\n";
  enum { STATUS_NO_FIX = -1, STATUS_FIX = 0, STATUS_SBAS_FIX = 1, STATUS_GBAS_FIX = 2 };
  int8_t status = -1;
  uint16_t service = 0;
  void write(Writer& w) const { w.i8(status); w.u16(service); }
  static NavSatStatus read(Reader& r) { NavSatStatus m; m.status = r.i8(); m.service = r.u16(); return m; }
  std::vector<uint8_t> serialize() const { Writer w; write(w); return w.b; }
  static NavSatStatus deserialize(const std::vector<uint8_t>& b) { Reader r(b); return read(r); }
};

struct NavSatFix {
  static constexpr const char* TYPE = "sensor_msgs/NavSatFix";
  static constexpr const char* MD5 = "2d3a8cd499b9b4a0249fb98fd05cfa48";
  static constexpr const char* DEFINITION =
      "std_msgs/Header header\nsensor_msgs/NavSatStatus status\nfloat64 latitude\nfloat64 longitude\n"
      "float64 altitude\nfloat64[9] position_covariance\nuint8 position_covariance_type\n";
  std_msgs::Header header;
  NavSatStatus status;
  double latitude = 0, longitude = 0, altitude = 0;
  std::array<double, 9> position_covariance{};
  uint8_t position_covariance_type = 0;
  std::vector<uint8_t> serialize() const {
    Writer w; header.write(w); status.write(w); w.f64(latitude); w.f64(longitude); w.f64(altitude);
    for (double v : position_covariance) w.f64(v); w.u8(position_covariance_type); return w.b;
  }
  static NavSatFix deserialize(const std::vector<uint8_t>& b) {
    Reader r(b); NavSatFix m; m.header = std_msgs::Header::read(r); m.status = NavSatStatus::read(r);
    m.latitude = r.f64(); m.longitude = r.f64(); m.altitude = r.f64();
    for (auto& v : m.position_covariance) v = r.f64(); m.position_covariance_type = r.u8(); return m;
  }
};

struct Range {
  static constexpr const char* TYPE = "sensor_msgs/Range";
  static constexpr const char* MD5 = "c005c34273dc426c67a020a87bc24148";
  static constexpr const char* DEFINITION =
      "std_msgs/Header header\nuint8 ULTRASOUND=0\nuint8 INFRARED=1\nuint8 radiation_type\n"
      "float32 field_of_view\nfloat32 min_range\nfloat32 max_range\nfloat32 range\n";
  enum { ULTRASOUND = 0, INFRARED = 1 };
  std_msgs::Header header;
  uint8_t radiation_type = 0;
  float field_of_view = 0, min_range = 0, max_range = 0, range = 0;
  std::vector<uint8_t> serialize() const {
    Writer w; header.write(w); w.u8(radiation_type); w.f32(field_of_view);
    w.f32(min_range); w.f32(max_range); w.f32(range); return w.b;
  }
  static Range deserialize(const std::vector<uint8_t>& b) {
    Reader r(b); Range m; m.header = std_msgs::Header::read(r); m.radiation_type = r.u8();
    m.field_of_view = r.f32(); m.min_range = r.f32(); m.max_range = r.f32(); m.range = r.f32(); return m;
  }
};

struct Temperature {
  static constexpr const char* TYPE = "sensor_msgs/Temperature";
  static constexpr const char* MD5 = "ff71b307acdbe7c871a5a6d7ed359100";
  static constexpr const char* DEFINITION =
      "std_msgs/Header header\nfloat64 temperature\nfloat64 variance\n";
  std_msgs::Header header;
  double temperature = 0, variance = 0;
  std::vector<uint8_t> serialize() const {
    Writer w; header.write(w); w.f64(temperature); w.f64(variance); return w.b;
  }
  static Temperature deserialize(const std::vector<uint8_t>& b) {
    Reader r(b); Temperature m; m.header = std_msgs::Header::read(r);
    m.temperature = r.f64(); m.variance = r.f64(); return m;
  }
};

struct MagneticField {
  static constexpr const char* TYPE = "sensor_msgs/MagneticField";
  static constexpr const char* MD5 = "2f3b0b43eed0c9501de0fa3ff89a45aa";
  static constexpr const char* DEFINITION =
      "std_msgs/Header header\ngeometry_msgs/Vector3 magnetic_field\nfloat64[9] magnetic_field_covariance\n";
  std_msgs::Header header;
  geometry_msgs::Vector3 magnetic_field;
  std::array<double, 9> magnetic_field_covariance{};
  std::vector<uint8_t> serialize() const {
    Writer w; header.write(w); magnetic_field.write(w);
    for (double v : magnetic_field_covariance) w.f64(v); return w.b;
  }
  static MagneticField deserialize(const std::vector<uint8_t>& b) {
    Reader r(b); MagneticField m; m.header = std_msgs::Header::read(r);
    m.magnetic_field = geometry_msgs::Vector3::read(r);
    for (auto& v : m.magnetic_field_covariance) v = r.f64(); return m;
  }
};

struct CameraInfo {
  static constexpr const char* TYPE = "sensor_msgs/CameraInfo";
  static constexpr const char* MD5 = "c9a58c1b0b154e0e6da7578cb991d214";
  static constexpr const char* DEFINITION =
      "std_msgs/Header header\nuint32 height\nuint32 width\nstring distortion_model\nfloat64[] D\n"
      "float64[9] K\nfloat64[9] R\nfloat64[12] P\nuint32 binning_x\nuint32 binning_y\n"
      "sensor_msgs/RegionOfInterest roi\n";
  std_msgs::Header header;
  uint32_t height = 0, width = 0;
  std::string distortion_model = "plumb_bob";
  std::vector<double> D;
  std::array<double, 9> K{}, R{};
  std::array<double, 12> P{};
  uint32_t binning_x = 0, binning_y = 0;
  RegionOfInterest roi;
  std::vector<uint8_t> serialize() const {
    Writer w; header.write(w); w.u32(height); w.u32(width); w.str(distortion_model);
    w.u32((uint32_t)D.size()); for (double v : D) w.f64(v);
    for (double v : K) w.f64(v); for (double v : R) w.f64(v); for (double v : P) w.f64(v);
    w.u32(binning_x); w.u32(binning_y); roi.write(w); return w.b;
  }
  static CameraInfo deserialize(const std::vector<uint8_t>& b) {
    Reader r(b); CameraInfo m; m.header = std_msgs::Header::read(r);
    m.height = r.u32(); m.width = r.u32(); m.distortion_model = r.str();
    uint32_t n = r.u32(); m.D.resize(n); for (auto& v : m.D) v = r.f64();
    for (auto& v : m.K) v = r.f64(); for (auto& v : m.R) v = r.f64(); for (auto& v : m.P) v = r.f64();
    m.binning_x = r.u32(); m.binning_y = r.u32(); m.roi = RegionOfInterest::read(r); return m;
  }
};

}  // namespace sensor_msgs
