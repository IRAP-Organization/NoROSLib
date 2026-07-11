#!/usr/bin/env python3
"""noros webcam publisher: capture /dev/video0 with OpenCV and publish BOTH a
raw sensor_msgs/Image (bgr8) and a sensor_msgs/CompressedImage (jpeg) to a real
roscore -- no ROS linked on this side.

    /noros/image_raw              sensor_msgs/Image
    /noros/image_raw/compressed   sensor_msgs/CompressedImage

Run a roscore, then:  python3 webcam_pub.py
Receive with real ROS: ros_image_viewer.py (cv2.imshow)
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))  # locate the noros package (../)
import cv2

import noros
from noros import msg


def main():
    dev = int(sys.argv[1]) if len(sys.argv) > 1 else 0
    cap = cv2.VideoCapture(dev)
    if not cap.isOpened():
        noros.logerr("cannot open /dev/video%d" % dev)
        return
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)

    noros.set_master_uri("http://192.168.10.2:11311")  # set to your roscore URIs
    noros.set_hostname("192.168.10.2")  # set to your hostname
    noros.init_node("noros_webcam")
    raw_pub = noros.Publisher("/noros/image_raw", msg.Image)
    comp_pub = noros.Publisher("/noros/image_raw/compressed", msg.CompressedImage)
    rate = noros.Rate(30)
    seq = 0
    while not noros.is_shutdown():
        ok, frame = cap.read()
        if not ok:
            noros.logwarn("frame grab failed")
            rate.sleep()
            continue
        h, w = frame.shape[:2]
        stamp = noros.now()

        # --- raw Image (bgr8) ---
        raw = msg.Image()
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
            comp = msg.CompressedImage()
            comp.header.seq = seq
            comp.header.stamp = stamp
            comp.header.frame_id = "camera"
            comp.format = "jpeg"
            comp.data = enc.tobytes()
            comp_pub.publish(comp)

        if seq % 15 == 0:
            noros.loginfo("seq=%d %dx%d raw=%d B jpeg=%d B"
                          % (seq, w, h, len(raw.data),
                             len(enc) if okj else 0))
        seq += 1
        rate.sleep()

    cap.release()


if __name__ == "__main__":
    main()
