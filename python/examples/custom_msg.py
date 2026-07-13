#!/usr/bin/env python3
"""Defining and using your OWN message type in irap_noroslib -- two ways.

  1. From `.msg` TEXT, in code            -> define_message(...)
  2. From a `.msg` FILE on disk           -> load_msg_file(...)

Either way a message is just its `.msg` definition: irap_noroslib derives the md5sum
(matching `rosmsg md5`) and the wire codec from it, so real ROS nodes accept it.

Way 2 is what you want when you already HAVE the file -- e.g. you copied it off a
robot. No catkin package needed, no ROS installed: just the file's full path plus
the ROS package it came from (ROS names types "pkg/Type", so it needs the "pkg").

Watch it from real ROS:
    rostopic echo /pose2d       # works: same md5 as geometry_msgs/Pose2D
    rostopic info /reading      # shows the type + md5 we advertised

`rostopic echo /reading` will say "Cannot load message class" -- that is ROS being
ROS, not a irap_noroslib problem: echo needs the message class BUILT in a catkin
package, and "irap_noroslib_demo" is a made-up package that exists only here. Point
this at a real package's .msg (one that catkin built) and echo decodes it fine.
"""
import os
import irap_noroslib
from irap_noroslib import define_message, load_msg_file

HERE = os.path.dirname(os.path.abspath(__file__))

# -- 1. from .msg TEXT -------------------------------------------------------
# Nesting other registered types (built-in or your own) works too.
Pose2D = define_message("irap_noroslib_demo/Pose2D", """
    float64 x
    float64 y
    float64 theta
""")

# -- 2. from a .msg FILE -----------------------------------------------------
# examples/msgs/Reading.msg is a loose file -- no package.xml, no msg/ dir. The
# second argument is the ROS package it came from. It nests std_msgs/Header and
# geometry_msgs/Point; those are built in, so they resolve with no extra work.
# Got several custom messages? One call each, with its own path.
Reading = load_msg_file(os.path.join(HERE, "msgs", "Reading.msg"), "irap_noroslib_demo")


def main():
    # Point irap_noroslib at the ROS master before init_node (defaults to a local roscore).
    irap_noroslib.set_master_uri(os.environ.get("ROS_MASTER_URI", "http://localhost:11311"))
    irap_noroslib.set_hostname(os.environ.get("ROS_HOSTNAME", "localhost"))
    irap_noroslib.init_node("irap_noroslib_custom")

    irap_noroslib.loginfo("Pose2D  (from text) md5sum = %s" % Pose2D.md5sum())
    irap_noroslib.loginfo("Reading (from file) md5sum = %s" % Reading.md5sum())

    pose_pub = irap_noroslib.Publisher("/pose2d", Pose2D)
    reading_pub = irap_noroslib.Publisher("/reading", Reading)

    rate = irap_noroslib.Rate(5)
    x = 0.0
    while not irap_noroslib.is_shutdown():
        pose_pub.publish(Pose2D(x=x, y=-x, theta=x / 10.0))

        r = Reading(value=x / 2.0, unit="C")
        r.header.stamp = irap_noroslib.now()
        r.header.frame_id = "sensor_link"
        r.where.x = x
        reading_pub.publish(r)

        irap_noroslib.loginfo("published Pose2D x=%.1f and Reading value=%.1f%s"
                              % (x, r.value, r.unit))
        x += 1.0
        rate.sleep()


if __name__ == "__main__":
    main()
