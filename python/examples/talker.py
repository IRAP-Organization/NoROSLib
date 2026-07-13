#!/usr/bin/env python3
"""irap_noroslib talker -- publishes std_msgs/String at 10 Hz to a real roscore.

Run a roscore, then:   python3 talker.py
Watch it with real ROS: rostopic echo /chatter
"""
import os
import irap_noroslib
from irap_noroslib.std_msgs.msg import String


def main():
    # Point irap_noroslib at the ROS master before init_node (defaults to a local roscore).
    irap_noroslib.set_master_uri(os.environ.get("ROS_MASTER_URI", "http://localhost:11311"))
    irap_noroslib.set_hostname(os.environ.get("ROS_HOSTNAME", "localhost"))
    irap_noroslib.init_node("irap_noroslib_talker")
    pub = irap_noroslib.Publisher("/chatter", String)
    rate = irap_noroslib.Rate(10)
    i = 0
    while not irap_noroslib.is_shutdown():
        m = String(data="hello world %d" % i)
        pub.publish(m)
        irap_noroslib.loginfo("published: %s" % m.data)
        i += 1
        rate.sleep()


if __name__ == "__main__":
    main()
