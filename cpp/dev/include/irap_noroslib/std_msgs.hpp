// std_msgs.hpp — std_msgs message types (TYPE/MD5/DEFINITION match real ROS).
#pragma once
#include "irap_noroslib/message.hpp"

namespace std_msgs {
using irap_noroslib::Reader;
using irap_noroslib::Writer;

struct String {
  static constexpr const char* TYPE = "std_msgs/String";
  static constexpr const char* MD5 = "992ce8a1687cec8c8bd883ec73ca41d1";
  static constexpr const char* DEFINITION = "string data\n";
  std::string data;
  std::vector<uint8_t> serialize() const { Writer w; w.str(data); return w.b; }
  static String deserialize(const std::vector<uint8_t>& buf) { Reader r(buf); String m; m.data = r.str(); return m; }
};

struct Bool {
  static constexpr const char* TYPE = "std_msgs/Bool";
  static constexpr const char* MD5 = "8b94c1b53db61fb6aed406028ad6332a";
  static constexpr const char* DEFINITION = "bool data\n";
  bool data = false;
  std::vector<uint8_t> serialize() const { Writer w; w.boolean(data); return w.b; }
  static Bool deserialize(const std::vector<uint8_t>& buf) { Reader r(buf); Bool m; m.data = r.boolean(); return m; }
};

struct Int32 {
  static constexpr const char* TYPE = "std_msgs/Int32";
  static constexpr const char* MD5 = "da5909fbe378aeaf85e547e830cc1bb7";
  static constexpr const char* DEFINITION = "int32 data\n";
  int32_t data = 0;
  std::vector<uint8_t> serialize() const { Writer w; w.i32(data); return w.b; }
  static Int32 deserialize(const std::vector<uint8_t>& buf) { Reader r(buf); Int32 m; m.data = r.i32(); return m; }
};

struct Int64 {
  static constexpr const char* TYPE = "std_msgs/Int64";
  static constexpr const char* MD5 = "34add168574510e6e17f5d23ecc077ef";
  static constexpr const char* DEFINITION = "int64 data\n";
  int64_t data = 0;
  std::vector<uint8_t> serialize() const { Writer w; w.i64(data); return w.b; }
  static Int64 deserialize(const std::vector<uint8_t>& buf) { Reader r(buf); Int64 m; m.data = r.i64(); return m; }
};

struct Float32 {
  static constexpr const char* TYPE = "std_msgs/Float32";
  static constexpr const char* MD5 = "73fcbf46b49191e672908e50842a83d4";
  static constexpr const char* DEFINITION = "float32 data\n";
  float data = 0.0f;
  std::vector<uint8_t> serialize() const { Writer w; w.f32(data); return w.b; }
  static Float32 deserialize(const std::vector<uint8_t>& buf) { Reader r(buf); Float32 m; m.data = r.f32(); return m; }
};

struct Float64 {
  static constexpr const char* TYPE = "std_msgs/Float64";
  static constexpr const char* MD5 = "fdb28210bfa9d7c91146260178d9a584";
  static constexpr const char* DEFINITION = "float64 data\n";
  double data = 0.0;
  std::vector<uint8_t> serialize() const { Writer w; w.f64(data); return w.b; }
  static Float64 deserialize(const std::vector<uint8_t>& buf) { Reader r(buf); Float64 m; m.data = r.f64(); return m; }
};

// std_msgs/Header : uint32 seq, time stamp, string frame_id
struct Header {
  static constexpr const char* TYPE = "std_msgs/Header";
  static constexpr const char* MD5 = "2176decaecbce78abc3b96ef049fabed";
  static constexpr const char* DEFINITION = "uint32 seq\ntime stamp\nstring frame_id\n";
  uint32_t seq = 0;
  uint32_t stamp_sec = 0;
  uint32_t stamp_nsec = 0;
  std::string frame_id;
  void stamp_now() {
    struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts);
    stamp_sec = static_cast<uint32_t>(ts.tv_sec);
    stamp_nsec = static_cast<uint32_t>(ts.tv_nsec);
  }
  void write(Writer& w) const { w.u32(seq); w.u32(stamp_sec); w.u32(stamp_nsec); w.str(frame_id); }
  static Header read(Reader& r) {
    Header h; h.seq = r.u32(); h.stamp_sec = r.u32(); h.stamp_nsec = r.u32(); h.frame_id = r.str(); return h;
  }
  std::vector<uint8_t> serialize() const { Writer w; write(w); return w.b; }
  static Header deserialize(const std::vector<uint8_t>& buf) { Reader r(buf); return read(r); }
};

