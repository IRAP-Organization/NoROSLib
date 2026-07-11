// Defining your OWN message type in noros C++.
//
// A noros message is any struct with TYPE / MD5 / DEFINITION and serialize() /
// deserialize(). Get the md5 from `rosmsg md5 <type>` (or let noros discover it
// from the publisher). Here we model geometry_msgs/Pose2D under our own name.
#include "noros.hpp"
#include "noros.hpp"

struct Pose2D {
  static constexpr const char* TYPE = "noros_demo/Pose2D";
  // md5 of "float64 x\nfloat64 y\nfloat64 theta" — same fields as
  // geometry_msgs/Pose2D, so `rosmsg md5 geometry_msgs/Pose2D` gives this too.
  static constexpr const char* MD5 = "938fa65709584ad8e77d238529be13b8";
  static constexpr const char* DEFINITION = "float64 x\nfloat64 y\nfloat64 theta\n";
  double x = 0, y = 0, theta = 0;
  std::vector<uint8_t> serialize() const {
    noros::Writer w; w.f64(x); w.f64(y); w.f64(theta); return w.b;
  }
  static Pose2D deserialize(const std::vector<uint8_t>& b) {
    noros::Reader r(b); Pose2D m; m.x = r.f64(); m.y = r.f64(); m.theta = r.f64(); return m;
  }
};

int main() {
  noros::init_node("noros_custom");
  noros::Publisher<Pose2D> pub("/pose2d");
  noros::Rate rate(5);
  double x = 0;
  while (noros::ok()) {
    Pose2D p; p.x = x; p.y = -x; p.theta = x / 10.0;
    pub.publish(p);
    noros::loginfo("published Pose2D x=" + std::to_string(x));
    x += 1.0;
    rate.sleep();
  }
  return 0;
}
