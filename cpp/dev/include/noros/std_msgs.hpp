// std_msgs.hpp — std_msgs message types (TYPE/MD5/DEFINITION match real ROS).
#pragma once
#include "noros/message.hpp"

namespace std_msgs {
using noros::Reader;
using noros::Writer;

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
