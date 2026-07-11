// std_srvs.hpp — built-in service types for noros C++.
//
// A noros service type provides static TYPE + MD5 (from `rossrv md5 <type>`) and
// nested Request/Response message structs (each a normal noros message). Copy
// this pattern to model any .srv — see examples/add_two_ints_server.cpp.
#pragma once
#include "noros/message.hpp"

namespace std_srvs {
using noros::Reader;
using noros::Writer;

// std_srvs/Empty : (no fields) --- (no fields)
struct Empty {
  static constexpr const char* TYPE = "std_srvs/Empty";
  static constexpr const char* MD5 = "d41d8cd98f00b204e9800998ecf8427e";
  struct Request {
    static constexpr const char* TYPE = "std_srvs/EmptyRequest";
    std::vector<uint8_t> serialize() const { return {}; }
    static Request deserialize(const std::vector<uint8_t>&) { return {}; }
  };
  struct Response {
    static constexpr const char* TYPE = "std_srvs/EmptyResponse";
    std::vector<uint8_t> serialize() const { return {}; }
    static Response deserialize(const std::vector<uint8_t>&) { return {}; }
  };
};

// std_srvs/Trigger : (empty) --- bool success, string message
struct Trigger {
  static constexpr const char* TYPE = "std_srvs/Trigger";
  static constexpr const char* MD5 = "937c9679a518e3a18d831e57125ea522";
  struct Request {
    static constexpr const char* TYPE = "std_srvs/TriggerRequest";
    std::vector<uint8_t> serialize() const { return {}; }
    static Request deserialize(const std::vector<uint8_t>&) { return {}; }
  };
  struct Response {
    static constexpr const char* TYPE = "std_srvs/TriggerResponse";
    bool success = false;
    std::string message;
    std::vector<uint8_t> serialize() const { Writer w; w.boolean(success); w.str(message); return w.b; }
    static Response deserialize(const std::vector<uint8_t>& b) {
      Reader r(b); Response m; m.success = r.boolean(); m.message = r.str(); return m;
    }
  };
};

// std_srvs/SetBool : bool data --- bool success, string message
struct SetBool {
  static constexpr const char* TYPE = "std_srvs/SetBool";
  static constexpr const char* MD5 = "09fb03525b03e7ea1fd3992bafd87e16";
  struct Request {
    static constexpr const char* TYPE = "std_srvs/SetBoolRequest";
    bool data = false;
    std::vector<uint8_t> serialize() const { Writer w; w.boolean(data); return w.b; }
    static Request deserialize(const std::vector<uint8_t>& b) {
      Reader r(b); Request m; m.data = r.boolean(); return m;
    }
  };
  struct Response {
    static constexpr const char* TYPE = "std_srvs/SetBoolResponse";
    bool success = false;
    std::string message;
    std::vector<uint8_t> serialize() const { Writer w; w.boolean(success); w.str(message); return w.b; }
    static Response deserialize(const std::vector<uint8_t>& b) {
      Reader r(b); Response m; m.success = r.boolean(); m.message = r.str(); return m;
    }
  };
};

}  // namespace std_srvs
