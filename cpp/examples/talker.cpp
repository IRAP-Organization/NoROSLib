// noros talker — publish std_msgs/String at 10 Hz to a real roscore.
//   rostopic echo /chatter        # watch it from real ROS
#include "noros.hpp"

int main() {
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
