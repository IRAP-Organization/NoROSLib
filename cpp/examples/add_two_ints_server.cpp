// irap_noroslib service SERVER — answers rospy_tutorials/AddTwoInts on /add_two_ints.
//   rosservice call /add_two_ints "a: 3
//   b: 4"
#include "irap_noroslib.hpp"
#include "add_two_ints.hpp"
#include <cstdlib>

int main() {
  // Point irap_noroslib at the ROS master before init_node (defaults to a local roscore).
  const char* mu = std::getenv("ROS_MASTER_URI");
  const char* hn = std::getenv("ROS_HOSTNAME");
  irap_noroslib::set_master_uri(mu ? mu : "http://localhost:11311");
  irap_noroslib::set_hostname(hn ? hn : "localhost");
  irap_noroslib::init_node("add_two_ints_server");
  irap_noroslib::ServiceServer<AddTwoInts> srv(
      "/add_two_ints", [](const AddTwoInts::Request& req, AddTwoInts::Response& resp) {
        irap_noroslib::loginfo("request: " + std::to_string(req.a) + " + " + std::to_string(req.b));
        resp.sum = req.a + req.b;
        return true;
      });
  irap_noroslib::loginfo("ready to add two ints");
  irap_noroslib::spin();
  return 0;
}
