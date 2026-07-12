#!/usr/bin/env python3
"""Demonstrate noros's automatic md5 discovery.

We subscribe to a real ROS topic but deliberately present a WRONG md5sum. A real
ROS publisher rejects that with the classic
    "... but our version has [std_msgs/String/992ce8...]. Dropping connection."
noros parses the publisher's REAL md5 out of that error, adopts it, reconnects,
and data flows -- no "Dropping connection" left standing.

Run:  rostopic pub -r 5 /disc std_msgs/String "data: real ros here"
Then: python3 md5_discovery.py
"""
import os
import noros
from noros import msg  # registers std_msgs/String so post-discovery decode works


def callback(m):
    # after discovery, msg_class resolves to std_msgs/String and we get a String
    data = m.data if hasattr(m, "data") else m
    noros.loginfo("RECOVERED and received: %r" % data)


def main():
    # Point noros at the ROS master before init_node (defaults to a local roscore).
    noros.set_master_uri(os.environ.get("ROS_MASTER_URI", "http://localhost:11311"))
    noros.set_hostname(os.environ.get("ROS_HOSTNAME", "localhost"))
    node = noros.init_node("noros_md5_discovery")
    WRONG_MD5 = "00000000000000000000000000000000"
    noros.logwarn("subscribing to /disc with a deliberately WRONG md5: %s" % WRONG_MD5)
    # go through the node's low-level subscribe so we can force a bad md5 while
    # still naming the correct type. msg_class=None => resolve it after discovery.
    node.subscribe("/disc", "std_msgs/String", WRONG_MD5, callback, None)
    noros.spin()


if __name__ == "__main__":
    main()
