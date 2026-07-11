// noros listener — subscribe to std_msgs/String from a real roscore.
//   rostopic pub -r 10 /chatter std_msgs/String "data: hi"
#include "noros.hpp"

int main() {
  noros::init_node("noros_listener");
  noros::Subscriber<std_msgs::String> sub("/chatter", [](const std_msgs::String& m) {
    noros::loginfo("I heard: " + m.data);
  });
  noros::spin();
  return 0;
}
