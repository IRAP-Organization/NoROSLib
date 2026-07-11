#!/usr/bin/env python3
"""REAL ROS verifier (rospy). Discovers every /stress/<lang>/* topic a noros node
advertises, subscribes with the GENUINE installed ROS message class, and waits
for a decoded message. Because rospy enforces the md5 handshake and decodes the
bytes itself, one received message per topic proves noros's wire format + md5 +
message definition are all correct against real ROS.

Custom types (not installed in this ROS) are decoded via `rostopic echo`, which
builds the class dynamically from the connection-header message_definition noros
sends -- i.e. real ROS tooling handling a brand-new custom message.

Usage: python3 verify_ros.py /stress/py   [wait_sec]
"""
import subprocess
import sys
import threading
import time

import rospy
import roslib.message

SENTINEL_STR = "noros"
SENTINEL_INT = 7
SENTINEL_FLOAT = 1.5


def spot_check(short, m):
    """Return None if ok, else a mismatch string. Only a few well-known types."""
    try:
        if short == "String" and m.data != SENTINEL_STR:
            return "data=%r" % m.data
        if short in ("Int8", "Int16", "Int32", "Int64", "UInt8", "UInt16",
                     "UInt32", "UInt64", "Byte", "Char") and m.data != SENTINEL_INT:
            return "data=%r" % (m.data,)
        if short in ("Float32", "Float64") and abs(m.data - SENTINEL_FLOAT) > 1e-6:
            return "data=%r" % (m.data,)
        if short == "Bool" and m.data is not True:
            return "data=%r" % (m.data,)
        if short == "Odometry" and m.child_frame_id != SENTINEL_STR:
            return "child_frame_id=%r" % m.child_frame_id
        if short == "Imu" and abs(m.orientation.x - SENTINEL_FLOAT) > 1e-6:
            return "orientation.x=%r" % m.orientation.x
        if short == "JointState" and (not m.name or m.name[0] != SENTINEL_STR):
            return "name=%r" % (m.name,)
        if short == "CustomData":
            if m.label != SENTINEL_STR:
                return "label=%r" % m.label
            if m.id != SENTINEL_INT:
                return "id=%r" % m.id
            if not m.valid:
                return "valid=%r" % m.valid
            if abs(m.location.x - SENTINEL_FLOAT) > 1e-6:
                return "location.x=%r" % m.location.x
    except Exception as e:
        return "spotcheck-exc:%r" % e
    return None


def main():
    prefix = sys.argv[1] if len(sys.argv) > 1 else "/stress/py"
    wait_sec = float(sys.argv[2]) if len(sys.argv) > 2 else 8.0
    rospy.init_node("noros_stress_verifier", disable_signals=True)

    time.sleep(1.0)
    topics = [(t, ty) for t, ty in rospy.get_published_topics() if t.startswith(prefix + "/")]
    topics.sort()
    if not topics:
        print("NO TOPICS under %s" % prefix); sys.exit(2)

    results = {}           # topic -> ("PASS"/"FAIL"/..., detail)
    received = {}          # topic -> first msg
    lock = threading.Lock()
    custom = []

    for topic, ty in topics:
        cls = roslib.message.get_message_class(ty)
        if cls is None:
            custom.append((topic, ty))     # not installed -> rostopic echo path
            continue

        def make_cb(tp, short):
            def cb(m):
                with lock:
                    if tp not in received:
                        received[tp] = (short, m)
            return cb

        short = topic.rsplit("/", 1)[-1]
        rospy.Subscriber(topic, cls, make_cb(topic, short), queue_size=2)

    # collect for wait_sec
    deadline = time.time() + wait_sec
    n_installed = len(topics) - len(custom)
    while time.time() < deadline:
        with lock:
            if len(received) >= n_installed:
                break
        time.sleep(0.1)

    # score installed types
    for topic, ty in topics:
        if (topic, ty) in custom:
            continue
        with lock:
            got = received.get(topic)
        if got is None:
            results[topic] = ("FAIL", "no message decoded (md5 mismatch or no data) [%s]" % ty)
        else:
            short, m = got
            mm = spot_check(short, m)
            results[topic] = ("PASS", ty) if mm is None else ("MISMATCH", "%s %s" % (ty, mm))

    # score custom types via rostopic echo (dynamic decode from conn header)
    for topic, ty in custom:
        try:
            out = subprocess.check_output(["rostopic", "echo", "-n", "1", topic],
                                          stderr=subprocess.STDOUT, timeout=wait_sec).decode()
            ok = ("label:" in out or "data:" in out or "header:" in out)
            has_sentinel = SENTINEL_STR in out
            results[topic] = ("PASS", "%s [custom via echo, sentinel=%s]" % (ty, has_sentinel)) \
                if ok else ("FAIL", "%s echo produced no fields" % ty)
        except Exception as e:
            results[topic] = ("FAIL", "%s echo error: %r" % (ty, e))

    # report
    npass = sum(1 for v in results.values() if v[0] == "PASS")
    print("\n==== VERIFY %s : %d/%d PASS ====" % (prefix, npass, len(results)))
    for topic in sorted(results):
        status, detail = results[topic]
        mark = "OK " if status == "PASS" else ">> "
        print("%s%-10s %-28s %s" % (mark, status, topic, detail))
    print("==== %s : %d/%d PASS (%d custom) ====" % (prefix, npass, len(results), len(custom)))
    sys.exit(0 if npass == len(results) else 1)


if __name__ == "__main__":
    main()
