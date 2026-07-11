#!/usr/bin/env python3
"""A REAL ROS node (rospy + cv_bridge + OpenCV) that receives the images noros
publishes and shows them with cv2.imshow. This is the interop proof: a genuine
ROS subscriber decoding noros's sensor_msgs/Image and sensor_msgs/CompressedImage.

    source /opt/ros/noetic/setup.bash
    export DISPLAY=:0
    python3 ros_image_viewer.py [runtime_sec] [out_dir]

Subscribes:
    /noros/image_raw              sensor_msgs/Image
    /noros/image_raw/compressed   sensor_msgs/CompressedImage
"""
import os
import subprocess
import sys
import time

import cv2
import numpy as np
import rospy
from sensor_msgs.msg import Image, CompressedImage
from cv_bridge import CvBridge


def display_reachable():
    """True if the X server named by $DISPLAY is actually reachable. Guards
    against a hard GTK abort when the display is set but not authorized."""
    disp = os.environ.get("DISPLAY")
    if not disp:
        return False
    try:
        return subprocess.call(["xset", "q"], stdout=subprocess.DEVNULL,
                               stderr=subprocess.DEVNULL) == 0
    except OSError:
        return False

bridge = CvBridge()
state = {"raw": None, "comp": None, "nraw": 0, "ncomp": 0,
         "raw_shape": None, "comp_shape": None}


def raw_cb(m):
    img = bridge.imgmsg_to_cv2(m, "bgr8")     # real cv_bridge decode
    state["raw"] = img
    state["raw_shape"] = img.shape
    state["nraw"] += 1


def comp_cb(m):
    arr = np.frombuffer(m.data, np.uint8)
    img = cv2.imdecode(arr, cv2.IMREAD_COLOR)  # decode the jpeg noros sent
    state["comp"] = img
    state["comp_shape"] = None if img is None else img.shape
    state["ncomp"] += 1


def main():
    runtime = float(sys.argv[1]) if len(sys.argv) > 1 else 12.0
    out_dir = sys.argv[2] if len(sys.argv) > 2 else "."

    rospy.init_node("ros_image_viewer", disable_signals=True)
    rospy.Subscriber("/noros/image_raw", Image, raw_cb, queue_size=1)
    rospy.Subscriber("/noros/image_raw/compressed", CompressedImage, comp_cb, queue_size=1)
    use_gui = display_reachable()
    rospy.loginfo("ROS viewer up; DISPLAY=%r gui=%s"
                  % (os.environ.get("DISPLAY"), use_gui))
    if not use_gui:
        rospy.logwarn("DISPLAY not reachable -> headless; still saving frames to disk")

    saved = False
    t_end = time.time() + runtime
    last_log = 0.0
    while not rospy.is_shutdown() and time.time() < t_end:
        if use_gui:
            try:
                if state["raw"] is not None:
                    cv2.imshow("noros raw (real ROS)", state["raw"])
                if state["comp"] is not None:
                    cv2.imshow("noros compressed (real ROS)", state["comp"])
                cv2.waitKey(1)
            except cv2.error as e:
                rospy.logwarn("cv2.imshow unavailable (%s); continuing headless" % e)
                use_gui = False

        # save proof frames once both streams are flowing
        if (not saved and state["raw"] is not None and state["comp"] is not None
                and state["nraw"] > 5):
            cv2.imwrite(out_dir + "/recv_raw.png", state["raw"])
            cv2.imwrite(out_dir + "/recv_compressed.png", state["comp"])
            saved = True
            rospy.loginfo("saved recv_raw.png + recv_compressed.png to %s" % out_dir)

        now = time.time()
        if now - last_log > 2.0:
            rospy.loginfo("raw: %d frames %s | compressed: %d frames %s"
                          % (state["nraw"], state["raw_shape"],
                             state["ncomp"], state["comp_shape"]))
            last_log = now
        time.sleep(0.01)

    rospy.loginfo("DONE raw=%d compressed=%d gui=%s saved=%s"
                  % (state["nraw"], state["ncomp"], use_gui, saved))
    if use_gui:
        cv2.destroyAllWindows()


if __name__ == "__main__":
    main()
