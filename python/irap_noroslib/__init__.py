"""irap_noroslib -- a ROS pub/sub client library with no ROS installed.

irap_noroslib impersonates a ROS node by speaking the real ROS wire protocols directly
(XML-RPC master/slave API + TCPROS), so a real roscore and real ROS nodes treat
it as a legitimate node. It ships std_msgs/geometry_msgs/sensor_msgs, lets you
define your own message from its `.msg` text in one call, and -- the headline
feature -- automatically discovers a publisher's real md5sum from the mismatch
error, so you never hit "Dropping connection" over an md5 you got wrong.

    import irap_noroslib
    from irap_noroslib import msg

    irap_noroslib.init_node("talker")
    pub = irap_noroslib.Publisher("/chatter", msg.String)
    rate = irap_noroslib.Rate(10)
    while not irap_noroslib.is_shutdown():
        pub.publish(msg.String(data="hello world"))
        rate.sleep()
"""
from .node import (
    init_node, get_node, is_shutdown, signal_shutdown,
    set_master_uri, set_hostname, configure,
    Publisher, Subscriber, Service, ServiceProxy, wait_for_service,
    get_param, set_param, has_param, delete_param, search_param, get_param_names,
    Rate, spin, sleep, now,
    loginfo, logwarn, logerr, set_log_level,
)
from .message import define_message, get_message_class, Message
from .srv import define_service, ServiceException
from .actionlib import define_action, SimpleActionClient, SimpleActionServer
from .msgfile import (
    load_msg_file, load_msg_files, load_srv_file, load_action_file, loaded_files,
)
from . import msg
from . import srv
from . import actionlib

__all__ = [
    "init_node", "get_node", "is_shutdown", "signal_shutdown",
    "set_master_uri", "set_hostname", "configure",
    "get_param", "set_param", "has_param", "delete_param", "search_param", "get_param_names",
    "Publisher", "Subscriber", "Service", "ServiceProxy", "wait_for_service",
    "Rate", "spin", "sleep", "now",
    "loginfo", "logwarn", "logerr", "set_log_level",
    "define_message", "get_message_class", "Message", "msg",
    "define_service", "ServiceException", "srv",
    "define_action", "SimpleActionClient", "SimpleActionServer", "actionlib",
    "load_msg_file", "load_msg_files", "load_srv_file", "load_action_file",
    "loaded_files",
]

__version__ = "0.1.0"
