// irap_noroslib action CLIENT — sends a Fibonacci goal, prints feedback + result.
// Works against the irap_noroslib server OR real ROS: rosrun actionlib_tutorials fibonacci_server
#include "irap_noroslib.hpp"
#include "irap_noroslib.hpp"
#include "fibonacci_action.hpp"
#include <cstdio>
#include <cstdlib>

int main(int argc, char** argv) {
  int order = argc > 1 ? std::atoi(argv[1]) : 8;
  // Point irap_noroslib at the ROS master before init_node (defaults to a local roscore).
  const char* mu = std::getenv("ROS_MASTER_URI");
  const char* hn = std::getenv("ROS_HOSTNAME");
  irap_noroslib::set_master_uri(mu ? mu : "http://localhost:11311");
  irap_noroslib::set_hostname(hn ? hn : "localhost");
  irap_noroslib::init_node("irap_noroslib_fib_client");
  irap_noroslib::SimpleActionClient<fib::Fibonacci> client("/fibonacci");
  irap_noroslib::loginfo("waiting for action server...");
  if (!client.waitForServer(8.0)) { irap_noroslib::logerr("no server"); return 1; }

  fib::Goal goal; goal.order = order;
  int fb = 0;
  client.sendGoal(goal, [&](const fib::Feedback& f) { ++fb; (void)f; });
  if (!client.waitForResult(15.0)) { irap_noroslib::logerr("timed out"); return 1; }

  fib::Result r = client.getResult();
  std::printf("state=%d (3=SUCCEEDED)  feedbacks=%d  result:", client.getState(), fb);
  for (int32_t x : r.sequence) std::printf(" %d", x);
  std::printf("\n");
  irap_noroslib::shutdown("done");
  return 0;
}
