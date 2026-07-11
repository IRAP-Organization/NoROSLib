#!/usr/bin/env python3
"""noros parameter server usage. Cross-check with `rosparam get/set/list`."""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))  # locate the noros package (../)
import noros


def main():
    noros.init_node("params_example")
    noros.set_param("/demo/rate", 30)
    noros.set_param("/demo/name", "noros")
    noros.set_param("/demo/gains", [1.0, 0.5, 0.1])
    noros.set_param("/demo/cfg", {"p": 1, "i": 0})

    noros.loginfo("rate  = %r" % noros.get_param("/demo/rate"))
    noros.loginfo("name  = %r" % noros.get_param("/demo/name"))
    noros.loginfo("gains = %r" % noros.get_param("/demo/gains"))
    noros.loginfo("cfg   = %r" % noros.get_param("/demo/cfg"))
    noros.loginfo("missing (default) = %r" % noros.get_param("/demo/nope", default="DEF"))
    noros.loginfo("has /demo/rate = %s" % noros.has_param("/demo/rate"))
    noros.delete_param("/demo/name")
    noros.loginfo("after delete, has /demo/name = %s" % noros.has_param("/demo/name"))


if __name__ == "__main__":
    main()
