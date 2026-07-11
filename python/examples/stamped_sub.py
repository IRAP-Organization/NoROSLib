#!/usr/bin/env python3
"""Subscribe to the custom Header-bearing message from stamped_pub.py and print
the Header fields (seq, stamp, frame_id) plus the payload.

Run a roscore + stamped_pub.py, then:  python3 stamped_sub.py
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))  # locate the noros package (../)
import noros
from noros import define_message

# Same definition as the publisher -> same md5, so the handshake matches.
SensorReading = define_message("noros_demo/SensorReading", """
    std_msgs/Header header
    float64 temperature
    float64 humidity
    string sensor_id
""")


def callback(m):
    secs, nsecs = m.header.stamp
    noros.loginfo("seq=%d stamp=%d.%09d frame=%s | %s temp=%.1f hum=%.0f"
                  % (m.header.seq, secs, nsecs, m.header.frame_id,
                     m.sensor_id, m.temperature, m.humidity))


def main():
    # Point noros at the ROS master before init_node (defaults to a local roscore).
    noros.set_master_uri(os.environ.get("ROS_MASTER_URI", "http://localhost:11311"))
    noros.set_hostname(os.environ.get("ROS_HOSTNAME", "localhost"))
    noros.init_node("stamped_sub")
    noros.Subscriber("/sensor_reading", SensorReading, callback)
    noros.spin()


if __name__ == "__main__":
    main()
