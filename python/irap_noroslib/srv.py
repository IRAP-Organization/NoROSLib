"""Service (.srv) types for irap_noroslib.

A service type is two `.msg` bodies -- a request and a response -- separated in a
`.srv` file by a `---` line. Register both and irap_noroslib derives the service md5sum
(the ROS rule: md5 of request-md5-text concatenated with response-md5-text) and
gives you Request/Response message classes.

    from irap_noroslib.srv import define_service
    AddTwoInts = define_service("rospy_tutorials/AddTwoInts",
                                "int64 a\nint64 b", "int64 sum")
    req = AddTwoInts.Request(a=3, b=4)
    resp = AddTwoInts.Response(sum=7)
    AddTwoInts.md5sum()      # matches `rossrv md5 rospy_tutorials/AddTwoInts`

Built-in std_srvs (Empty, Trigger, SetBool) are below.
"""
import hashlib

from .message import registry


class ServiceException(RuntimeError):
    """Raised when a service call fails (server returned an error)."""


def define_service(full_type, request_text, response_text):
    """Register a service type and return a class exposing .Request / .Response
    (message classes), ._type, and md5sum(). Idempotent-ish (re-registers)."""
    if "/" not in full_type:
        raise ValueError("service type must be 'pkg/Type', got %r" % full_type)
    req_cls = registry.register(full_type + "Request", request_text or "\n")
    resp_cls = registry.register(full_type + "Response", response_text or "\n")
    md5 = _service_md5(full_type)

    class Service(object):
        _type = full_type
        _md5 = md5
        Request = req_cls
        Response = resp_cls

        @classmethod
        def md5sum(cls):
            return cls._md5

        @classmethod
        def data_type(cls):
            return cls._type

    Service.__name__ = str(full_type.split("/")[-1])
    Service.__qualname__ = Service.__name__
    return Service


def _service_md5(full_type):
    req = registry.get_spec(full_type + "Request").md5_text()
    resp = registry.get_spec(full_type + "Response").md5_text()
    return hashlib.md5((req + resp).encode("utf-8")).hexdigest()


# -- built-in std_srvs ------------------------------------------------------
Empty = define_service("std_srvs/Empty", "", "")
Trigger = define_service("std_srvs/Trigger", "", "bool success\nstring message")
SetBool = define_service("std_srvs/SetBool", "bool data", "bool success\nstring message")
