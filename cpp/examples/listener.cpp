// irap_noroslib listener — subscribe to std_msgs/String from a real roscore.
//   rostopic pub -r 10 /chatter std_msgs/String "data: hi"
#include "irap_noroslib.hpp"
#include "irap_noroslib/std_msgs/String.h"
#include <cstdlib>

int main() {
  // Point irap_noroslib at the ROS master before init_node (defaults to a local roscore).
  const char* mu = std::getenv("ROS_MASTER_URI");
  const char* hn = std::getenv("ROS_HOSTNAME");
  irap_noroslib::set_master_uri(mu ? mu : "http://localhost:11311");
  irap_noroslib::set_hostname(hn ? hn : "localhost");
  irap_noroslib::init_node("irap_noroslib_listener");
  irap_noroslib::Subscriber<std_msgs::String> sub("/chatter", [](const std_msgs::String& m) {
    irap_noroslib::loginfo("I heard: " + m.data);
  });
  irap_noroslib::spin();
  return 0;
}
