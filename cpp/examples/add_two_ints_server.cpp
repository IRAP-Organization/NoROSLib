// noros service SERVER — answers rospy_tutorials/AddTwoInts on /add_two_ints.
//   rosservice call /add_two_ints "a: 3
//   b: 4"
#include "noros.hpp"
#include "add_two_ints.hpp"
#include <cstdlib>

int main() {
  // Point noros at the ROS master before init_node (defaults to a local roscore).
  const char* mu = std::getenv("ROS_MASTER_URI");
  const char* hn = std::getenv("ROS_HOSTNAME");
  noros::set_master_uri(mu ? mu : "http://localhost:11311");
  noros::set_hostname(hn ? hn : "localhost");
  noros::init_node("add_two_ints_server");
  noros::ServiceServer<AddTwoInts> srv(
      "/add_two_ints", [](const AddTwoInts::Request& req, AddTwoInts::Response& resp) {
        noros::loginfo("request: " + std::to_string(req.a) + " + " + std::to_string(req.b));
        resp.sum = req.a + req.b;
        return true;
      });
  noros::loginfo("ready to add two ints");
  noros::spin();
  return 0;
}
