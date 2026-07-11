// noros service CLIENT — calls /add_two_ints (rospy_tutorials/AddTwoInts).
// Works against the noros server OR a real ROS server
// (rosrun rospy_tutorials add_two_ints_server).
//   ./add_two_ints_client 3 4     ->  3 + 4 = 7
#include "noros.hpp"
#include "add_two_ints.hpp"
#include <cstdlib>

int main(int argc, char** argv) {
  int64_t a = argc > 1 ? std::atoll(argv[1]) : 3;
  int64_t b = argc > 2 ? std::atoll(argv[2]) : 4;
  // Point noros at the ROS master before init_node (defaults to a local roscore).
  const char* mu = std::getenv("ROS_MASTER_URI");
  const char* hn = std::getenv("ROS_HOSTNAME");
  noros::set_master_uri(mu ? mu : "http://localhost:11311");
  noros::set_hostname(hn ? hn : "localhost");
  noros::init_node("add_two_ints_client");
  noros::ServiceClient<AddTwoInts> client("/add_two_ints");
  if (!client.waitForExistence(5.0)) { noros::logerr("service not available"); return 1; }
  AddTwoInts::Request req; req.a = a; req.b = b;
  AddTwoInts::Response resp;
  if (!client.call(req, resp)) { noros::logerr("call failed"); return 1; }
  noros::loginfo(std::to_string(a) + " + " + std::to_string(b) + " = " + std::to_string(resp.sum));
  noros::shutdown("done");
  return 0;
}
