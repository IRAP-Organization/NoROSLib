// Defining your OWN service type in noros C++.
//
// A noros service type = static TYPE + MD5 (from `rossrv md5 <type>`) + nested
// Request/Response message structs. Here: rospy_tutorials/AddTwoInts.
#pragma once
#include "noros.hpp"

struct AddTwoInts {
  static constexpr const char* TYPE = "rospy_tutorials/AddTwoInts";
  static constexpr const char* MD5 = "6a2e34150c00229791cc89ff309fff21";  // rossrv md5
  struct Request {
    static constexpr const char* TYPE = "rospy_tutorials/AddTwoIntsRequest";
    int64_t a = 0, b = 0;
    std::vector<uint8_t> serialize() const { noros::Writer w; w.i64(a); w.i64(b); return w.b; }
    static Request deserialize(const std::vector<uint8_t>& buf) {
      noros::Reader r(buf); Request m; m.a = r.i64(); m.b = r.i64(); return m;
    }
  };
  struct Response {
    static constexpr const char* TYPE = "rospy_tutorials/AddTwoIntsResponse";
    int64_t sum = 0;
    std::vector<uint8_t> serialize() const { noros::Writer w; w.i64(sum); return w.b; }
    static Response deserialize(const std::vector<uint8_t>& buf) {
      noros::Reader r(buf); Response m; m.sum = r.i64(); return m;
    }
  };
};
