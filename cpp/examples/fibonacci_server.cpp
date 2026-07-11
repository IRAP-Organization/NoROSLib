// noros action SERVER — computes Fibonacci, publishing feedback then a result.
// Drive it with real ROS: rosrun actionlib_tutorials fibonacci_client
#include "noros.hpp"
#include "noros.hpp"
#include "fibonacci_action.hpp"
#include <thread>

int main() {
  noros::init_node("noros_fib_server");
  noros::SimpleActionServer<fib::Fibonacci> server(
      "/fibonacci", [](const fib::Goal& goal, noros::SimpleActionServer<fib::Fibonacci>& s) {
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
        noros::loginfo("served goal order=" + std::to_string(goal.order));
      });
  noros::loginfo("noros fibonacci action server ready");
  noros::spin();
  return 0;
}
