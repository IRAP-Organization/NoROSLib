// Minimal XML-RPC used by ROS: only the value types ROS actually exchanges
// (string, int, boolean, double, array). Enough to build/parse methodCall and
// methodResponse without any external XML-RPC library.
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace irap_noroslib {

// A tiny XML-RPC value tree.
struct XmlValue {
  enum class Type { Int, Bool, Double, String, Array, Base64, Struct } type = Type::String;
  int64_t i = 0;
  bool b = false;
  double d = 0.0;
  std::string s;  // holds the string, or the RAW (decoded) bytes for Base64
  std::vector<XmlValue> arr;
  std::vector<std::pair<std::string, XmlValue>> members;  // for Struct (dict)

  static XmlValue Int(int64_t v) { XmlValue x; x.type = Type::Int; x.i = v; return x; }
  static XmlValue Bool(bool v) { XmlValue x; x.type = Type::Bool; x.b = v; return x; }
  static XmlValue Str(std::string v) { XmlValue x; x.type = Type::String; x.s = std::move(v); return x; }
  static XmlValue Double(double v) { XmlValue x; x.type = Type::Double; x.d = v; return x; }
  static XmlValue Array(std::vector<XmlValue> v) { XmlValue x; x.type = Type::Array; x.arr = std::move(v); return x; }
  // Base64: `raw` is the decoded byte payload; it is base64-encoded on the wire.
  static XmlValue Base64Bytes(std::string raw) { XmlValue x; x.type = Type::Base64; x.s = std::move(raw); return x; }
  // Struct: an XML-RPC <struct> (a dict of named values), used by ROS params.
  static XmlValue StructVal(std::vector<std::pair<std::string, XmlValue>> m) {
    XmlValue x; x.type = Type::Struct; x.members = std::move(m); return x;
  }

  // Convenience accessors (best-effort; return defaults on type mismatch).
  int64_t as_int() const;
  std::string as_str() const;
  const XmlValue* at(size_t idx) const { return idx < arr.size() ? &arr[idx] : nullptr; }
  // Struct member lookup by name (nullptr if absent or not a Struct).
  const XmlValue* member(const std::string& name) const {
    if (type == Type::Struct)
      for (const auto& kv : members) if (kv.first == name) return &kv.second;
    return nullptr;
  }
};

// Build a <methodCall> body for the given method + params.
std::string build_method_call(const std::string& method, const std::vector<XmlValue>& params);

// Build a <methodResponse> body wrapping a single value.
std::string build_method_response(const XmlValue& value);

// Parse a <methodCall>: extracts method name and params. Returns false on error.
bool parse_method_call(const std::string& xml, std::string* method, std::vector<XmlValue>* params);

// Parse a <methodResponse>: extracts the single return value. Returns false on
// error or if it's a <fault>.
bool parse_method_response(const std::string& xml, XmlValue* out, std::string* fault_msg);

}  // namespace irap_noroslib
