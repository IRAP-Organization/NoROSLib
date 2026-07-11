// Demonstrate noros C++ automatic md5 discovery.
//
// We subscribe to a real ROS topic but deliberately present a WRONG md5sum via
// the low-level detail::subscribe API. The real ROS publisher rejects it with
// "... our version has [std_msgs/String/992ce8...]. Dropping connection."
// noros parses the real md5 from that error, adopts it, reconnects, and the raw
// bodies flow — which we then decode as std_msgs/String.
//
//   rostopic pub -r 5 /disc std_msgs/String "data: real ros here"
//   ./md5_discovery
#include "noros.hpp"

int main() {
  noros::init_node("noros_md5_discovery");
  const char* WRONG_MD5 = "00000000000000000000000000000000";
  noros::logwarn(std::string("subscribing to /disc with a deliberately WRONG md5: ") + WRONG_MD5);
  auto sub = noros::detail::subscribe(
      "/disc", "std_msgs/String", WRONG_MD5,
      [](const std::vector<uint8_t>& body) {
        std_msgs::String m = std_msgs::String::deserialize(body);
        noros::loginfo("RECOVERED and received: " + m.data);
      });
  noros::spin();
  (void)sub;
  return 0;
}
