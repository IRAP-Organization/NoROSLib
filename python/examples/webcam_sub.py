#!/usr/bin/env python3
"""irap_noroslib webcam subscriber: receive the images webcam_pub sends, and show them.

The mirror of webcam_pub.py. It subscribes to BOTH streams and displays them:

    /irap_noroslib/image_raw              sensor_msgs/Image            (bgr8)
    /irap_noroslib/image_raw/compressed   sensor_msgs/CompressedImage  (jpeg)

No ROS linked on this side -- irap_noroslib decodes the ROS wire format itself and
hands you the bytes; OpenCV only turns them back into a picture.

REQUIRES OpenCV (`pip install opencv-python`) for cv2.imshow / cv2.imdecode.
**OpenCV is not a dependency of irap_noroslib** -- the library needs nothing but the
Python standard library. It is only this demo that wants a window to draw in.

Run a roscore, then:
    python3 webcam_pub.py        # in one terminal
    python3 webcam_sub.py        # in another

The publisher can equally be a real ROS node (or the C++ webcam_pub) -- the wire
format is the same. Press q or ESC to quit.
"""
import os

import cv2
import numpy as np

import irap_noroslib
from irap_noroslib.sensor_msgs.msg import Image, CompressedImage

_latest = {"raw": None, "jpeg": None}


def on_raw(m):
    """sensor_msgs/Image -- raw bgr8 bytes, so reshape straight into a picture."""
    if m.encoding != "bgr8":
        irap_noroslib.logwarn("expected bgr8, got %r" % m.encoding)
        return
    buf = np.frombuffer(m.data, dtype=np.uint8)
    if buf.size != m.height * m.step:
        return                                  # a partial frame; skip it
    _latest["raw"] = buf.reshape(m.height, m.step // 3, 3)[:, :m.width]


def on_compressed(m):
    """sensor_msgs/CompressedImage -- a jpeg blob; let OpenCV decode it."""
    _latest["jpeg"] = cv2.imdecode(np.frombuffer(m.data, dtype=np.uint8),
                                   cv2.IMREAD_COLOR)


def main():
    # Point irap_noroslib at the ROS master before init_node (defaults to a local roscore).
    irap_noroslib.set_master_uri(os.environ.get("ROS_MASTER_URI", "http://localhost:11311"))
    irap_noroslib.set_hostname(os.environ.get("ROS_HOSTNAME", "localhost"))
    irap_noroslib.init_node("irap_noroslib_webcam_sub")

    irap_noroslib.Subscriber("/irap_noroslib/image_raw", Image, on_raw)
    irap_noroslib.Subscriber("/irap_noroslib/image_raw/compressed", CompressedImage,
                             on_compressed)
    irap_noroslib.loginfo("waiting for images... (press q or ESC to quit)")

    rate = irap_noroslib.Rate(50)
    while not irap_noroslib.is_shutdown():
        if _latest["raw"] is not None:
            cv2.imshow("irap_noroslib raw (python)", _latest["raw"])
        if _latest["jpeg"] is not None:
            cv2.imshow("irap_noroslib compressed (python)", _latest["jpeg"])
        key = cv2.waitKey(1) & 0xFF
        if key in (ord("q"), 27):
            break
        rate.sleep()
    cv2.destroyAllWindows()


if __name__ == "__main__":
    main()
