#!/usr/bin/env python3
"""irap_noroslib action SERVER. Drive it with the irap_noroslib client OR real ROS:
    rosrun actionlib_tutorials fibonacci_client
"""
import os
import irap_noroslib
from irap_noroslib import define_action, SimpleActionServer

Fibonacci = define_action("actionlib_tutorials/Fibonacci",
                          "int32 order", "int32[] sequence", "int32[] sequence")


def execute(goal, server):
    seq = [0, 1]
    for _ in range(goal.order):
        if server.is_preempt_requested():
            server.set_preempted()
            return
        seq.append(seq[-1] + seq[-2])
        server.publish_feedback(Fibonacci.Feedback(sequence=list(seq)))
        irap_noroslib.sleep(0.1)
    server.set_succeeded(Fibonacci.Result(sequence=list(seq)))
    irap_noroslib.loginfo("served goal order=%d -> %d terms" % (goal.order, len(seq)))


def main():
    # Point irap_noroslib at the ROS master before init_node (defaults to a local roscore).
    irap_noroslib.set_master_uri(os.environ.get("ROS_MASTER_URI", "http://localhost:11311"))
    irap_noroslib.set_hostname(os.environ.get("ROS_HOSTNAME", "localhost"))
    irap_noroslib.init_node("irap_noroslib_fib_server")
    SimpleActionServer("/fibonacci", Fibonacci, execute)
    irap_noroslib.loginfo("fibonacci action server ready")
    irap_noroslib.spin()


if __name__ == "__main__":
    main()
