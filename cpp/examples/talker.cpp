// noros talker — publish std_msgs/String at 10 Hz to a real roscore.
//   rostopic echo /chatter        # watch it from real ROS
#include "noros.hpp"
#include <cstdlib>

int main() {
  // Point noros at the ROS master before init_node (defaults to a local roscore).
  const char* mu = std::getenv("ROS_MASTER_URI");
  const char* hn = std::getenv("ROS_HOSTNAME");
  noros::set_master_uri(mu ? mu : "http://localhost:11311");
  noros::set_hostname(hn ? hn : "localhost");
  noros::init_node("noros_talker");
  noros::Publisher<std_msgs::String> pub("/chatter");
  noros::Rate rate(10);
  int i = 0;
  while (noros::ok()) {
    std_msgs::String m;
    m.data = "hello world " + std::to_string(i++);
    pub.publish(m);
    noros::loginfo("published: " + m.data);
    rate.sleep();
  }
  noros::shutdown("done");
  return 0;
}
