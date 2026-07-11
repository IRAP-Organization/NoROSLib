// noros listener — subscribe to std_msgs/String from a real roscore.
//   rostopic pub -r 10 /chatter std_msgs/String "data: hi"
#include "noros.hpp"
#include <cstdlib>

int main() {
  // Point noros at the ROS master before init_node (defaults to a local roscore).
  const char* mu = std::getenv("ROS_MASTER_URI");
  const char* hn = std::getenv("ROS_HOSTNAME");
  noros::set_master_uri(mu ? mu : "http://localhost:11311");
  noros::set_hostname(hn ? hn : "localhost");
  noros::init_node("noros_listener");
  noros::Subscriber<std_msgs::String> sub("/chatter", [](const std_msgs::String& m) {
    noros::loginfo("I heard: " + m.data);
  });
  noros::spin();
  return 0;
}
