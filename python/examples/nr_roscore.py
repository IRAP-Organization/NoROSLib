#!/usr/bin/env python3
"""nr_roscore -- run a standalone ROS master (roscore) with no ROS installed.

    python3 nr_roscore.py                 # binds :11311, advertises this host
    python3 nr_roscore.py --port 11322
    python3 nr_roscore.py --host 192.168.1.10 --port 11311

Configuration (same knobs as a real roscore):
  * port     : --port, else the port in $ROS_MASTER_URI, else 11311
  * hostname : --host, else $ROS_HOSTNAME, else $ROS_IP, else the system hostname
Then point nodes at it:  export ROS_MASTER_URI=http://<host>:<port>
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))  # locate the noros package (../)
from noros.roscore import main

if __name__ == "__main__":
    main()
