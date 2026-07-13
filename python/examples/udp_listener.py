#!/usr/bin/env python3
"""irap_noroslib subscriber over UDPROS (unreliable UDP transport).

Feed it from a roscpp publisher (which offers UDPROS):
    rosrun roscpp_tutorials talker
Then: python3 udp_listener.py

Only the transport hint changes vs a normal Subscriber.
"""
import os
import irap_noroslib
from irap_noroslib.std_msgs.msg import String


def main():
    # Point irap_noroslib at the ROS master before init_node (defaults to a local roscore).
    irap_noroslib.set_master_uri(os.environ.get("ROS_MASTER_URI", "http://localhost:11311"))
    irap_noroslib.set_hostname(os.environ.get("ROS_HOSTNAME", "localhost"))
    irap_noroslib.init_node("irap_noroslib_udp_listener")
    irap_noroslib.Subscriber("/chatter", String,
                     lambda m: irap_noroslib.loginfo("UDPROS heard: %s" % m.data),
                     transport="udpros")            # <-- UDP instead of TCP
    irap_noroslib.spin()


if __name__ == "__main__":
    main()
