// message.hpp — ROS-compatible serialization primitives for irap_noroslib messages.
//
// ROS serialization, in full: little-endian, no padding; a string or variable
// array is prefixed with a uint32 length; a fixed array is its elements back to
// back; fields go in .msg declaration order. Every message struct in
// std_msgs.hpp / geometry_msgs.hpp / sensor_msgs.hpp is that recipe applied to
// one `rosmsg show` definition — copy the pattern to add your own type.
//
// A irap_noroslib message type is any struct that provides:
//   static constexpr const char* TYPE;        // "pkg/Type"
//   static constexpr const char* MD5;          // rosmsg md5 <type>
//   static constexpr const char* DEFINITION;   // full message text (may be "")
//   std::vector<uint8_t> serialize() const;
//   static T deserialize(const std::vector<uint8_t>&);
#pragma once

#include <cstdint>
#include <cstring>
#include <ctime>
#include <stdexcept>
#include <string>
#include <vector>

namespace irap_noroslib {

// ROS `time` and `duration` as values. (The compile-time message structs flatten
// these into their own uint32 pairs; these are what the runtime .msg loader uses.)
struct Time {
  uint32_t sec = 0, nsec = 0;
  bool operator==(const Time& o) const { return sec == o.sec && nsec == o.nsec; }
};
struct Duration {
  int32_t sec = 0, nsec = 0;
  bool operator==(const Duration& o) const { return sec == o.sec && nsec == o.nsec; }
};

// little-endian write buffer
struct Writer {
  std::vector<uint8_t> b;
  void u8(uint8_t v) { b.push_back(v); }
  void i8(int8_t v) { b.push_back(static_cast<uint8_t>(v)); }
  void u16(uint16_t v) { b.push_back(v); b.push_back(v >> 8); }
  void i16(int16_t v) { u16(static_cast<uint16_t>(v)); }
  void i32(int32_t v) { u32(static_cast<uint32_t>(v)); }
  void u32(uint32_t v) {
    b.push_back(v); b.push_back(v >> 8); b.push_back(v >> 16); b.push_back(v >> 24);
  }
  void i64(int64_t v) { u64(static_cast<uint64_t>(v)); }
  void u64(uint64_t v) { for (int i = 0; i < 8; ++i) b.push_back(static_cast<uint8_t>(v >> (8 * i))); }
  void f32(float f) { uint32_t v; std::memcpy(&v, &f, 4); u32(v); }
  void f64(double d) { uint64_t v; std::memcpy(&v, &d, 8); u64(v); }
  void boolean(bool v) { b.push_back(v ? 1 : 0); }
  void str(const std::string& s) {
    u32(static_cast<uint32_t>(s.size()));
    b.insert(b.end(), s.begin(), s.end());
  }
  void bytes(const std::vector<uint8_t>& v) {
    u32(static_cast<uint32_t>(v.size()));
    b.insert(b.end(), v.begin(), v.end());
  }
  void time(const Time& t) { u32(t.sec); u32(t.nsec); }
  void duration(const Duration& d) { i32(d.sec); i32(d.nsec); }
  // raw bytes with NO length prefix -- a fixed-size uint8[N] field
  void raw(const std::vector<uint8_t>& v) { b.insert(b.end(), v.begin(), v.end()); }
};

// little-endian read cursor
struct Reader {
  const uint8_t* p;
  const uint8_t* end;
  explicit Reader(const std::vector<uint8_t>& v) : p(v.data()), end(v.data() + v.size()) {}
  void need(size_t n) { if (p + n > end) throw std::runtime_error("irap_noroslib: short read"); }
  uint8_t u8() { need(1); return *p++; }
  int8_t i8() { return static_cast<int8_t>(u8()); }
  uint16_t u16() { need(2); uint16_t v = p[0] | (p[1] << 8); p += 2; return v; }
  int16_t i16() { return static_cast<int16_t>(u16()); }
  int32_t i32() { return static_cast<int32_t>(u32()); }
  uint32_t u32() {
    need(4);
    uint32_t v = p[0] | (p[1] << 8) | (p[2] << 16) | (uint32_t(p[3]) << 24);
    p += 4; return v;
  }
  int64_t i64() { return static_cast<int64_t>(u64()); }
  uint64_t u64() {
    need(8); uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v |= uint64_t(p[i]) << (8 * i);
    p += 8; return v;
  }
  float f32() { uint32_t v = u32(); float f; std::memcpy(&f, &v, 4); return f; }
  double f64() { uint64_t v = u64(); double d; std::memcpy(&d, &v, 8); return d; }
  bool boolean() { return u8() != 0; }
  std::string str() {
    uint32_t n = u32(); need(n);
    std::string s(reinterpret_cast<const char*>(p), n); p += n; return s;
  }
  std::vector<uint8_t> bytes() {
    uint32_t n = u32(); need(n);
    std::vector<uint8_t> v(p, p + n); p += n; return v;
  }
  Time time() { Time t; t.sec = u32(); t.nsec = u32(); return t; }
  Duration duration() { Duration d; d.sec = i32(); d.nsec = i32(); return d; }
  // exactly n bytes, no length prefix -- a fixed-size uint8[N] field
  std::vector<uint8_t> raw(size_t n) {
    need(n); std::vector<uint8_t> v(p, p + n); p += n; return v;
  }
};

}  // namespace irap_noroslib
