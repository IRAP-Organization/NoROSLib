#!/usr/bin/env python3
"""noros subscriber over UDPROS (unreliable UDP transport).

Feed it from a roscpp publisher (which offers UDPROS):
    rosrun roscpp_tutorials talker
Then: python3 udp_listener.py

Only the transport hint changes vs a normal Subscriber.
"""
import os
import noros
from noros import msg


def main():
    # Point noros at the ROS master before init_node (defaults to a local roscore).
    noros.set_master_uri(os.environ.get("ROS_MASTER_URI", "http://localhost:11311"))
    noros.set_hostname(os.environ.get("ROS_HOSTNAME", "localhost"))
    noros.init_node("noros_udp_listener")
    noros.Subscriber("/chatter", msg.String,
                     lambda m: noros.loginfo("UDPROS heard: %s" % m.data),
                     transport="udpros")            # <-- UDP instead of TCP
    noros.spin()


if __name__ == "__main__":
    main()
