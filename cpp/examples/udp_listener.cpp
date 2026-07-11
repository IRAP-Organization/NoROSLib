// noros subscriber over UDPROS (unreliable UDP transport), C++.
//
// Feed it from a roscpp publisher (which offers UDPROS):
//     rosrun roscpp_tutorials talker
// Then: ./udp_listener
//
// Only the transport hint changes vs a normal Subscriber (3rd arg "udpros").
#include "noros.hpp"
#include <cstdlib>

int main() {
  // Point noros at the ROS master before init_node (defaults to a local roscore).
  const char* mu = std::getenv("ROS_MASTER_URI");
  const char* hn = std::getenv("ROS_HOSTNAME");
  noros::set_master_uri(mu ? mu : "http://localhost:11311");
  noros::set_hostname(hn ? hn : "localhost");
  noros::init_node("noros_udp_listener");

  noros::Subscriber<std_msgs::String> sub(
      "/chatter",
      [](const std_msgs::String& m) { noros::loginfo("UDPROS heard: " + m.data); },
      "udpros");                         // <-- UDP instead of TCP
  noros::spin();
  return 0;
}
