#!/usr/bin/env python3
"""noros action SERVER. Drive it with the noros client OR real ROS:
    rosrun actionlib_tutorials fibonacci_client
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))  # locate the noros package (../)
import noros
from noros import define_action, SimpleActionServer

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
        noros.sleep(0.1)
    server.set_succeeded(Fibonacci.Result(sequence=list(seq)))
    noros.loginfo("served goal order=%d -> %d terms" % (goal.order, len(seq)))


def main():
    noros.init_node("noros_fib_server")
    SimpleActionServer("/fibonacci", Fibonacci, execute)
    noros.loginfo("fibonacci action server ready")
    noros.spin()


if __name__ == "__main__":
    main()
