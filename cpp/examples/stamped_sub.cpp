// Subscribe to the custom Header-bearing message and print the Header fields.
//   ./stamped_sub        (with a roscore + ./stamped_pub running)
#include "irap_noroslib.hpp"
#include "sensor_reading.hpp"
#include <cstdlib>

int main() {
  // Point irap_noroslib at the ROS master before init_node (defaults to a local roscore).
  const char* mu = std::getenv("ROS_MASTER_URI");
  const char* hn = std::getenv("ROS_HOSTNAME");
  irap_noroslib::set_master_uri(mu ? mu : "http://localhost:11311");
  irap_noroslib::set_hostname(hn ? hn : "localhost");
  irap_noroslib::init_node("stamped_sub");
  irap_noroslib::Subscriber<SensorReading> sub("/sensor_reading", [](const SensorReading& m) {
    char buf[256];
    std::snprintf(buf, sizeof(buf),
                  "seq=%u stamp=%u.%09u frame=%s | %s temp=%.1f hum=%.0f",
                  m.header.seq, m.header.stamp_sec, m.header.stamp_nsec,
                  m.header.frame_id.c_str(), m.sensor_id.c_str(),
                  m.temperature, m.humidity);
    irap_noroslib::loginfo(buf);
  });
  irap_noroslib::spin();
  return 0;
}
