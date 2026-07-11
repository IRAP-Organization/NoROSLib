#!/usr/bin/env python3
"""noros service CLIENT -- calls /add_two_ints (rospy_tutorials/AddTwoInts).

Works against the noros server (add_two_ints_server.py) OR a real ROS server
(rosrun rospy_tutorials add_two_ints_server).

    python3 add_two_ints_client.py 3 4     ->   3 + 4 = 7
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))  # locate the noros package (../)
import noros
from noros import define_service

AddTwoInts = define_service("rospy_tutorials/AddTwoInts",
                            "int64 a\nint64 b", "int64 sum")


def main():
    a = int(sys.argv[1]) if len(sys.argv) > 1 else 3
    b = int(sys.argv[2]) if len(sys.argv) > 2 else 4
    noros.init_node("add_two_ints_client")
    noros.wait_for_service("/add_two_ints", timeout=5.0)
    add = noros.ServiceProxy("/add_two_ints", AddTwoInts)
    resp = add(AddTwoInts.Request(a=a, b=b))
    noros.loginfo("%d + %d = %d" % (a, b, resp.sum))


if __name__ == "__main__":
    main()
