// Defining and using your OWN message type in irap_noroslib C++ -- two ways.
//
//   1. As a STRUCT, at compile time   -> TYPE/MD5/DEFINITION + serialize/deserialize
//   2. From a `.msg` FILE, at runtime -> load_msg_file(...) + DynamicPublisher
//
// Way 1 is fully typed and zero-overhead, but you must supply the md5 yourself
// (`rosmsg md5 <type>`, or let irap_noroslib discover it from the publisher).
//
// Way 2 is what you want when you already HAVE the file -- e.g. you copied it off
// a robot. irap_noroslib parses it, derives the md5 the same way ROS does, and gives
// you a message whose fields you address by name. No catkin package needed, no ROS
// installed: just the file's full path plus the ROS package it came from (ROS names
// types "pkg/Type", so it needs the "pkg"). Several custom messages? One call each.
//
// Watch it from real ROS:
//     rostopic echo /pose2d       // works: same md5 as geometry_msgs/Pose2D
//     rostopic info /reading      // shows the type + md5 we advertised
//
// `rostopic echo /reading` will say "Cannot load message class" -- that is ROS
// being ROS, not a irap_noroslib problem: echo needs the message class BUILT in a
// catkin package, and "irap_noroslib_demo" is a made-up package that exists only
// here. Point this at a real package's .msg (one catkin built) and echo decodes it.
#include "irap_noroslib.hpp"
#include <cstdlib>
#include <string>

// -- 1. as a compile-time struct ---------------------------------------------
struct Pose2D {
  static constexpr const char* TYPE = "irap_noroslib_demo/Pose2D";
  // md5 of "float64 x\nfloat64 y\nfloat64 theta" — same fields as
  // geometry_msgs/Pose2D, so `rosmsg md5 geometry_msgs/Pose2D` gives this too.
  static constexpr const char* MD5 = "938fa65709584ad8e77d238529be13b8";
  static constexpr const char* DEFINITION = "float64 x\nfloat64 y\nfloat64 theta\n";
  double x = 0, y = 0, theta = 0;
  std::vector<uint8_t> serialize() const {
    irap_noroslib::Writer w; w.f64(x); w.f64(y); w.f64(theta); return w.b;
  }
  static Pose2D deserialize(const std::vector<uint8_t>& b) {
    irap_noroslib::Reader r(b); Pose2D m; m.x = r.f64(); m.y = r.f64(); m.theta = r.f64(); return m;
  }
};

int main(int argc, char** argv) {
  // Point irap_noroslib at the ROS master before init_node (defaults to a local roscore).
  const char* mu = std::getenv("ROS_MASTER_URI");
  const char* hn = std::getenv("ROS_HOSTNAME");
  irap_noroslib::set_master_uri(mu ? mu : "http://localhost:11311");
  irap_noroslib::set_hostname(hn ? hn : "localhost");

  // -- 2. from a .msg FILE, at runtime ---------------------------------------
  // examples/msgs/Reading.msg is a loose file -- no package.xml, no msg/ dir. The
  // second argument is the ROS package it came from. It nests std_msgs/Header and
  // geometry_msgs/Point; those are built in, so they resolve with no extra work.
  std::string dir = argc > 1 ? argv[1] : "examples/msgs";
  irap_noroslib::MsgType Reading =
      irap_noroslib::load_msg_file(dir + "/Reading.msg", "irap_noroslib_demo");

  irap_noroslib::init_node("irap_noroslib_custom");
  irap_noroslib::loginfo("Pose2D  (struct)    md5sum = " + std::string(Pose2D::MD5));
  irap_noroslib::loginfo("Reading (from file) md5sum = " + Reading.md5());

  irap_noroslib::Publisher<Pose2D> pose_pub("/pose2d");        // compile-time type
  irap_noroslib::DynamicPublisher reading_pub("/reading", Reading);  // runtime type

  irap_noroslib::Rate rate(5);
  double x = 0;
  while (irap_noroslib::ok()) {
    Pose2D p;
    p.x = x; p.y = -x; p.theta = x / 10.0;
    pose_pub.publish(p);

    // A runtime message: fields by name, nested ones with a dotted path.
    irap_noroslib::DynamicMessage r = Reading.create();
    r.set("value", x / 2.0);
    r.set("unit", "C");
    r.set("header.frame_id", "sensor_link");
    r.set("where.x", x);
    reading_pub.publish(r);

    irap_noroslib::loginfo("published Pose2D x=" + std::to_string(x) +
                           " and Reading value=" + std::to_string(x / 2.0) + "C");
    x += 1.0;
    rate.sleep();
  }
  return 0;
}
