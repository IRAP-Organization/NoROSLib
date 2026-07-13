// irap_noroslib action SERVER — computes Fibonacci, publishing feedback then a result.
// Drive it with real ROS: rosrun actionlib_tutorials fibonacci_client
#include "irap_noroslib.hpp"
#include "fibonacci_action.hpp"
#include <thread>
#include <cstdlib>

int main() {
  // Point irap_noroslib at the ROS master before init_node (defaults to a local roscore).
  const char* mu = std::getenv("ROS_MASTER_URI");
  const char* hn = std::getenv("ROS_HOSTNAME");
  irap_noroslib::set_master_uri(mu ? mu : "http://localhost:11311");
  irap_noroslib::set_hostname(hn ? hn : "localhost");
  irap_noroslib::init_node("irap_noroslib_fib_server");
  irap_noroslib::SimpleActionServer<fib::Fibonacci> server(
      "/fibonacci", [](const fib::Goal& goal, irap_noroslib::SimpleActionServer<fib::Fibonacci>& s) {
        std::vector<int32_t> seq{0, 1};
        for (int i = 0; i < goal.order; ++i) {
          if (s.isPreemptRequested()) { s.setPreempted(); return; }
          seq.push_back(seq[seq.size() - 1] + seq[seq.size() - 2]);
          fib::Feedback fb; fb.sequence = seq;
          s.publishFeedback(fb);
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        fib::Result r; r.sequence = seq;
        s.setSucceeded(r);
        irap_noroslib::loginfo("served goal order=" + std::to_string(goal.order));
      });
  irap_noroslib::loginfo("irap_noroslib fibonacci action server ready");
  irap_noroslib::spin();
  return 0;
}
