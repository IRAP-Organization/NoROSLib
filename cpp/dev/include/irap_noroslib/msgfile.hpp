// msgfile.hpp -- load ROS `.msg` / `.srv` / `.action` FILES at runtime.
//
// Copy a definition off a real ROS robot and hand irap_noroslib its full path --
// no catkin package, no ROS install, just the file:
//
//   MsgType CustomData = load_msg_file("/home/me/msgs/CustomData.msg", "my_robot_msgs");
//
//   DynamicPublisher pub("/data", CustomData);
//   DynamicMessage m = CustomData.create();
//   m.set("id", 7).set("label", "hi");
//   pub.publish(m);
//
// The md5sum and the wire codec come from the file, so the type is exactly what
// real ROS computes (`rosmsg md5`) and real ROS nodes accept it.
//
// ONE FILE, ONE CALL. Several custom messages means several calls, each with its
// own path. Order doesn't matter -- a nested type is resolved when it is first
// used, and if you forgot its file the error names the one to load.
//
// The package name is the "my_robot_msgs" in "my_robot_msgs/CustomData"; ROS
// identifies types by that full name, so pass the package the message came from.
// It is inferred only when the file still sits in a <pkg>/msg/<Type>.msg layout.
#pragma once

#include <string>
#include <vector>

#include "irap_noroslib/dynmsg.hpp"

namespace irap_noroslib {

/// A runtime-loaded service type.
class SrvType {
 public:
  SrvType() = default;
  SrvType(std::string type, std::string md5, MsgType req, MsgType resp)
      : type_(std::move(type)), md5_(std::move(md5)),
        req_(std::move(req)), resp_(std::move(resp)) {}

  bool valid() const { return req_.valid(); }
  const std::string& type() const { return type_; }
  const std::string& md5() const { return md5_; }        // md5(req.md5_text + resp.md5_text)
  const MsgType& request() const { return req_; }
  const MsgType& response() const { return resp_; }

 private:
  std::string type_, md5_;
  MsgType req_, resp_;
};

/// A runtime-loaded action type: the 7 message types ROS generates from a .action.
class ActionType {
 public:
  ActionType() = default;
  ActionType(std::string type, MsgType goal, MsgType result, MsgType feedback,
             MsgType ag, MsgType ar, MsgType af, MsgType action)
      : type_(std::move(type)), goal_(std::move(goal)), result_(std::move(result)),
        feedback_(std::move(feedback)), action_goal_(std::move(ag)),
        action_result_(std::move(ar)), action_feedback_(std::move(af)),
        action_(std::move(action)) {}

  bool valid() const { return goal_.valid(); }
  const std::string& type() const { return type_; }
  const MsgType& goal() const { return goal_; }
  const MsgType& result() const { return result_; }
  const MsgType& feedback() const { return feedback_; }
  const MsgType& action_goal() const { return action_goal_; }
  const MsgType& action_result() const { return action_result_; }
  const MsgType& action_feedback() const { return action_feedback_; }
  const MsgType& action() const { return action_; }

 private:
  std::string type_;
  MsgType goal_, result_, feedback_, action_goal_, action_result_, action_feedback_, action_;
};

// -- register from text (no file involved) -----------------------------------
MsgType register_msg(const std::string& full_type, const std::string& text);

/// Build a type from a ROS `message_definition` -- the type's own text followed by
/// every dependency, each behind a `====` / `MSG: pkg/Type` separator.
///
/// This is what lets us decode a topic we have no `.msg` file for: a ROS publisher
/// hands over the full definition in the TCPROS handshake, so the type can be
/// reconstructed on the spot. Types already known are kept as they are.
MsgType register_msg_from_definition(const std::string& full_type,
                                     const std::string& definition);
SrvType register_srv(const std::string& full_type, const std::string& request_text,
                     const std::string& response_text);
ActionType register_action(const std::string& full_type, const std::string& goal_text,
                           const std::string& result_text,
                           const std::string& feedback_text);

// -- load from a file, by full path ------------------------------------------
/// `pkg` is the ROS package the message came from. It may be omitted only if the
/// file still sits in a <pkg>/msg/<Type>.msg (or srv/, action/) layout.
MsgType load_msg_file(const std::string& path, const std::string& pkg = "");
SrvType load_srv_file(const std::string& path, const std::string& pkg = "");
ActionType load_action_file(const std::string& path, const std::string& pkg = "");

/// Several .msg files from one package, in one call.
std::vector<MsgType> load_msg_files(const std::vector<std::string>& paths,
                                    const std::string& pkg = "");

// -- lookup ------------------------------------------------------------------
MsgType get_msg_type(const std::string& full_type);      // throws if not registered
bool has_msg_type(const std::string& full_type);

/// A service type reconstructed from its registered Request/Response messages --
/// so a service type discovered by probing (which yields only the name) can be
/// turned back into request/response codecs, IF the .srv was seeded (std_srvs)
/// or loaded with load_srv_file(). get_srv_type throws if unknown.
SrvType get_srv_type(const std::string& full_type);
bool has_srv_type(const std::string& full_type);

/// Self-check with no ROS anywhere: recompute every built-in type's md5 from its
/// own DEFINITION text and compare it with the hardcoded MD5 constant. Exercises
/// the MD5 code, the .msg parser and the gentools md5 rules in one shot.
/// Returns the number of types checked; *failures* lists any that disagree.
int selftest_builtin_md5(std::vector<std::string>* failures);

}  // namespace irap_noroslib
