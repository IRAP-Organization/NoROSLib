#!/usr/bin/env python3
"""noros action CLIENT. Works against the noros server (fibonacci_server.py) OR
real ROS: rosrun actionlib_tutorials fibonacci_server

    python3 fibonacci_client.py 10
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))  # locate the noros package (../)
import noros
from noros import define_action, SimpleActionClient

Fibonacci = define_action("actionlib_tutorials/Fibonacci",
                          "int32 order", "int32[] sequence", "int32[] sequence")


def main():
    order = int(sys.argv[1]) if len(sys.argv) > 1 else 8
    # Point noros at the ROS master before init_node (defaults to a local roscore).
    noros.set_master_uri(os.environ.get("ROS_MASTER_URI", "http://localhost:11311"))
    noros.set_hostname(os.environ.get("ROS_HOSTNAME", "localhost"))
    noros.init_node("noros_fib_client")
    client = SimpleActionClient("/fibonacci", Fibonacci)
    if not client.wait_for_server(timeout=8.0):
        noros.logerr("action server not available")
        return
    client.send_goal(Fibonacci.Goal(order=order),
                     feedback_cb=lambda fb: noros.loginfo("feedback: %r" % list(fb.sequence)))
    client.wait_for_result(timeout=20.0)
    noros.loginfo("state=%s (3=SUCCEEDED) result=%r"
                  % (client.get_state(), list(client.get_result().sequence)))


if __name__ == "__main__":
    main()
