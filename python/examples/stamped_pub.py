#!/usr/bin/env python3
"""Publish a CUSTOM message that carries a std_msgs/Header.

The .msg text just names `std_msgs/Header header` as its first field; noros
nests the built-in Header for you (serialization + md5). We stamp each message
with the current time, an incrementing seq, and a frame_id -- exactly as a ROS
sensor driver would.

Run a roscore, then:  python3 stamped_pub.py
See it (noros side):   python3 stamped_sub.py
"""
import os
import noros
from noros import define_message

# A custom message whose first field is a Header (ROS convention for stamped
# data). md5sum is derived automatically from the Header md5 + the other fields.
SensorReading = define_message("noros_demo/SensorReading", """
    std_msgs/Header header
    float64 temperature
    float64 humidity
    string sensor_id
""")


def main():
    # Point noros at the ROS master before init_node (defaults to a local roscore).
    noros.set_master_uri(os.environ.get("ROS_MASTER_URI", "http://localhost:11311"))
    noros.set_hostname(os.environ.get("ROS_HOSTNAME", "localhost"))
    noros.init_node("stamped_pub")
    noros.loginfo("SensorReading md5sum = %s" % SensorReading.md5sum())
    pub = noros.Publisher("/sensor_reading", SensorReading)
    rate = noros.Rate(5)
    seq = 0
    while not noros.is_shutdown():
        m = SensorReading()
        m.header.seq = seq                     # ROS auto-fills this too
        m.header.stamp = noros.now()           # (secs, nsecs)
        m.header.frame_id = "sensor_link"
        m.temperature = 20.0 + seq * 0.1
        m.humidity = 55.0
        m.sensor_id = "dht22-0"
        pub.publish(m)
        noros.loginfo("published seq=%d temp=%.1f frame=%s"
                      % (m.header.seq, m.temperature, m.header.frame_id))
        seq += 1
        rate.sleep()


if __name__ == "__main__":
    main()
