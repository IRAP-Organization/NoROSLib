#!/usr/bin/env python3
"""irap_noroslib service SERVER -- answers rospy_tutorials/AddTwoInts on /add_two_ints.

Run a roscore, then:   python3 add_two_ints_server.py
Call it from real ROS:  rosservice call /add_two_ints "a: 3
b: 4"
Or from the irap_noroslib client: python3 add_two_ints_client.py 3 4
"""
import os
import irap_noroslib
from irap_noroslib import define_service

# A service is two .msg bodies (request --- response). irap_noroslib derives the service
# md5sum, matching `rossrv md5 rospy_tutorials/AddTwoInts`.
AddTwoInts = define_service("rospy_tutorials/AddTwoInts",
                            "int64 a\nint64 b", "int64 sum")


def handle(req):
    irap_noroslib.loginfo("request: %d + %d" % (req.a, req.b))
    return AddTwoInts.Response(sum=req.a + req.b)


def main():
    # Point irap_noroslib at the ROS master before init_node (defaults to a local roscore).
    irap_noroslib.set_master_uri(os.environ.get("ROS_MASTER_URI", "http://localhost:11311"))
    irap_noroslib.set_hostname(os.environ.get("ROS_HOSTNAME", "localhost"))
    irap_noroslib.init_node("add_two_ints_server")
    irap_noroslib.Service("/add_two_ints", AddTwoInts, handle)
    irap_noroslib.loginfo("ready to add two ints (md5 %s)" % AddTwoInts.md5sum())
    irap_noroslib.spin()


if __name__ == "__main__":
    main()
