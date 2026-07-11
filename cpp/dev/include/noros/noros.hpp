// noros.hpp — umbrella include for the noros C++ library.
//
// noros is a ROS pub/sub client library that speaks the real ROS wire protocols
// (XML-RPC master/slave + TCPROS) directly, so a real roscore and real ROS nodes
// treat it as a legitimate node — with NO ROS libraries linked. It ships
// std_msgs/geometry_msgs/sensor_msgs, lets you add your own message type, and
// automatically discovers a publisher's real md5sum from the mismatch error.
#pragma once

#include "noros/node.hpp"
#include "noros/std_msgs.hpp"
#include "noros/geometry_msgs.hpp"
#include "noros/sensor_msgs.hpp"
#include "noros/nav_msgs.hpp"
#include "noros/diagnostic_msgs.hpp"
#include "noros/trajectory_msgs.hpp"
#include "noros/actionlib_msgs.hpp"
