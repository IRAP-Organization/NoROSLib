"""nr_rosbag -- `rosbag`, without ROS installed.

The subcommands you actually use, same arguments, same output shape:

    nr_rosbag record [-a] [-O NAME] [-o PREFIX] [topic ...]
    nr_rosbag play [-r RATE] [-l] [--clock] bag [bag ...]
    nr_rosbag info bag [bag ...]

`record` captures the exact wire bytes of ANY topic -- even a custom type you
have no .msg file for -- because it saves the publisher's message_definition
alongside, exactly as real rosbag does. Bags are the real v2.0 format, so they
open in genuine `rosbag info` / `rosbag play` / `rqt_bag`; and a bag recorded by
real ROS plays back here.

Point it at a master with $ROS_MASTER_URI, or `--master http://host:11311`.
It shares ./master.yaml with nr_rostopic.
"""
import argparse
import datetime
import os
import signal
import socket
import sys
import threading
import time

import irap_noroslib
from . import msg as _msg  # noqa: F401
from .bag import BagWriter, BagReader
from .message import define_message_from_definition, get_message_class
from .rostopic import (
    resolve_master, load_master_yaml, save_master_yaml, _node,
)


def _wall():
    t = time.time()
    secs = int(t)
    return secs, int(round((t - secs) * 1e9))


# ------------------------------------------------------------------ record ----
def cmd_record(args):
    if not args.all and not args.topics:
        print("nr_rosbag record: give topics, or -a/--all", file=sys.stderr)
        return 1
    _node("nr_rosbag")
    lock = threading.Lock()
    subs = {}                           # topic -> Subscriber
    stop = threading.Event()
    total = [0]
    # --split writes a sequence of bags (base_0.bag, base_1.bag, ...); without it,
    # a single bag. `st` holds the mutable current-bag state, so a roll swaps the
    # writer and re-creates the connections for the new file.
    split_bytes = int(args.size * 1024 * 1024) if args.size else 0
    split_secs = args.duration or 0
    st = {"writer": None, "conns": {}, "seen_meta": {},
          "bytes": 0, "opened": 0.0, "index": 0}

    def open_bag():
        name = _record_name(args, st["index"] if args.split else None)
        st["writer"] = BagWriter(name)
        st["conns"] = {}
        st["bytes"] = 0
        st["opened"] = time.time()
        print("Recording to '%s'." % name)

    def maybe_roll():
        if not args.split:
            return
        big = split_bytes and st["bytes"] >= split_bytes
        old = split_secs and (time.time() - st["opened"]) >= split_secs
        if big or old:
            st["writer"].close()
            st["index"] += 1
            open_bag()

    def make_cb(topic):
        def cb(type_name, md5, definition, body):
            secs, nsecs = _wall()
            with lock:
                st["seen_meta"][topic] = (type_name, md5, definition)
                c = st["conns"].get(topic)
                if c is None:
                    c = st["writer"].connection(topic, type_name, md5, definition,
                                                callerid=_node("nr_rosbag").name)
                    st["conns"][topic] = c
                    if st["index"] == 0:
                        print("Subscribed to [%s]" % topic)
                st["writer"].write(c, secs, nsecs, body)
                st["bytes"] += len(body)
                total[0] += 1
                maybe_roll()
        return cb

    def ensure(topic):
        if topic in subs:
            return
        subs[topic] = irap_noroslib.subscribe_raw(topic, make_cb(topic))

    # A bag MUST be finalized (its header rewritten with the index offsets) or it
    # is unreadable. SIGINT is the normal stop; also handle SIGTERM so a killed
    # recorder still closes the bag cleanly.
    def _on_signal(_signum, _frame):
        stop.set()
    try:
        signal.signal(signal.SIGTERM, _on_signal)
    except (ValueError, OSError):
        pass

    open_bag()
    try:
        if args.topics and not args.all:
            for t in args.topics:
                ensure(t if t.startswith("/") else "/" + t)
        # -a (and topic discovery) polls the master for topics as they appear.
        while not stop.is_set() and not irap_noroslib.is_shutdown():
            if args.all:
                for topic, _ty in _node("nr_rosbag").master.get_topic_types():
                    ensure(topic)
            stop.wait(0.5)
    except KeyboardInterrupt:
        pass
    finally:
        with lock:
            st["writer"].close()
        print("\nDone. Wrote %d messages%s."
              % (total[0], " across %d bags" % (st["index"] + 1) if args.split else ""))
    return 0


