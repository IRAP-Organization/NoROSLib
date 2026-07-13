// Demonstrate irap_noroslib C++ automatic md5 discovery.
//
// We subscribe to a real ROS topic but deliberately present a WRONG md5sum via
// the low-level detail::subscribe API. The real ROS publisher rejects it with
// "... our version has [std_msgs/String/992ce8...]. Dropping connection."
// irap_noroslib parses the real md5 from that error, adopts it, reconnects, and the raw
// bodies flow — which we then decode as std_msgs/String.
//
//   rostopic pub -r 5 /disc std_msgs/String "data: real ros here"
//   ./md5_discovery
#include "irap_noroslib.hpp"
#include <cstdlib>

int main() {
  // Point irap_noroslib at the ROS master before init_node (defaults to a local roscore).
  const char* mu = std::getenv("ROS_MASTER_URI");
  const char* hn = std::getenv("ROS_HOSTNAME");
  irap_noroslib::set_master_uri(mu ? mu : "http://localhost:11311");
  irap_noroslib::set_hostname(hn ? hn : "localhost");
  irap_noroslib::init_node("irap_noroslib_md5_discovery");
  const char* WRONG_MD5 = "00000000000000000000000000000000";
  irap_noroslib::logwarn(std::string("subscribing to /disc with a deliberately WRONG md5: ") + WRONG_MD5);
  auto sub = irap_noroslib::detail::subscribe(
      "/disc", "std_msgs/String", WRONG_MD5,
      [](const std::vector<uint8_t>& body) {
        std_msgs::String m = std_msgs::String::deserialize(body);
        irap_noroslib::loginfo("RECOVERED and received: " + m.data);
      });
  irap_noroslib::spin();
  (void)sub;
  return 0;
}