struct Int8 {
  static constexpr const char* TYPE = "std_msgs/Int8";
  static constexpr const char* MD5 = "27ffa0c9c4b8fb8492252bcad9e5c57b";
  static constexpr const char* DEFINITION = "int8 data\n";
  int8_t data = 0;
  std::vector<uint8_t> serialize() const { Writer w; w.i8(data); return w.b; }
  static Int8 deserialize(const std::vector<uint8_t>& buf) { Reader r(buf); Int8 m; m.data = r.i8(); return m; }
};

struct Int16 {
  static constexpr const char* TYPE = "std_msgs/Int16";
  static constexpr const char* MD5 = "8524586e34fbd7cb1c08c5f5f1ca0e57";
  static constexpr const char* DEFINITION = "int16 data\n";
  int16_t data = 0;
  std::vector<uint8_t> serialize() const { Writer w; w.u16(static_cast<uint16_t>(data)); return w.b; }
  static Int16 deserialize(const std::vector<uint8_t>& buf) { Reader r(buf); Int16 m; m.data = static_cast<int16_t>(r.u16()); return m; }
};

struct UInt8 {
  static constexpr const char* TYPE = "std_msgs/UInt8";
  static constexpr const char* MD5 = "7c8164229e7d2c17eb95e9231617fdee";
  static constexpr const char* DEFINITION = "uint8 data\n";
  uint8_t data = 0;
  std::vector<uint8_t> serialize() const { Writer w; w.u8(data); return w.b; }
  static UInt8 deserialize(const std::vector<uint8_t>& buf) { Reader r(buf); UInt8 m; m.data = r.u8(); return m; }
};

struct UInt16 {
  static constexpr const char* TYPE = "std_msgs/UInt16";
  static constexpr const char* MD5 = "1df79edf208b629fe6b81923a544552d";
  static constexpr const char* DEFINITION = "uint16 data\n";
  uint16_t data = 0;
  std::vector<uint8_t> serialize() const { Writer w; w.u16(data); return w.b; }
  static UInt16 deserialize(const std::vector<uint8_t>& buf) { Reader r(buf); UInt16 m; m.data = r.u16(); return m; }
};

struct UInt32 {
  static constexpr const char* TYPE = "std_msgs/UInt32";
  static constexpr const char* MD5 = "304a39449588c7f8ce2df6e8001c5fce";
  static constexpr const char* DEFINITION = "uint32 data\n";
  uint32_t data = 0;
  std::vector<uint8_t> serialize() const { Writer w; w.u32(data); return w.b; }
  static UInt32 deserialize(const std::vector<uint8_t>& buf) { Reader r(buf); UInt32 m; m.data = r.u32(); return m; }
};

struct UInt64 {
  static constexpr const char* TYPE = "std_msgs/UInt64";
  static constexpr const char* MD5 = "1b2a79973e8bf53d7b53acb71299cb57";
  static constexpr const char* DEFINITION = "uint64 data\n";
  uint64_t data = 0;
  std::vector<uint8_t> serialize() const { Writer w; w.u64(data); return w.b; }
  static UInt64 deserialize(const std::vector<uint8_t>& buf) { Reader r(buf); UInt64 m; m.data = r.u64(); return m; }
};

