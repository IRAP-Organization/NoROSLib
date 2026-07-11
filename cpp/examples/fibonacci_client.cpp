// noros action CLIENT — sends a Fibonacci goal, prints feedback + result.
// Works against the noros server OR real ROS: rosrun actionlib_tutorials fibonacci_server
#include "noros.hpp"
#include "noros.hpp"
#include "fibonacci_action.hpp"
#include <cstdio>
#include <cstdlib>

int main(int argc, char** argv) {
  int order = argc > 1 ? std::atoi(argv[1]) : 8;
  // Point noros at the ROS master before init_node (defaults to a local roscore).
  const char* mu = std::getenv("ROS_MASTER_URI");
  const char* hn = std::getenv("ROS_HOSTNAME");
  noros::set_master_uri(mu ? mu : "http://localhost:11311");
  noros::set_hostname(hn ? hn : "localhost");
  noros::init_node("noros_fib_client");
  noros::SimpleActionClient<fib::Fibonacci> client("/fibonacci");
  noros::loginfo("waiting for action server...");
  if (!client.waitForServer(8.0)) { noros::logerr("no server"); return 1; }

  fib::Goal goal; goal.order = order;
  int fb = 0;
  client.sendGoal(goal, [&](const fib::Feedback& f) { ++fb; (void)f; });
  if (!client.waitForResult(15.0)) { noros::logerr("timed out"); return 1; }

  fib::Result r = client.getResult();
  std::printf("state=%d (3=SUCCEEDED)  feedbacks=%d  result:", client.getState(), fb);
  for (int32_t x : r.sequence) std::printf(" %d", x);
  std::printf("\n");
  noros::shutdown("done");
  return 0;
}
