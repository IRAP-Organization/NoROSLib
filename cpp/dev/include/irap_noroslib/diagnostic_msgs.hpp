// diagnostic_msgs.hpp — diagnostic_msgs message types (TYPE/MD5 match real ROS).
#pragma once
#include "irap_noroslib/message.hpp"
#include "irap_noroslib/std_msgs.hpp"

namespace diagnostic_msgs {
using irap_noroslib::Reader;
using irap_noroslib::Writer;

struct KeyValue {
  static constexpr const char* TYPE = "diagnostic_msgs/KeyValue";
  static constexpr const char* MD5 = "cf57fdc6617a881a88c16e768132149c";
  static constexpr const char* DEFINITION = "string key\nstring value\n";
  std::string key, value;
  void write(Writer& w) const { w.str(key); w.str(value); }
  static KeyValue read(Reader& r) { KeyValue m; m.key = r.str(); m.value = r.str(); return m; }
  std::vector<uint8_t> serialize() const { Writer w; write(w); return w.b; }
  static KeyValue deserialize(const std::vector<uint8_t>& b) { Reader r(b); return read(r); }
};

struct DiagnosticStatus {
  static constexpr const char* TYPE = "diagnostic_msgs/DiagnosticStatus";
  static constexpr const char* MD5 = "d0ce08bc6e5ba34c7754f563a9cabaf1";
  static constexpr const char* DEFINITION =
      "byte OK=0\nbyte WARN=1\nbyte ERROR=2\nbyte STALE=3\nbyte level\nstring name\nstring message\n"
      "string hardware_id\ndiagnostic_msgs/KeyValue[] values\n";
  enum { OK = 0, WARN = 1, ERROR = 2, STALE = 3 };
  int8_t level = 0;
  std::string name, message, hardware_id;
  std::vector<KeyValue> values;
  void write(Writer& w) const {
    w.i8(level); w.str(name); w.str(message); w.str(hardware_id);
    w.u32((uint32_t)values.size()); for (auto& kv : values) kv.write(w);
  }
  static DiagnosticStatus read(Reader& r) {
    DiagnosticStatus m; m.level = r.i8(); m.name = r.str(); m.message = r.str(); m.hardware_id = r.str();
    uint32_t n = r.u32(); m.values.resize(n); for (auto& kv : m.values) kv = KeyValue::read(r); return m;
  }
  std::vector<uint8_t> serialize() const { Writer w; write(w); return w.b; }
  static DiagnosticStatus deserialize(const std::vector<uint8_t>& b) { Reader r(b); return read(r); }
};

struct DiagnosticArray {
  static constexpr const char* TYPE = "diagnostic_msgs/DiagnosticArray";
  static constexpr const char* MD5 = "60810da900de1dd6ddd437c3503511da";
  static constexpr const char* DEFINITION =
      "std_msgs/Header header\ndiagnostic_msgs/DiagnosticStatus[] status\n";
  std_msgs::Header header;
  std::vector<DiagnosticStatus> status;
  std::vector<uint8_t> serialize() const {
    Writer w; header.write(w); w.u32((uint32_t)status.size()); for (auto& s : status) s.write(w); return w.b;
  }
  static DiagnosticArray deserialize(const std::vector<uint8_t>& b) {
    Reader r(b); DiagnosticArray m; m.header = std_msgs::Header::read(r);
    uint32_t n = r.u32(); m.status.resize(n); for (auto& s : m.status) s = DiagnosticStatus::read(r); return m;
  }
};

}  // namespace diagnostic_msgs
