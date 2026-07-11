#!/usr/bin/env python3
"""noros subscriber decoding the custom message published by REAL ROS (rospy).
Run rospy_pub_custom.py (a genuine ROS node) first."""
import os, sys, time
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(_HERE, "..", "python"))
import noros
from noros import define_message

CustomData = define_message("noros_stress_msgs/CustomData", """
    std_msgs/Header header
    int32 id
    float64[] samples
    string label
    uint8[] blob
    geometry_msgs/Point location
    bool valid
""")

got = {}


def main():
    noros.set_master_uri(os.environ.get("ROS_MASTER_URI", "http://localhost:11311"))
    noros.set_hostname(os.environ.get("ROS_HOSTNAME", "localhost"))
    noros.init_node("noros_custom_sub")
    noros.Subscriber("/stress/realros/CustomData", CustomData, lambda m: got.__setitem__("m", m))
    deadline = time.time() + 8.0
    while time.time() < deadline and "m" not in got:
        time.sleep(0.1)
    if "m" not in got:
        print("FAIL: noros received nothing from real ROS"); sys.exit(1)
    m = got["m"]
    ok = (m.id == 4242 and m.label == "from_real_ros" and m.valid and
          list(m.samples) == [0.25, 0.5, 0.75] and bytes(m.blob) == bytes([9, 8, 7]) and
          abs(m.location.x - 1.0) < 1e-9 and m.header.frame_id == "real_ros")
    print("noros decoded real-ROS custom msg: id=%d label=%r samples=%r blob=%r loc.x=%.1f frame=%r"
          % (m.id, m.label, list(m.samples), bytes(m.blob), m.location.x, m.header.frame_id))
    print("PASS" if ok else "FAIL: field mismatch")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