// std_msgs/Byte : `byte data` (a signed 8-bit value on the wire).
struct Byte {
  static constexpr const char* TYPE = "std_msgs/Byte";
  static constexpr const char* MD5 = "ad736a2e8818154c487bb80fe42ce43b";
  static constexpr const char* DEFINITION = "byte data\n";
  int8_t data = 0;
  std::vector<uint8_t> serialize() const { Writer w; w.i8(data); return w.b; }
  static Byte deserialize(const std::vector<uint8_t>& buf) { Reader r(buf); Byte m; m.data = r.i8(); return m; }
};

// std_msgs/Char : `char data` (an unsigned 8-bit value on the wire).
struct Char {
  static constexpr const char* TYPE = "std_msgs/Char";
  static constexpr const char* MD5 = "1bf77f25acecdedba0e224b162199717";
  static constexpr const char* DEFINITION = "char data\n";
  uint8_t data = 0;
  std::vector<uint8_t> serialize() const { Writer w; w.u8(data); return w.b; }
  static Char deserialize(const std::vector<uint8_t>& buf) { Reader r(buf); Char m; m.data = r.u8(); return m; }
};

struct Empty {
  static constexpr const char* TYPE = "std_msgs/Empty";
  static constexpr const char* MD5 = "d41d8cd98f00b204e9800998ecf8427e";
  static constexpr const char* DEFINITION = "";
  std::vector<uint8_t> serialize() const { return {}; }
  static Empty deserialize(const std::vector<uint8_t>&) { return Empty{}; }
};

// std_msgs/Time : `time data` — two uint32 (secs, nsecs) on the wire.
struct Time {
  static constexpr const char* TYPE = "std_msgs/Time";
  static constexpr const char* MD5 = "cd7166c74c552c311fbcc2fe5a7bc289";
  static constexpr const char* DEFINITION = "time data\n";
  uint32_t data_sec = 0;
  uint32_t data_nsec = 0;
  std::vector<uint8_t> serialize() const { Writer w; w.u32(data_sec); w.u32(data_nsec); return w.b; }
  static Time deserialize(const std::vector<uint8_t>& buf) { Reader r(buf); Time m; m.data_sec = r.u32(); m.data_nsec = r.u32(); return m; }
};

// std_msgs/Duration : `duration data` — two int32 (secs, nsecs) on the wire.
struct Duration {
  static constexpr const char* TYPE = "std_msgs/Duration";
  static constexpr const char* MD5 = "3e286caf4241d664e55f3ad380e2ae46";
  static constexpr const char* DEFINITION = "duration data\n";
  int32_t data_sec = 0;
  int32_t data_nsec = 0;
  std::vector<uint8_t> serialize() const { Writer w; w.i32(data_sec); w.i32(data_nsec); return w.b; }
  static Duration deserialize(const std::vector<uint8_t>& buf) { Reader r(buf); Duration m; m.data_sec = r.i32(); m.data_nsec = r.i32(); return m; }
};

struct ColorRGBA {
  static constexpr const char* TYPE = "std_msgs/ColorRGBA";
  static constexpr const char* MD5 = "a29a96539573343b1310c73607334b00";
  static constexpr const char* DEFINITION = "float32 r\nfloat32 g\nfloat32 b\nfloat32 a\n";
  float r = 0, g = 0, b = 0, a = 0;
  void write(Writer& w) const { w.f32(r); w.f32(g); w.f32(b); w.f32(a); }
  static ColorRGBA read(Reader& rd) { ColorRGBA c; c.r = rd.f32(); c.g = rd.f32(); c.b = rd.f32(); c.a = rd.f32(); return c; }
  std::vector<uint8_t> serialize() const { Writer w; write(w); return w.b; }
  static ColorRGBA deserialize(const std::vector<uint8_t>& buf) { Reader rd(buf); return read(rd); }
};

}  // namespace std_msgs
