#!/usr/bin/env python3
"""irap_noroslib listener -- subscribes to std_msgs/String from a real roscore.

Run a roscore, then:   python3 listener.py
Feed it with real ROS:  rostopic pub -r 10 /chatter std_msgs/String "data: hi"
"""
import os
import irap_noroslib
from irap_noroslib import msg


def callback(m):
    irap_noroslib.loginfo("I heard: %s" % m.data)


def main():
    # Point irap_noroslib at the ROS master before init_node (defaults to a local roscore).
    irap_noroslib.set_master_uri(os.environ.get("ROS_MASTER_URI", "http://localhost:11311"))
    irap_noroslib.set_hostname(os.environ.get("ROS_HOSTNAME", "localhost"))
    irap_noroslib.init_node("irap_noroslib_listener")
    irap_noroslib.Subscriber("/chatter", msg.String, callback)
    irap_noroslib.spin()


if __name__ == "__main__":
    main()
