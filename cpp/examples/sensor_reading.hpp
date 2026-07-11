// A CUSTOM message whose first field is a std_msgs/Header (ROS convention for
// stamped data). Compose the built-in std_msgs::Header via its write()/read().
#pragma once
#include "noros.hpp"
#include "noros.hpp"

struct SensorReading {
  static constexpr const char* TYPE = "noros_demo/SensorReading";
  // md5 of "<Header md5> header\nfloat64 temperature\nfloat64 humidity\nstring sensor_id"
  static constexpr const char* MD5 = "8de810f936b1126bb3a22807fcd428d5";
  static constexpr const char* DEFINITION =
      "std_msgs/Header header\nfloat64 temperature\nfloat64 humidity\nstring sensor_id\n"
      "================================================================================\n"
      "MSG: std_msgs/Header\nuint32 seq\ntime stamp\nstring frame_id\n";

  std_msgs::Header header;
  double temperature = 0;
  double humidity = 0;
  std::string sensor_id;

  std::vector<uint8_t> serialize() const {
    noros::Writer w;
    header.write(w);              // nested Header, ROS layout
    w.f64(temperature);
    w.f64(humidity);
    w.str(sensor_id);
    return w.b;
  }
  static SensorReading deserialize(const std::vector<uint8_t>& buf) {
    noros::Reader r(buf);
    SensorReading m;
    m.header = std_msgs::Header::read(r);
    m.temperature = r.f64();
    m.humidity = r.f64();
    m.sensor_id = r.str();
    return m;
  }
};