def _record_name(args, index=None):
    if args.output_name:
        base = args.output_name[:-4] if args.output_name.endswith(".bag") \
            else args.output_name
    else:
        stamp = datetime.datetime.now().strftime("%Y-%m-%d-%H-%M-%S")
        prefix = (args.output_prefix + "_") if args.output_prefix else ""
        base = "%s%s" % (prefix, stamp)
    if index is not None:
        base = "%s_%d" % (base, index)
    return base + ".bag"


# -------------------------------------------------------------------- play ----
def cmd_play(args):
    # Merge every message from every bag, in timestamp order.
    items = []                          # (secs, nsecs, topic, type, md5, def, body)
    for path in args.bags:
        if not os.path.exists(path):
            print("nr_rosbag: no such bag '%s'" % path, file=sys.stderr)
            return 1
        r = BagReader(path)
        for m in r.read_messages():
            info = r._conns.get(m.conn, {})
            items.append((m.secs, m.nsecs, m.topic, info.get("type", ""),
                          info.get("md5", ""), info.get("definition", ""), m.data))
        r.close()
    if not items:
        print("nr_rosbag: nothing to play")
        return 0
    items.sort(key=lambda x: (x[0], x[1]))

    _node("nr_rosbag")
    pubs = {}
    for _s, _n, topic, type_name, _md5, definition, _body in items:
        if topic in pubs:
            continue
        cls = get_message_class(type_name)
        if cls is None and definition:
            try:
                cls = define_message_from_definition(type_name, definition)
            except Exception as e:  # noqa
                print("nr_rosbag: cannot rebuild %s: %s" % (type_name, e),
                      file=sys.stderr)
        if cls is None:
            print("nr_rosbag: skipping %s (unknown type %s, no definition)"
                  % (topic, type_name), file=sys.stderr)
            continue
        pubs[topic] = irap_noroslib.Publisher(topic, cls, latch=False)

    rate = args.rate or 1.0
    print("Waiting %.1fs for subscribers..." % args.delay)
    time.sleep(args.delay)

    loops = 0
    try:
        while not irap_noroslib.is_shutdown():
            print("Playing %d messages from %d bag(s)%s"
                  % (len(items), len(args.bags),
                     " (loop %d)" % (loops + 1) if args.loop else ""))
            t0 = items[0][0] + items[0][1] * 1e-9
            wall0 = time.time()
            for secs, nsecs, topic, _ty, _md5, _def, body in items:
                if irap_noroslib.is_shutdown():
                    break
                pub = pubs.get(topic)
                if pub is None:
                    continue
                target = ((secs + nsecs * 1e-9) - t0) / rate
                now = time.time() - wall0
                if target > now:
                    time.sleep(target - now)
                pub._pub.publish(body)          # raw bytes straight to the wire
            loops += 1
            if not args.loop:
                break
    except KeyboardInterrupt:
        pass
    print("Done.")
    return 0


# -------------------------------------------------------------------- info ----
def cmd_info(args):
    rc = 0
    for path in args.bags:
        if not os.path.exists(path):
            print("nr_rosbag: no such bag '%s'" % path, file=sys.stderr)
            rc = 1
            continue
        _info_one(path)
    return rc


def _info_one(path):
    r = BagReader(path)
    conns = r.connections()
    infos = r.chunk_infos()
    size = os.path.getsize(path)
    # Aggregate per-connection message counts + overall time span from chunk info.
    per_conn = {}
    start = end = None
    for _pos, s, e, counts in infos:
        for cid, n in counts.items():
            per_conn[cid] = per_conn.get(cid, 0) + n
        st = s[0] + s[1] * 1e-9
        et = e[0] + e[1] * 1e-9
        start = st if start is None else min(start, st)
        end = et if end is None else max(end, et)
    total_msgs = sum(per_conn.values())
    # types present
    types = {}
    for cid, c in conns.items():
        types[c["type"]] = c["md5"]

    print("path:        %s" % path)
    print("version:     2.0")
    if start is not None:
        dur = end - start
        print("duration:    %s" % _fmt_dur(dur))
        print("start:       %s (%.2f)" % (_fmt_time(start), start))
        print("end:         %s (%.2f)" % (_fmt_time(end), end))
    print("size:        %s" % _fmt_size(size))
    print("messages:    %d" % total_msgs)
    print("compression: none [%d chunk(s)]" % len(infos))
    print("types:       " + ("\n             ".join(
        "%s [%s]" % (t, m) for t, m in sorted(types.items())) or "(none)"))
    # topics: name, count, type
    lines = []
    for cid, c in sorted(conns.items(), key=lambda kv: kv[1]["topic"]):
        lines.append("%-28s %6d msgs : %s"
                     % (c["topic"], per_conn.get(cid, 0), c["type"]))
    print("topics:      " + ("\n             ".join(lines) or "(none)"))
    r.close()


