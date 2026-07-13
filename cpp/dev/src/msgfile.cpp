// msgfile.cpp -- built-in seeding, and loading .msg/.srv/.action files from disk.
#include "irap_noroslib/msgfile.hpp"

#include <stdexcept>

#include "irap_noroslib/actionlib_msgs.hpp"
#include "irap_noroslib/diagnostic_msgs.hpp"
#include "irap_noroslib/geometry_msgs.hpp"
#include "irap_noroslib/nav_msgs.hpp"
#include "irap_noroslib/platform.hpp"
#include "irap_noroslib/sensor_msgs.hpp"
#include "irap_noroslib/std_msgs.hpp"
#include "irap_noroslib/std_srvs.hpp"
#include "irap_noroslib/trajectory_msgs.hpp"

namespace irap_noroslib {
namespace {

struct Builtin {
  const char* type;
  const char* text;   // the struct's own DEFINITION -- one source of truth
  const char* md5;    // its hardcoded MD5, only so the self-test can compare
};

#define B(T) {T::TYPE, T::DEFINITION, T::MD5}

const Builtin kBuiltins[] = {
    // std_msgs (19)
    B(std_msgs::String), B(std_msgs::Bool), B(std_msgs::Int32), B(std_msgs::Int64),
    B(std_msgs::Float32), B(std_msgs::Float64), B(std_msgs::Header), B(std_msgs::Int8),
    B(std_msgs::Int16), B(std_msgs::UInt8), B(std_msgs::UInt16), B(std_msgs::UInt32),
    B(std_msgs::UInt64), B(std_msgs::Byte), B(std_msgs::Char), B(std_msgs::Empty),
    B(std_msgs::Time), B(std_msgs::Duration), B(std_msgs::ColorRGBA),
    // geometry_msgs (16)
    B(geometry_msgs::Vector3), B(geometry_msgs::Point), B(geometry_msgs::Point32),
    B(geometry_msgs::Quaternion), B(geometry_msgs::Twist), B(geometry_msgs::Accel),
    B(geometry_msgs::Wrench), B(geometry_msgs::Pose), B(geometry_msgs::PoseStamped),
    B(geometry_msgs::TwistStamped), B(geometry_msgs::PoseArray), B(geometry_msgs::Polygon),
    B(geometry_msgs::Transform), B(geometry_msgs::TransformStamped),
    B(geometry_msgs::PoseWithCovariance), B(geometry_msgs::TwistWithCovariance),
    // sensor_msgs (14)
    B(sensor_msgs::Image), B(sensor_msgs::CompressedImage), B(sensor_msgs::PointField),
    B(sensor_msgs::PointCloud2), B(sensor_msgs::RegionOfInterest), B(sensor_msgs::Imu),
    B(sensor_msgs::LaserScan), B(sensor_msgs::JointState), B(sensor_msgs::NavSatStatus),
    B(sensor_msgs::NavSatFix), B(sensor_msgs::Range), B(sensor_msgs::Temperature),
    B(sensor_msgs::MagneticField), B(sensor_msgs::CameraInfo),
    // nav_msgs (5)
    B(nav_msgs::MapMetaData), B(nav_msgs::Odometry), B(nav_msgs::Path),
    B(nav_msgs::OccupancyGrid), B(nav_msgs::GridCells),
    // diagnostic_msgs (3)
    B(diagnostic_msgs::KeyValue), B(diagnostic_msgs::DiagnosticStatus),
    B(diagnostic_msgs::DiagnosticArray),
    // trajectory_msgs (4)
    B(trajectory_msgs::JointTrajectoryPoint), B(trajectory_msgs::JointTrajectory),
    B(trajectory_msgs::MultiDOFJointTrajectoryPoint),
    B(trajectory_msgs::MultiDOFJointTrajectory),
    // actionlib_msgs (3)
    B(actionlib_msgs::GoalID), B(actionlib_msgs::GoalStatus),
    B(actionlib_msgs::GoalStatusArray),
};
#undef B

// std_srvs has no DEFINITION constants (only service-level md5s), so its texts
// are spelled out here -- same as the Python side does.
struct BuiltinSrv {
  const char* type;
  const char* req;
  const char* resp;
  const char* md5;
};
const BuiltinSrv kBuiltinSrvs[] = {
    {"std_srvs/Empty", "", "", std_srvs::Empty::MD5},
    {"std_srvs/Trigger", "", "bool success\nstring message", std_srvs::Trigger::MD5},
    {"std_srvs/SetBool", "bool data", "bool success\nstring message",
     std_srvs::SetBool::MD5},
};

std::string strip_ext(const std::string& name, const std::string& ext) {
  if (name.size() <= ext.size() || name.compare(name.size() - ext.size(), ext.size(), ext) != 0)
    throw std::runtime_error("irap_noroslib: expected a " + ext + " file, got \"" + name + "\"");
  return name.substr(0, name.size() - ext.size());
}

/// "pkg/Type" for a file. `pkg` wins; else infer a <pkg>/<subdir>/<Type><ext> layout.
std::string full_type_of(const std::string& path, const std::string& pkg,
                         const std::string& subdir, const std::string& ext) {
  std::string type_name = strip_ext(fs_basename(path), ext);
  std::string p = pkg;
  if (p.empty()) {
    std::string parent = fs_dirname(path);
    std::string grand = fs_dirname(parent);
    if (fs_basename(parent) == subdir && !grand.empty()) {
      p = fs_basename(grand);
    } else {
      throw std::runtime_error(
          "irap_noroslib: cannot tell which ROS package \"" + path +
          "\" belongs to. Pass the package name, e.g.\n"
          "    load_" + subdir + "_file(\"" + path + "\", \"my_robot_msgs\");\n"
          "ROS names a type \"pkg/" + type_name + "\", so it needs the package the "
          "message came from.");
    }
  }
  return p + "/" + type_name;
}

std::string read_file(const std::string& path) {
  std::string full = fs_expand_user(path);
  std::string text;
  if (!fs_read_file(full, &text))
    throw std::runtime_error("irap_noroslib: no such file: " + path);
  return text;
}

/// Split .srv/.action text on lines that are exactly "---".
std::vector<std::string> split_sections(const std::string& text, size_t n,
                                        const std::string& what) {
  std::vector<std::string> parts;
  std::string cur;
  std::string line;
  auto flush_line = [&](const std::string& l) {
    std::string t = l;
    while (!t.empty() && (t.back() == ' ' || t.back() == '\t' || t.back() == '\r')) t.pop_back();
    size_t a = t.find_first_not_of(" \t");
    std::string trimmed = a == std::string::npos ? "" : t.substr(a);
    if (trimmed == "---") {
      parts.push_back(cur);
      cur.clear();
    } else {
      cur += l;
      cur += "\n";
    }
  };
  for (char c : text) {
    if (c == '\n') { flush_line(line); line.clear(); }
    else { line += c; }
  }
  if (!line.empty()) flush_line(line);
  parts.push_back(cur);

  if (parts.size() != n)
    throw std::runtime_error("irap_noroslib: " + what + ": expected " + std::to_string(n) +
                             " sections separated by '---', found " +
                             std::to_string(parts.size()));
  for (std::string& p : parts)
    if (p.empty()) p = "\n";
  return parts;
}

}  // namespace

// Called by Registry on first use. Registering from each struct's own DEFINITION
// means there is no second copy of the .msg text to drift out of sync -- and the
// md5 comes out *computed*, which selftest_builtin_md5() then checks against the
// hardcoded constant.
void seed_builtin_types(Registry& r) {
  for (const Builtin& b : kBuiltins) r.register_msg(b.type, b.text);
  for (const BuiltinSrv& s : kBuiltinSrvs) {
    r.register_msg(std::string(s.type) + "Request", *s.req ? s.req : "\n");
    r.register_msg(std::string(s.type) + "Response", *s.resp ? s.resp : "\n");
  }
}

MsgType register_msg(const std::string& full_type, const std::string& text) {
  return Registry::global().register_msg(full_type, text);
}

MsgType register_msg_from_definition(const std::string& full_type,
                                     const std::string& definition) {
  // Split into (type, text) blocks: the main type, then each dependency behind a
  // "====" separator line and a "MSG: pkg/Type" line.
  std::vector<std::pair<std::string, std::string>> blocks;
  std::string cur_type = full_type, cur, line;
  auto flush_line = [&](const std::string& raw) {
    std::string t = raw;
    while (!t.empty() && (t.back() == ' ' || t.back() == '\t' || t.back() == '\r'))
      t.pop_back();
    size_t a = t.find_first_not_of(" \t");
    std::string s = a == std::string::npos ? "" : t.substr(a);
    if (!s.empty() && s.find_first_not_of('=') == std::string::npos)
      return;                                    // the ==== separator line
    if (s.rfind("MSG:", 0) == 0) {
      blocks.emplace_back(cur_type, cur);
      cur_type = s.substr(4);
      size_t b = cur_type.find_first_not_of(" \t");
      cur_type = b == std::string::npos ? "" : cur_type.substr(b);
      cur.clear();
      return;
    }
    cur += raw;
    cur += "\n";
  };
  for (char c : definition) {
    if (c == '\n') { flush_line(line); line.clear(); }
    else { line += c; }
  }
  if (!line.empty()) flush_line(line);
  blocks.emplace_back(cur_type, cur);

  // Dependencies first. Skip anything already known: real ROS ships the same
  // definitions with comments, which don't change the md5 but would look like a
  // conflicting redefinition.
  Registry& r = Registry::global();
  for (auto it = blocks.rbegin(); it != blocks.rend(); ++it) {
    if (it->first.find('/') == std::string::npos) continue;
    if (!r.has(it->first)) r.register_msg(it->first, it->second);
  }
  return r.get(full_type);
}

SrvType register_srv(const std::string& full_type, const std::string& request_text,
                     const std::string& response_text) {
  Registry& r = Registry::global();
  MsgType req = r.register_msg(full_type + "Request",
                               request_text.empty() ? "\n" : request_text);
  MsgType resp = r.register_msg(full_type + "Response",
                                response_text.empty() ? "\n" : response_text);
  // The ROS rule: hash the two PRE-HASH texts concatenated -- not the two md5s.
  std::string md5 = md5_hex(req.md5_text() + resp.md5_text());
  return SrvType(full_type, md5, req, resp);
}

ActionType register_action(const std::string& full_type, const std::string& goal_text,
                           const std::string& result_text,
                           const std::string& feedback_text) {
  Registry& r = Registry::global();
  MsgType goal = r.register_msg(full_type + "Goal", goal_text.empty() ? "\n" : goal_text);
  MsgType result = r.register_msg(full_type + "Result",
                                  result_text.empty() ? "\n" : result_text);
  MsgType feedback = r.register_msg(full_type + "Feedback",
                                    feedback_text.empty() ? "\n" : feedback_text);
  // The 4 wrappers ROS generates. Fully-qualified names, so no package context.
  MsgType ag = r.register_msg(full_type + "ActionGoal",
                              "std_msgs/Header header\nactionlib_msgs/GoalID goal_id\n" +
                                  full_type + "Goal goal\n");
  MsgType ar = r.register_msg(full_type + "ActionResult",
                              "std_msgs/Header header\nactionlib_msgs/GoalStatus status\n" +
                                  full_type + "Result result\n");
  MsgType af = r.register_msg(full_type + "ActionFeedback",
                              "std_msgs/Header header\nactionlib_msgs/GoalStatus status\n" +
                                  full_type + "Feedback feedback\n");
  MsgType action = r.register_msg(full_type + "Action",
                                  full_type + "ActionGoal action_goal\n" + full_type +
                                      "ActionResult action_result\n" + full_type +
                                      "ActionFeedback action_feedback\n");
  return ActionType(full_type, goal, result, feedback, ag, ar, af, action);
}

MsgType load_msg_file(const std::string& path, const std::string& pkg) {
  std::string full = fs_expand_user(path);
  return register_msg(full_type_of(full, pkg, "msg", ".msg"), read_file(full));
}

SrvType load_srv_file(const std::string& path, const std::string& pkg) {
  std::string full = fs_expand_user(path);
  std::string type = full_type_of(full, pkg, "srv", ".srv");
  std::vector<std::string> s = split_sections(read_file(full), 2, type);
  return register_srv(type, s[0], s[1]);
}

ActionType load_action_file(const std::string& path, const std::string& pkg) {
  std::string full = fs_expand_user(path);
  std::string type = full_type_of(full, pkg, "action", ".action");
  std::vector<std::string> s = split_sections(read_file(full), 3, type);
  return register_action(type, s[0], s[1], s[2]);
}

std::vector<MsgType> load_msg_files(const std::vector<std::string>& paths,
                                    const std::string& pkg) {
  std::vector<MsgType> out;
  out.reserve(paths.size());
  for (const std::string& p : paths) out.push_back(load_msg_file(p, pkg));
  return out;
}

MsgType get_msg_type(const std::string& full_type) {
  return Registry::global().get(full_type);
}

bool has_msg_type(const std::string& full_type) {
  return Registry::global().has(full_type);
}

int selftest_builtin_md5(std::vector<std::string>* failures) {
  Registry& r = Registry::global();
  int n = 0;
  for (const Builtin& b : kBuiltins) {
    MsgType t = r.get(b.type);
    ++n;
    if (t.md5() != b.md5 && failures)
      failures->push_back(std::string(b.type) + ": computed " + t.md5() + ", header says " +
                          b.md5);
  }
  for (const BuiltinSrv& s : kBuiltinSrvs) {
    std::string got = md5_hex(r.get(std::string(s.type) + "Request").md5_text() +
                              r.get(std::string(s.type) + "Response").md5_text());
    ++n;
    if (got != s.md5 && failures)
      failures->push_back(std::string(s.type) + ": computed " + got + ", header says " +
                          s.md5);
  }
  return n;
}

}  // namespace irap_noroslib
