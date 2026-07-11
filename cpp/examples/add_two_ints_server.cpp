// noros service SERVER — answers rospy_tutorials/AddTwoInts on /add_two_ints.
//   rosservice call /add_two_ints "a: 3
//   b: 4"
#include "noros.hpp"
#include "add_two_ints.hpp"

int main() {
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
