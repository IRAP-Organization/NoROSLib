#!/usr/bin/env python3
"""REAL ROS (rospy) publisher of the custom noros_stress_msgs/CustomData."""
import rospy
from std_msgs.msg import Header
from geometry_msgs.msg import Point
from noros_stress_msgs.msg import CustomData

rospy.init_node("real_ros_custom_pub", disable_signals=True)
pub = rospy.Publisher("/stress/realros/CustomData", CustomData, queue_size=2, latch=True)
r = rospy.Rate(10)
while not rospy.is_shutdown():
    m = CustomData()
    m.header = Header(seq=1, frame_id="real_ros")
    m.id = 4242
    m.samples = [0.25, 0.5, 0.75]
    m.label = "from_real_ros"
    m.blob = bytes([9, 8, 7])
    m.location = Point(x=1.0, y=2.0, z=3.0)
    m.valid = True
    pub.publish(m)
    r.sleep()
