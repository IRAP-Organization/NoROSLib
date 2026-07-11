// noros C++ subscriber decoding the custom message published by REAL ROS (rospy).
#include "noros.hpp"
#include <atomic>
#include <chrono>
#include <thread>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <vector>

struct CustomData {
  static constexpr const char* TYPE = "noros_stress_msgs/CustomData";
  static constexpr const char* MD5 = "90f507711f5fc7a674b7527eafdf210d";
  static constexpr const char* DEFINITION = "";  // only needed for advertising
  std_msgs::Header header;
  int32_t id = 0;
  std::vector<double> samples;
  std::string label;
  std::vector<uint8_t> blob;
  geometry_msgs::Point location;
  bool valid = false;
  std::vector<uint8_t> serialize() const { return {}; }
  static CustomData deserialize(const std::vector<uint8_t>& b) {
    noros::Reader r(b);
    CustomData m;
    m.header = std_msgs::Header::read(r);
    m.id = r.i32();
    uint32_t n = r.u32();
    for (uint32_t i = 0; i < n; ++i) m.samples.push_back(r.f64());
    m.label = r.str();
    uint32_t bn = r.u32();
    for (uint32_t i = 0; i < bn; ++i) m.blob.push_back(r.u8());
    m.location = geometry_msgs::Point::read(r);
    m.valid = r.u8() != 0;
    return m;
  }
};

int main() {
  const char* mu = std::getenv("ROS_MASTER_URI");
  noros::set_master_uri(mu ? mu : "http://localhost:11311");
  noros::set_hostname("localhost");
  noros::init_node("noros_custom_sub_cpp");

  std::atomic<bool> ok{false};
  noros::Subscriber<CustomData> sub("/stress/realros/CustomData", [&](const CustomData& m) {
    bool good = (m.id == 4242 && m.label == "from_real_ros" && m.valid &&
                 m.samples.size() == 3 && m.blob.size() == 3 &&
                 m.location.x == 1.0 && m.header.frame_id == "real_ros");
    std::printf("noros C++ decoded real-ROS custom: id=%d label=%s valid=%d samples=%zu blob=%zu loc.x=%.1f frame=%s\n",
                m.id, m.label.c_str(), (int)m.valid, m.samples.size(), m.blob.size(),
                m.location.x, m.header.frame_id.c_str());
    if (good) ok = true;
  });

  for (int i = 0; i < 80 && !ok; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(100));
  std::printf(ok ? "PASS\n" : "FAIL\n");
  return ok ? 0 : 1;
}
