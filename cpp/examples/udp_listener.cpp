// irap_noroslib subscriber over UDPROS (unreliable UDP transport), C++.
//
// Feed it from a roscpp publisher (which offers UDPROS):
//     rosrun roscpp_tutorials talker
// Then: ./udp_listener
//
// Only the transport hint changes vs a normal Subscriber (3rd arg "udpros").
#include "irap_noroslib.hpp"
#include <cstdlib>

int main() {
  // Point irap_noroslib at the ROS master before init_node (defaults to a local roscore).
  const char* mu = std::getenv("ROS_MASTER_URI");
  const char* hn = std::getenv("ROS_HOSTNAME");
  irap_noroslib::set_master_uri(mu ? mu : "http://localhost:11311");
  irap_noroslib::set_hostname(hn ? hn : "localhost");
  irap_noroslib::init_node("irap_noroslib_udp_listener");

  irap_noroslib::Subscriber<std_msgs::String> sub(
      "/chatter",
      [](const std_msgs::String& m) { irap_noroslib::loginfo("UDPROS heard: " + m.data); },
      "udpros");                         // <-- UDP instead of TCP
  irap_noroslib::spin();
  return 0;
}
