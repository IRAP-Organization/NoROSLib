// irap_noroslib.hpp — umbrella include for the irap_noroslib C++ library.
//
// irap_noroslib is a ROS pub/sub client library that speaks the real ROS wire protocols
// (XML-RPC master/slave + TCPROS) directly, so a real roscore and real ROS nodes
// treat it as a legitimate node — with NO ROS libraries linked. It ships
// std_msgs/geometry_msgs/sensor_msgs, lets you add your own message type, and
// automatically discovers a publisher's real md5sum from the mismatch error.
#pragma once

#include "irap_noroslib/platform.hpp"   // sockets/threads compat + wall_time + fs
#include "irap_noroslib/node.hpp"
#include "irap_noroslib/std_msgs.hpp"
#include "irap_noroslib/geometry_msgs.hpp"
#include "irap_noroslib/sensor_msgs.hpp"
#include "irap_noroslib/nav_msgs.hpp"
#include "irap_noroslib/diagnostic_msgs.hpp"
#include "irap_noroslib/trajectory_msgs.hpp"
#include "irap_noroslib/actionlib_msgs.hpp"
#include "irap_noroslib/std_srvs.hpp"
#include "irap_noroslib/actionlib.hpp"
// master queries: get_topic_types / get_system_state (what nr_rostopic runs on)
#include "irap_noroslib/xmlrpc_client.hpp"
// runtime .msg/.srv/.action file loading
#include "irap_noroslib/msgfile.hpp"
#include "irap_noroslib/dynamic_node.hpp"
#include "irap_noroslib/dynamic_actionlib.hpp"
