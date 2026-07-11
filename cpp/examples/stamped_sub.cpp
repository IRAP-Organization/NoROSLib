// Subscribe to the custom Header-bearing message and print the Header fields.
//   ./stamped_sub        (with a roscore + ./stamped_pub running)
#include "noros.hpp"
#include "sensor_reading.hpp"

int main() {
  noros::init_node("stamped_sub");
  noros::Subscriber<SensorReading> sub("/sensor_reading", [](const SensorReading& m) {
    char buf[256];
    std::snprintf(buf, sizeof(buf),
                  "seq=%u stamp=%u.%09u frame=%s | %s temp=%.1f hum=%.0f",
                  m.header.seq, m.header.stamp_sec, m.header.stamp_nsec,
                  m.header.frame_id.c_str(), m.sensor_id.c_str(),
                  m.temperature, m.humidity);
    noros::loginfo(buf);
  });
  noros::spin();
  return 0;
}
