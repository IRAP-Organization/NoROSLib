// irap_noroslib parameter server usage (C++). Cross-check with `rosparam get/set/list`.
//   ./params_example        (with a roscore running)
//
// The C++ parameter API is scalar-typed (int / double / bool / string); the
// Python params_example additionally shows list/dict values, which the nested
// nr_roscore parameter tree also supports.
#include "irap_noroslib.hpp"
#include <cstdlib>

int main() {
  // Point irap_noroslib at the ROS master before init_node (defaults to a local roscore).
  const char* mu = std::getenv("ROS_MASTER_URI");
  const char* hn = std::getenv("ROS_HOSTNAME");
  irap_noroslib::set_master_uri(mu ? mu : "http://localhost:11311");
  irap_noroslib::set_hostname(hn ? hn : "localhost");
  irap_noroslib::init_node("params_example");

  irap_noroslib::set_param("/demo/rate", 30);
  irap_noroslib::set_param("/demo/name", "irap_noroslib");
  irap_noroslib::set_param("/demo/gain", 0.5);
  irap_noroslib::set_param("/demo/enabled", true);

  irap_noroslib::loginfo("rate    = " + std::to_string(irap_noroslib::get_param_or<int>("/demo/rate", 0)));
  irap_noroslib::loginfo("name    = " + irap_noroslib::get_param_or<std::string>("/demo/name", "?"));
  irap_noroslib::loginfo("gain    = " + std::to_string(irap_noroslib::get_param_or<double>("/demo/gain", 0.0)));
  irap_noroslib::loginfo(std::string("enabled = ") + (irap_noroslib::get_param_or<bool>("/demo/enabled", false) ? "true" : "false"));
  irap_noroslib::loginfo("missing (default) = " + irap_noroslib::get_param_or<std::string>("/demo/nope", "DEF"));
  irap_noroslib::loginfo(std::string("has /demo/rate = ") + (irap_noroslib::has_param("/demo/rate") ? "true" : "false"));
  irap_noroslib::delete_param("/demo/name");
  irap_noroslib::loginfo(std::string("after delete, has /demo/name = ") + (irap_noroslib::has_param("/demo/name") ? "true" : "false"));

  irap_noroslib::shutdown("done");
  return 0;
}
