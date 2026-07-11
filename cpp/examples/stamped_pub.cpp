// Publish a CUSTOM message carrying a std_msgs/Header, stamped each cycle.
//   ./stamped_pub        (with a roscore running)
//   ./stamped_sub        (to see it)
#include "noros.hpp"
#include "sensor_reading.hpp"
#include <cstdlib>

int main() {
  // Point noros at the ROS master before init_node (defaults to a local roscore).
  const char* mu = std::getenv("ROS_MASTER_URI");
  const char* hn = std::getenv("ROS_HOSTNAME");
  noros::set_master_uri(mu ? mu : "http://localhost:11311");
  noros::set_hostname(hn ? hn : "localhost");
  noros::init_node("stamped_pub");
  noros::Publisher<SensorReading> pub("/sensor_reading");
  noros::Rate rate(5);
  uint32_t seq = 0;
  while (noros::ok()) {
    SensorReading m;
    m.header.seq = seq;              // ROS auto-fills this too
    m.header.stamp_now();            // fills stamp_sec / stamp_nsec
    m.header.frame_id = "sensor_link";
    m.temperature = 20.0 + seq * 0.1;
    m.humidity = 55.0;
    m.sensor_id = "dht22-0";
    pub.publish(m);
    noros::loginfo("published seq=" + std::to_string(seq) + " temp=" +
                   std::to_string(m.temperature) + " frame=" + m.header.frame_id);
    ++seq;
    rate.sleep();
  }
  noros::shutdown("done");
  return 0;
}
