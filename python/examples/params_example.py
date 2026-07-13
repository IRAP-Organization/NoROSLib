#!/usr/bin/env python3
"""irap_noroslib parameter server usage. Cross-check with `rosparam get/set/list`."""
import os
import irap_noroslib


def main():
    # Point irap_noroslib at the ROS master before init_node (defaults to a local roscore).
    irap_noroslib.set_master_uri(os.environ.get("ROS_MASTER_URI", "http://localhost:11311"))
    irap_noroslib.set_hostname(os.environ.get("ROS_HOSTNAME", "localhost"))
    irap_noroslib.init_node("params_example")
    irap_noroslib.set_param("/demo/rate", 30)
    irap_noroslib.set_param("/demo/name", "irap_noroslib")
    irap_noroslib.set_param("/demo/gains", [1.0, 0.5, 0.1])
    irap_noroslib.set_param("/demo/cfg", {"p": 1, "i": 0})

    irap_noroslib.loginfo("rate  = %r" % irap_noroslib.get_param("/demo/rate"))
    irap_noroslib.loginfo("name  = %r" % irap_noroslib.get_param("/demo/name"))
    irap_noroslib.loginfo("gains = %r" % irap_noroslib.get_param("/demo/gains"))
    irap_noroslib.loginfo("cfg   = %r" % irap_noroslib.get_param("/demo/cfg"))
    irap_noroslib.loginfo("missing (default) = %r" % irap_noroslib.get_param("/demo/nope", default="DEF"))
    irap_noroslib.loginfo("has /demo/rate = %s" % irap_noroslib.has_param("/demo/rate"))
    irap_noroslib.delete_param("/demo/name")
    irap_noroslib.loginfo("after delete, has /demo/name = %s" % irap_noroslib.has_param("/demo/name"))


if __name__ == "__main__":
    main()
