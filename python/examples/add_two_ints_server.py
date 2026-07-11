#!/usr/bin/env python3
"""noros service SERVER -- answers rospy_tutorials/AddTwoInts on /add_two_ints.

Run a roscore, then:   python3 add_two_ints_server.py
Call it from real ROS:  rosservice call /add_two_ints "a: 3
b: 4"
Or from the noros client: python3 add_two_ints_client.py 3 4
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))  # locate the noros package (../)
import noros
from noros import define_service

# A service is two .msg bodies (request --- response). noros derives the service
# md5sum, matching `rossrv md5 rospy_tutorials/AddTwoInts`.
AddTwoInts = define_service("rospy_tutorials/AddTwoInts",
                            "int64 a\nint64 b", "int64 sum")


def handle(req):
    noros.loginfo("request: %d + %d" % (req.a, req.b))
    return AddTwoInts.Response(sum=req.a + req.b)


def main():
    noros.init_node("add_two_ints_server")
    noros.Service("/add_two_ints", AddTwoInts, handle)
    noros.loginfo("ready to add two ints (md5 %s)" % AddTwoInts.md5sum())
    noros.spin()


if __name__ == "__main__":
    main()
