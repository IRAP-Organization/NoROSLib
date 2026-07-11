#!/usr/bin/env python3
"""noros listener -- subscribes to std_msgs/String from a real roscore.

Run a roscore, then:   python3 listener.py
Feed it with real ROS:  rostopic pub -r 10 /chatter std_msgs/String "data: hi"
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))  # locate the noros package (../)
import noros
from noros import msg


def callback(m):
    noros.loginfo("I heard: %s" % m.data)


def main():
    noros.init_node("noros_listener")
    noros.Subscriber("/chatter", msg.String, callback)
    noros.spin()


if __name__ == "__main__":
    main()
