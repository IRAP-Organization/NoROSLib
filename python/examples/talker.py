#!/usr/bin/env python3
"""noros talker -- publishes std_msgs/String at 10 Hz to a real roscore.

Run a roscore, then:   python3 talker.py
Watch it with real ROS: rostopic echo /chatter
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))  # locate the noros package (../)
import noros
from noros import msg


def main():
    # Point noros at the ROS master before init_node (defaults to a local roscore).
    noros.set_master_uri(os.environ.get("ROS_MASTER_URI", "http://localhost:11311"))
    noros.set_hostname(os.environ.get("ROS_HOSTNAME", "localhost"))
    noros.init_node("noros_talker")
    pub = noros.Publisher("/chatter", msg.String)
    rate = noros.Rate(10)
    i = 0
    while not noros.is_shutdown():
        m = msg.String(data="hello world %d" % i)
        pub.publish(m)
        noros.loginfo("published: %s" % m.data)
        i += 1
        rate.sleep()


if __name__ == "__main__":
    main()
