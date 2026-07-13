#!/usr/bin/env python3
"""Demonstrate irap_noroslib's automatic md5 discovery.

We subscribe to a real ROS topic but deliberately present a WRONG md5sum. A real
ROS publisher rejects that with the classic
    "... but our version has [std_msgs/String/992ce8...]. Dropping connection."
irap_noroslib parses the publisher's REAL md5 out of that error, adopts it, reconnects,
and data flows -- no "Dropping connection" left standing.

Run:  rostopic pub -r 5 /disc std_msgs/String "data: real ros here"
Then: python3 md5_discovery.py
"""
import os
import irap_noroslib
from irap_noroslib.std_msgs.msg import String  # registers std_msgs/String so post-discovery decode works


def callback(m):
    # after discovery, msg_class resolves to std_msgs/String and we get a String
    data = m.data if hasattr(m, "data") else m
    irap_noroslib.loginfo("RECOVERED and received: %r" % data)


def main():
    # Point irap_noroslib at the ROS master before init_node (defaults to a local roscore).
    irap_noroslib.set_master_uri(os.environ.get("ROS_MASTER_URI", "http://localhost:11311"))
    irap_noroslib.set_hostname(os.environ.get("ROS_HOSTNAME", "localhost"))
    node = irap_noroslib.init_node("irap_noroslib_md5_discovery")
    WRONG_MD5 = "00000000000000000000000000000000"
    irap_noroslib.logwarn("subscribing to /disc with a deliberately WRONG md5: %s" % WRONG_MD5)
    # go through the node's low-level subscribe so we can force a bad md5 while
    # still naming the correct type. msg_class=None => resolve it after discovery.
    node.subscribe("/disc", "std_msgs/String", WRONG_MD5, callback, None)
    irap_noroslib.spin()


if __name__ == "__main__":
    main()
