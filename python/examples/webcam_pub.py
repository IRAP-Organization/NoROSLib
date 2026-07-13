#!/usr/bin/env python3
"""irap_noroslib webcam publisher: capture /dev/video0 with OpenCV and publish BOTH a
raw sensor_msgs/Image (bgr8) and a sensor_msgs/CompressedImage (jpeg) to a real
roscore -- no ROS linked on this side.

    /irap_noroslib/image_raw              sensor_msgs/Image
    /irap_noroslib/image_raw/compressed   sensor_msgs/CompressedImage

Run a roscore, then:  python3 webcam_pub.py
Receive with real ROS: ros_image_viewer.py (cv2.imshow)
"""
import os, sys
import cv2

import irap_noroslib
from irap_noroslib.sensor_msgs.msg import Image, CompressedImage


def main():
    dev = int(sys.argv[1]) if len(sys.argv) > 1 else 0
    cap = cv2.VideoCapture(dev)
    if not cap.isOpened():
        irap_noroslib.logerr("cannot open /dev/video%d" % dev)
        return
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)

    # Point irap_noroslib at the ROS master before init_node (defaults to a local roscore).
    irap_noroslib.set_master_uri(os.environ.get("ROS_MASTER_URI", "http://localhost:11311"))
    irap_noroslib.set_hostname(os.environ.get("ROS_HOSTNAME", "localhost"))
    irap_noroslib.init_node("irap_noroslib_webcam")
    raw_pub = irap_noroslib.Publisher("/irap_noroslib/image_raw", Image)
    comp_pub = irap_noroslib.Publisher("/irap_noroslib/image_raw/compressed", CompressedImage)
    rate = irap_noroslib.Rate(30)
    seq = 0
    while not irap_noroslib.is_shutdown():
        ok, frame = cap.read()
        if not ok:
            irap_noroslib.logwarn("frame grab failed")
            rate.sleep()
            continue
        h, w = frame.shape[:2]
        stamp = irap_noroslib.now()

        # --- raw Image (bgr8) ---
        raw = Image()
        raw.header.seq = seq
        raw.header.stamp = stamp
        raw.header.frame_id = "camera"
        raw.height, raw.width = h, w
        raw.encoding = "bgr8"
        raw.is_bigendian = 0
        raw.step = w * 3
        raw.data = frame.tobytes()
        raw_pub.publish(raw)

        # --- CompressedImage (jpeg) ---
        okj, enc = cv2.imencode(".jpg", frame, [cv2.IMWRITE_JPEG_QUALITY, 80])
        if okj:
            comp = CompressedImage()
            comp.header.seq = seq
            comp.header.stamp = stamp
            comp.header.frame_id = "camera"
            comp.format = "jpeg"
            comp.data = enc.tobytes()
            comp_pub.publish(comp)

        if seq % 15 == 0:
            irap_noroslib.loginfo("seq=%d %dx%d raw=%d B jpeg=%d B"
                          % (seq, w, h, len(raw.data),
                             len(enc) if okj else 0))
        seq += 1
        rate.sleep()

    cap.release()


if __name__ == "__main__":
    main()
