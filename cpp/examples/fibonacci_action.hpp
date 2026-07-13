// Defining an ACTION type in irap_noroslib C++: the 7 auto-generated message structs
// (Goal/Result/Feedback + the Action* wrappers) grouped in a traits struct.
// md5sums come from `rosmsg md5 <type>`. This is actionlib_tutorials/Fibonacci.
#pragma once
#include "irap_noroslib.hpp"
#include "irap_noroslib/std_msgs/Header.h"
#include "irap_noroslib/actionlib_msgs/GoalID.h"

namespace fib {
using irap_noroslib::Reader;
using irap_noroslib::Writer;

inline void write_i32_array(Writer& w, const std::vector<int32_t>& v) {
  w.u32((uint32_t)v.size());
  for (int32_t x : v) w.i32(x);
}
inline std::vector<int32_t> read_i32_array(Reader& r) {
  uint32_t n = r.u32(); std::vector<int32_t> v; v.reserve(n);
  for (uint32_t i = 0; i < n; ++i) v.push_back(r.i32());
  return v;
}

struct Goal {
  static constexpr const char* TYPE = "actionlib_tutorials/FibonacciGoal";
  static constexpr const char* MD5 = "6889063349a00b249bd1661df429d822";
  static constexpr const char* DEFINITION = "int32 order\n";
  int32_t order = 0;
  std::vector<uint8_t> serialize() const { Writer w; w.i32(order); return w.b; }
  static Goal deserialize(const std::vector<uint8_t>& b) { Reader r(b); Goal g; g.order = r.i32(); return g; }
};

struct Result {
  static constexpr const char* TYPE = "actionlib_tutorials/FibonacciResult";
  static constexpr const char* MD5 = "b81e37d2a31925a0e8ae261a8699cb79";
  static constexpr const char* DEFINITION = "int32[] sequence\n";
  std::vector<int32_t> sequence;
  std::vector<uint8_t> serialize() const { Writer w; write_i32_array(w, sequence); return w.b; }
  static Result deserialize(const std::vector<uint8_t>& b) { Reader r(b); Result m; m.sequence = read_i32_array(r); return m; }
};

struct Feedback {
  static constexpr const char* TYPE = "actionlib_tutorials/FibonacciFeedback";
  static constexpr const char* MD5 = "b81e37d2a31925a0e8ae261a8699cb79";
  static constexpr const char* DEFINITION = "int32[] sequence\n";
  std::vector<int32_t> sequence;
  std::vector<uint8_t> serialize() const { Writer w; write_i32_array(w, sequence); return w.b; }
  static Feedback deserialize(const std::vector<uint8_t>& b) { Reader r(b); Feedback m; m.sequence = read_i32_array(r); return m; }
};

struct ActionGoal {
  static constexpr const char* TYPE = "actionlib_tutorials/FibonacciActionGoal";
  static constexpr const char* MD5 = "006871c7fa1d0e3d5fe2226bf17b2a94";
  static constexpr const char* DEFINITION = "";
  std_msgs::Header header;
  actionlib_msgs::GoalID goal_id;
  Goal goal;
  std::vector<uint8_t> serialize() const {
    Writer w; header.write(w); goal_id.write(w); auto g = goal.serialize();
    w.b.insert(w.b.end(), g.begin(), g.end()); return w.b;
  }
  static ActionGoal deserialize(const std::vector<uint8_t>& b) {
    Reader r(b); ActionGoal m; m.header = std_msgs::Header::read(r);
    m.goal_id = actionlib_msgs::GoalID::read(r); m.goal.order = r.i32(); return m;
  }
};

struct ActionResult {
  static constexpr const char* TYPE = "actionlib_tutorials/FibonacciActionResult";
  static constexpr const char* MD5 = "bee73a9fe29ae25e966e105f5553dd03";
  static constexpr const char* DEFINITION = "";
  std_msgs::Header header;
  actionlib_msgs::GoalStatus status;
  Result result;
  std::vector<uint8_t> serialize() const {
    Writer w; header.write(w); status.write(w); write_i32_array(w, result.sequence); return w.b;
  }
  static ActionResult deserialize(const std::vector<uint8_t>& b) {
    Reader r(b); ActionResult m; m.header = std_msgs::Header::read(r);
    m.status = actionlib_msgs::GoalStatus::read(r); m.result.sequence = read_i32_array(r); return m;
  }
};

struct ActionFeedback {
  static constexpr const char* TYPE = "actionlib_tutorials/FibonacciActionFeedback";
  static constexpr const char* MD5 = "73b8497a9f629a31c0020900e4148f07";
  static constexpr const char* DEFINITION = "";
  std_msgs::Header header;
  actionlib_msgs::GoalStatus status;
  Feedback feedback;
  std::vector<uint8_t> serialize() const {
    Writer w; header.write(w); status.write(w); write_i32_array(w, feedback.sequence); return w.b;
  }
  static ActionFeedback deserialize(const std::vector<uint8_t>& b) {
    Reader r(b); ActionFeedback m; m.header = std_msgs::Header::read(r);
    m.status = actionlib_msgs::GoalStatus::read(r); m.feedback.sequence = read_i32_array(r); return m;
  }
};

// The traits struct SimpleActionClient/Server<Fibonacci> expect.
struct Fibonacci {
  using Goal = fib::Goal;
  using Result = fib::Result;
  using Feedback = fib::Feedback;
  using ActionGoal = fib::ActionGoal;
  using ActionResult = fib::ActionResult;
  using ActionFeedback = fib::ActionFeedback;
};

}  // namespace fib
