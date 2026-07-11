// noros parameter server usage (C++). Cross-check with `rosparam get/set/list`.
//   ./params_example        (with a roscore running)
//
// The C++ parameter API is scalar-typed (int / double / bool / string); the
// Python params_example additionally shows list/dict values, which the nested
// nr_roscore parameter tree also supports.
#include "noros.hpp"
#include <cstdlib>

int main() {
  // Point noros at the ROS master before init_node (defaults to a local roscore).
  const char* mu = std::getenv("ROS_MASTER_URI");
  const char* hn = std::getenv("ROS_HOSTNAME");
  noros::set_master_uri(mu ? mu : "http://localhost:11311");
  noros::set_hostname(hn ? hn : "localhost");
  noros::init_node("params_example");

  noros::set_param("/demo/rate", 30);
  noros::set_param("/demo/name", "noros");
  noros::set_param("/demo/gain", 0.5);
  noros::set_param("/demo/enabled", true);

  noros::loginfo("rate    = " + std::to_string(noros::get_param_or<int>("/demo/rate", 0)));
  noros::loginfo("name    = " + noros::get_param_or<std::string>("/demo/name", "?"));
  noros::loginfo("gain    = " + std::to_string(noros::get_param_or<double>("/demo/gain", 0.0)));
  noros::loginfo(std::string("enabled = ") + (noros::get_param_or<bool>("/demo/enabled", false) ? "true" : "false"));
  noros::loginfo("missing (default) = " + noros::get_param_or<std::string>("/demo/nope", "DEF"));
  noros::loginfo(std::string("has /demo/rate = ") + (noros::has_param("/demo/rate") ? "true" : "false"));
  noros::delete_param("/demo/name");
  noros::loginfo(std::string("after delete, has /demo/name = ") + (noros::has_param("/demo/name") ? "true" : "false"));

  noros::shutdown("done");
  return 0;
}
