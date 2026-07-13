// irap_noroslib talker — publish std_msgs/String at 10 Hz to a real roscore.
//   rostopic echo /chatter        # watch it from real ROS
#include "irap_noroslib.hpp"
#include <cstdlib>

int main() {
  // Point irap_noroslib at the ROS master before init_node (defaults to a local roscore).
  const char* mu = std::getenv("ROS_MASTER_URI");
  const char* hn = std::getenv("ROS_HOSTNAME");
  irap_noroslib::set_master_uri(mu ? mu : "http://localhost:11311");
  irap_noroslib::set_hostname(hn ? hn : "localhost");
  irap_noroslib::init_node("irap_noroslib_talker");
  irap_noroslib::Publisher<std_msgs::String> pub("/chatter");
  irap_noroslib::Rate rate(10);
  int i = 0;
  while (irap_noroslib::ok()) {
    std_msgs::String m;
    m.data = "hello world " + std::to_string(i++);
    pub.publish(m);
    irap_noroslib::loginfo("published: " + m.data);
    rate.sleep();
  }
  irap_noroslib::shutdown("done");
  return 0;
}
