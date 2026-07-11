#!/usr/bin/env python3
"""noros subscriber over UDPROS (unreliable UDP transport).

Feed it from a roscpp publisher (which offers UDPROS):
    rosrun roscpp_tutorials talker
Then: python3 udp_listener.py

Only the transport hint changes vs a normal Subscriber.
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))  # locate the noros package (../)
import noros
from noros import msg


def main():
    noros.init_node("noros_udp_listener")
    noros.Subscriber("/chatter", msg.String,
                     lambda m: noros.loginfo("UDPROS heard: %s" % m.data),
                     transport="udpros")            # <-- UDP instead of TCP
    noros.spin()


if __name__ == "__main__":
    main()
