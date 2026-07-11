// sensor_msgs.hpp — sensor_msgs message types (TYPE/MD5 match real ROS).
#pragma once
#include "noros/message.hpp"
#include "noros/std_msgs.hpp"

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
  enum { INT8 = 1, UINT8, INT16, UINT16, INT32, UINT32, FLOAT32, FLOAT64 };
  std::string name;
  uint32_t offset = 0;
  uint8_t datatype = FLOAT32;
  uint32_t count = 1;
  void write(Writer& w) const { w.str(name); w.u32(offset); w.u8(datatype); w.u32(count); }
  static PointField read(Reader& r) {
    PointField f; f.name = r.str(); f.offset = r.u32(); f.datatype = r.u8(); f.count = r.u32(); return f;
  }
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

}  // namespace sensor_msgs
