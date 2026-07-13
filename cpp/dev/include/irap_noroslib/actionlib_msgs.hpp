// actionlib_msgs.hpp — the standard action bookkeeping messages.
#pragma once
#include "irap_noroslib/message.hpp"
#include "irap_noroslib/std_msgs.hpp"

namespace actionlib_msgs {
using irap_noroslib::Reader;
using irap_noroslib::Writer;

struct GoalID {
  static constexpr const char* TYPE = "actionlib_msgs/GoalID";
  static constexpr const char* MD5 = "302881f31927c1df708a2dbab0e80ee8";
  static constexpr const char* DEFINITION = "time stamp\nstring id\n";
  uint32_t stamp_sec = 0, stamp_nsec = 0;
  std::string id;
  void write(Writer& w) const { w.u32(stamp_sec); w.u32(stamp_nsec); w.str(id); }
  static GoalID read(Reader& r) {
    GoalID g; g.stamp_sec = r.u32(); g.stamp_nsec = r.u32(); g.id = r.str(); return g;
  }
  std::vector<uint8_t> serialize() const { Writer w; write(w); return w.b; }
  static GoalID deserialize(const std::vector<uint8_t>& b) { Reader r(b); return read(r); }
};

struct GoalStatus {
  static constexpr const char* TYPE = "actionlib_msgs/GoalStatus";
  static constexpr const char* MD5 = "d388f9b87b3c471f784434d671988d4a";
  static constexpr const char* DEFINITION =
      "actionlib_msgs/GoalID goal_id\nuint8 status\nuint8 PENDING=0\nuint8 ACTIVE=1\n"
      "uint8 PREEMPTED=2\nuint8 SUCCEEDED=3\nuint8 ABORTED=4\nuint8 REJECTED=5\n"
      "uint8 PREEMPTING=6\nuint8 RECALLING=7\nuint8 RECALLED=8\nuint8 LOST=9\nstring text\n";
  enum { PENDING = 0, ACTIVE, PREEMPTED, SUCCEEDED, ABORTED, REJECTED,
         PREEMPTING, RECALLING, RECALLED, LOST };
  GoalID goal_id;
  uint8_t status = PENDING;
  std::string text;
  void write(Writer& w) const { goal_id.write(w); w.u8(status); w.str(text); }
  static GoalStatus read(Reader& r) {
    GoalStatus s; s.goal_id = GoalID::read(r); s.status = r.u8(); s.text = r.str(); return s;
  }
  std::vector<uint8_t> serialize() const { Writer w; write(w); return w.b; }
  static GoalStatus deserialize(const std::vector<uint8_t>& b) { Reader r(b); return read(r); }
};

struct GoalStatusArray {
  static constexpr const char* TYPE = "actionlib_msgs/GoalStatusArray";
  static constexpr const char* MD5 = "8b2b82f13216d0a8ea88bd3af735e619";
  static constexpr const char* DEFINITION =
      "std_msgs/Header header\nactionlib_msgs/GoalStatus[] status_list\n";
  std_msgs::Header header;
  std::vector<GoalStatus> status_list;
  std::vector<uint8_t> serialize() const {
    Writer w; header.write(w); w.u32((uint32_t)status_list.size());
    for (const auto& s : status_list) s.write(w);
    return w.b;
  }
  static GoalStatusArray deserialize(const std::vector<uint8_t>& b) {
    Reader r(b); GoalStatusArray a; a.header = std_msgs::Header::read(r);
    uint32_t n = r.u32();
    for (uint32_t i = 0; i < n; ++i) a.status_list.push_back(GoalStatus::read(r));
    return a;
  }
};

}  // namespace actionlib_msgs
