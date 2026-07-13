#!/usr/bin/env python3
"""irap_noroslib action CLIENT. Works against the irap_noroslib server (fibonacci_server.py) OR
real ROS: rosrun actionlib_tutorials fibonacci_server

    python3 fibonacci_client.py 10
"""
import os, sys
import irap_noroslib
from irap_noroslib import define_action, SimpleActionClient

Fibonacci = define_action("actionlib_tutorials/Fibonacci",
                          "int32 order", "int32[] sequence", "int32[] sequence")


def main():
    order = int(sys.argv[1]) if len(sys.argv) > 1 else 8
    # Point irap_noroslib at the ROS master before init_node (defaults to a local roscore).
    irap_noroslib.set_master_uri(os.environ.get("ROS_MASTER_URI", "http://localhost:11311"))
    irap_noroslib.set_hostname(os.environ.get("ROS_HOSTNAME", "localhost"))
    irap_noroslib.init_node("irap_noroslib_fib_client")
    client = SimpleActionClient("/fibonacci", Fibonacci)
    if not client.wait_for_server(timeout=8.0):
        irap_noroslib.logerr("action server not available")
        return
    client.send_goal(Fibonacci.Goal(order=order),
                     feedback_cb=lambda fb: irap_noroslib.loginfo("feedback: %r" % list(fb.sequence)))
    client.wait_for_result(timeout=20.0)
    irap_noroslib.loginfo("state=%s (3=SUCCEEDED) result=%r"
                  % (client.get_state(), list(client.get_result().sequence)))


if __name__ == "__main__":
    main()