def _fmt_dur(sec):
    if sec < 60:
        return "%.1fs" % sec
    m, s = divmod(sec, 60)
    return "%d:%04.1fs (%.1fs)" % (int(m), s, sec)


def _fmt_time(t):
    return datetime.datetime.fromtimestamp(t).strftime("%b %d %Y %H:%M:%S.%f")[:-3]


def _fmt_size(n):
    for unit in ("B", "KB", "MB", "GB"):
        if n < 1024:
            return "%.1f %s" % (n, unit)
        n /= 1024.0
    return "%.1f TB" % n


# ------------------------------------------------------------------ main ----
def main(argv=None):
    p = argparse.ArgumentParser(
        prog="nr_rosbag",
        description="rosbag, with no ROS installed: record / play / info. "
                    "Records ANY topic (custom types included) to real v2.0 bags.")
    p.add_argument("--master", metavar="HOST|URI")
    p.add_argument("--port", type=int)
    p.add_argument("--host")
    p.add_argument("--set_ros_master_uri", metavar="URI",
                   help="remember this master URI in ./master.yaml and exit")
    p.add_argument("--set_ros_hostname", metavar="HOST",
                   help="remember this hostname in ./master.yaml and exit")
    sub = p.add_subparsers(dest="cmd")

    s = sub.add_parser("record", help="record topics to a bag")
    s.add_argument("topics", nargs="*")
    s.add_argument("-a", "--all", action="store_true", help="record all topics")
    s.add_argument("-O", "--output-name", metavar="NAME", help="output bag name")
    s.add_argument("-o", "--output-prefix", metavar="PREFIX",
                   help="prefix, then a timestamp")
    s.add_argument("--split", action="store_true",
                   help="split into multiple bags (base_0.bag, base_1.bag, ...)")
    s.add_argument("--size", type=float, metavar="MB",
                   help="with --split: roll to a new bag at this size (MB)")
    s.add_argument("--duration", type=float, metavar="SEC",
                   help="with --split: roll to a new bag every SEC seconds")
    s.set_defaults(func=cmd_record)

    s = sub.add_parser("play", help="play back one or more bags")
    s.add_argument("bags", nargs="+")
    s.add_argument("-r", "--rate", type=float, default=1.0, help="playback rate")
    s.add_argument("-l", "--loop", action="store_true", help="loop forever")
    s.add_argument("-d", "--delay", type=float, default=0.2,
                   help="wait this long for subscribers before playing")
    s.add_argument("--clock", action="store_true", help="(accepted; no /clock yet)")
    s.set_defaults(func=cmd_play)

    s = sub.add_parser("info", help="summarize one or more bags")
    s.add_argument("bags", nargs="+")
    s.set_defaults(func=cmd_info)

    args = p.parse_args(argv)

    if args.set_ros_master_uri or args.set_ros_hostname:
        uri = args.set_ros_master_uri
        if uri:
            uri = resolve_master(uri, args.port, saved={})
        path, cur = save_master_yaml(uri, args.set_ros_hostname)
        print("saved to %s" % path)
        for key in ("ros_master_uri", "ros_hostname"):
            if key in cur:
                print("  %-15s %s" % (key + ":", cur[key]))
        return 0

    if not args.cmd:
        p.print_help()
        return 0

    # info needs no master; record/play do.
    if args.cmd != "info":
        saved = load_master_yaml()
        uri = resolve_master(args.master, args.port, saved)
        irap_noroslib.set_log_level("warn")
        irap_noroslib.set_master_uri(uri)
        irap_noroslib.set_hostname(
            args.host or saved.get("ros_hostname") or os.environ.get("ROS_HOSTNAME")
            or os.environ.get("ROS_IP") or "localhost")
    try:
        return args.func(args)
    except KeyboardInterrupt:
        return 0
    except (socket.gaierror, ConnectionError, OSError) as e:
        print("nr_rosbag: cannot reach the ROS master (%s). Start one with nr_roscore."
              % e, file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
