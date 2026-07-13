#!/usr/bin/env python3
"""irap_noroslib service CLIENT -- calls /add_two_ints (rospy_tutorials/AddTwoInts).

Works against the irap_noroslib server (add_two_ints_server.py) OR a real ROS server
(rosrun rospy_tutorials add_two_ints_server).

    python3 add_two_ints_client.py 3 4     ->   3 + 4 = 7
"""
import os, sys
import irap_noroslib
from irap_noroslib import define_service

AddTwoInts = define_service("rospy_tutorials/AddTwoInts",
                            "int64 a\nint64 b", "int64 sum")


def main():
    a = int(sys.argv[1]) if len(sys.argv) > 1 else 3
    b = int(sys.argv[2]) if len(sys.argv) > 2 else 4
    # Point irap_noroslib at the ROS master before init_node (defaults to a local roscore).
    irap_noroslib.set_master_uri(os.environ.get("ROS_MASTER_URI", "http://localhost:11311"))
    irap_noroslib.set_hostname(os.environ.get("ROS_HOSTNAME", "localhost"))
    irap_noroslib.init_node("add_two_ints_client")
    irap_noroslib.wait_for_service("/add_two_ints", timeout=5.0)
    add = irap_noroslib.ServiceProxy("/add_two_ints", AddTwoInts)
    resp = add(AddTwoInts.Request(a=a, b=b))
    irap_noroslib.loginfo("%d + %d = %d" % (a, b, resp.sum))


if __name__ == "__main__":
    main()
