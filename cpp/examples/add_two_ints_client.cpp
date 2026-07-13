// irap_noroslib service CLIENT — calls /add_two_ints (rospy_tutorials/AddTwoInts).
// Works against the irap_noroslib server OR a real ROS server
// (rosrun rospy_tutorials add_two_ints_server).
//   ./add_two_ints_client 3 4     ->  3 + 4 = 7
#include "irap_noroslib.hpp"
#include "add_two_ints.hpp"
#include <cstdlib>

int main(int argc, char** argv) {
  int64_t a = argc > 1 ? std::atoll(argv[1]) : 3;
  int64_t b = argc > 2 ? std::atoll(argv[2]) : 4;
  // Point irap_noroslib at the ROS master before init_node (defaults to a local roscore).
  const char* mu = std::getenv("ROS_MASTER_URI");
  const char* hn = std::getenv("ROS_HOSTNAME");
  irap_noroslib::set_master_uri(mu ? mu : "http://localhost:11311");
  irap_noroslib::set_hostname(hn ? hn : "localhost");
  irap_noroslib::init_node("add_two_ints_client");
  irap_noroslib::ServiceClient<AddTwoInts> client("/add_two_ints");
  if (!client.waitForExistence(5.0)) { irap_noroslib::logerr("service not available"); return 1; }
  AddTwoInts::Request req; req.a = a; req.b = b;
  AddTwoInts::Response resp;
  if (!client.call(req, resp)) { irap_noroslib::logerr("call failed"); return 1; }
  irap_noroslib::loginfo(std::to_string(a) + " + " + std::to_string(b) + " = " + std::to_string(resp.sum));
  irap_noroslib::shutdown("done");
  return 0;
}
