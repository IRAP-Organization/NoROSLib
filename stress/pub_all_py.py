#!/usr/bin/env python3
"""noros PY stress publisher: advertise EVERY built-in catalog type + a rich
CUSTOM message, populate each with deterministic sentinels, publish (latched +
periodic) to a real roscore. Run verify_ros.py (a real rospy node) to check that
each type decodes correctly against real ROS."""
import os, sys
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(_HERE, "..", "python"))   # locate the noros package
sys.path.insert(0, _HERE)                             # locate populate.py
import noros
import noros.msg as M
from noros import define_message
from populate import populate

# A genuinely custom message: Header + nested Point + arrays + scalars. Its name
# matches the catkin package in msgs/noros_stress_msgs so real ROS can decode it.
CustomData = define_message("noros_stress_msgs/CustomData", """
    std_msgs/Header header
    int32 id
    float64[] samples
    string label
    uint8[] blob
    geometry_msgs/Point location
    bool valid
""")


def main():
    noros.set_master_uri(os.environ.get("ROS_MASTER_URI", "http://localhost:11311"))
    noros.set_hostname(os.environ.get("ROS_HOSTNAME", "localhost"))
    noros.init_node("noros_stress_pub_py")

    pubs = []
    for n in [x for x in dir(M) if x[0].isupper()]:
        cls = getattr(M, n)
        if not hasattr(cls, "_type"):
            continue
        pubs.append((n, noros.Publisher("/stress/py/%s" % n, cls, latch=True), cls))
    pubs.append(("CustomData", noros.Publisher("/stress/py/CustomData", CustomData, latch=True), CustomData))

    noros.loginfo("advertising %d topics (%d built-ins + custom)" % (len(pubs), len(pubs) - 1))
    print("PUB_COUNT=%d" % len(pubs)); sys.stdout.flush()

    rate = noros.Rate(10)
    while not noros.is_shutdown():
        for n, pub, cls in pubs:
            try:
                pub.publish(populate(cls()))
            except Exception as e:
                noros.logerr("publish failed for %s: %r" % (n, e))
        rate.sleep()


if __name__ == "__main__":
    main()
