#!/usr/bin/env python3
"""Subscribe to the custom Header-bearing message from stamped_pub.py and print
the Header fields (seq, stamp, frame_id) plus the payload.

Run a roscore + stamped_pub.py, then:  python3 stamped_sub.py
"""
import os
import irap_noroslib
from irap_noroslib import define_message

# Same definition as the publisher -> same md5, so the handshake matches.
SensorReading = define_message("irap_noroslib_demo/SensorReading", """
    std_msgs/Header header
    float64 temperature
    float64 humidity
    string sensor_id
""")


def callback(m):
    secs, nsecs = m.header.stamp
    irap_noroslib.loginfo("seq=%d stamp=%d.%09d frame=%s | %s temp=%.1f hum=%.0f"
                  % (m.header.seq, secs, nsecs, m.header.frame_id,
                     m.sensor_id, m.temperature, m.humidity))


def main():
    # Point irap_noroslib at the ROS master before init_node (defaults to a local roscore).
    irap_noroslib.set_master_uri(os.environ.get("ROS_MASTER_URI", "http://localhost:11311"))
    irap_noroslib.set_hostname(os.environ.get("ROS_HOSTNAME", "localhost"))
    irap_noroslib.init_node("stamped_sub")
    irap_noroslib.Subscriber("/sensor_reading", SensorReading, callback)
    irap_noroslib.spin()


if __name__ == "__main__":
    main()
