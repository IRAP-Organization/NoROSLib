#!/usr/bin/env python3
"""Defining and using your OWN message type in noros.

A message is just its `.msg` text -- register it and noros derives the md5sum
(matching `rosmsg md5`) and the wire codec. Here we reproduce geometry_msgs/Pose2D
under our own package name and publish it. Because the md5 depends only on the
`.msg` text, a real ROS node subscribing to geometry_msgs/Pose2D would accept it.
"""
import os
import noros
from noros import define_message

# Define a custom message from its .msg text. Nesting other registered types
# (built-in or your own) works too -- just register the dependency first.
Pose2D = define_message("noros_demo/Pose2D", """
    float64 x
    float64 y
    float64 theta
""")


def main():
    # Point noros at the ROS master before init_node (defaults to a local roscore).
    noros.set_master_uri(os.environ.get("ROS_MASTER_URI", "http://localhost:11311"))
    noros.set_hostname(os.environ.get("ROS_HOSTNAME", "localhost"))
    noros.init_node("noros_custom")
    noros.loginfo("Pose2D md5sum = %s" % Pose2D.md5sum())
    pub = noros.Publisher("/pose2d", Pose2D)
    rate = noros.Rate(5)
    x = 0.0
    while not noros.is_shutdown():
        pub.publish(Pose2D(x=x, y=-x, theta=x / 10.0))
        noros.loginfo("published Pose2D x=%.1f" % x)
        x += 1.0
        rate.sleep()


if __name__ == "__main__":
    main()
