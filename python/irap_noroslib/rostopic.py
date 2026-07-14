"""nr_rostopic -- `rostopic`, without ROS installed.

Same subcommands, same arguments, same output shape as the real thing:

    nr_rostopic list [-v]
    nr_rostopic type   /chatter
    nr_rostopic info   /chatter
    nr_rostopic echo [-n N] [--noarr] /chatter
    nr_rostopic hz     /chatter
    nr_rostopic bw     /chatter
    nr_rostopic pub [-r HZ | -1] /chatter std_msgs/String "data: hi"
    nr_rostopic find   std_msgs/String

`echo` decodes **any** topic, including custom types you have no `.msg` file for:
a ROS publisher hands over the full message definition during the TCPROS
handshake, so the type is rebuilt on the spot. (Real `rostopic echo` cannot do
this -- it needs the message class built in a catkin package first.)

Point it at a master with $ROS_MASTER_URI, or `--master http://host:11311`.
"""
import argparse
import os
import socket
import sys
import threading
import time

import irap_noroslib
from . import msg as _msg  # noqa: F401  -- registers the built-in catalog
from .message import define_message_from_definition, get_message_class, registry

_ARRAY_ELIDE = 12          # like rostopic's --noarr, but only past this length


# ---------------------------------------------------------------- output ----
def _fmt(value, indent=0, noarr=False):
    """Render a message the way `rostopic echo` does (YAML-ish)."""
    pad = "  " * indent
    if hasattr(value, "_spec"):                     # a nested message
        out = []
        for f in value._spec.fields:
            if f.is_constant:
                continue
            v = getattr(value, f.name)
            if hasattr(v, "_spec"):
                out.append("%s%s: " % (pad, f.name))
                out.append(_fmt(v, indent + 1, noarr))
            elif isinstance(v, list) and v and hasattr(v[0], "_spec"):
                out.append("%s%s: " % (pad, f.name))
                if noarr:
                    out[-1] += "<array of %d>" % len(v)
                    continue
                for el in v:
                    out.append("%s  - " % pad)
                    out.append(_fmt(el, indent + 2, noarr))
            else:
                out.append("%s%s: %s" % (pad, f.name, _scalar(v, noarr)))
        return "\n".join(out)
    return "%s%s" % (pad, _scalar(value, noarr))


def _scalar(v, noarr=False):
    if isinstance(v, (bytes, bytearray)):
        if noarr or len(v) > _ARRAY_ELIDE:
            return "<%d bytes>" % len(v)
        return "[%s]" % ", ".join(str(b) for b in v)
    if isinstance(v, tuple) and len(v) == 2:        # time / duration
        return "\n    secs: %d\n    nsecs: %d" % v
    if isinstance(v, list):
        if noarr or len(v) > _ARRAY_ELIDE:
            return "<array of %d>" % len(v)
        return "[%s]" % ", ".join(str(x) for x in v)
    if isinstance(v, str):
        return '"%s"' % v
    return str(v)


# ----------------------------------------------------- remembered settings ----
# nr_rostopic remembers the master URI and hostname in a master.yaml in the
# CURRENT DIRECTORY, so you set them once and every later command just works:
#
#     nr_rostopic --set_ros_master_uri http://192.168.1.50:11311 \
#                 --set_ros_hostname   192.168.1.77
#     nr_rostopic list            # no flags needed any more
#
# The file WINS over $ROS_MASTER_URI / $ROS_IP / $ROS_HOSTNAME: a stale env var
# left in the shell by some other ROS setup must not silently override what you
# deliberately saved. An explicit --master/--port/--host still beats the file.
MASTER_YAML = "master.yaml"


def master_yaml_path():
    """The remembered-settings file: ./master.yaml, in the current directory."""
    return os.path.join(os.getcwd(), MASTER_YAML)


def load_master_yaml(path=None):
    """Read ./master.yaml. Returns {} if it isn't there or can't be read.

    Deliberately a 5-line parser, not PyYAML: irap_noroslib has zero dependencies,
    and this file only ever holds two `key: value` lines.
    """
    path = path or master_yaml_path()
    out = {}
    try:
        with open(path) as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith("#") or ":" not in line:
                    continue
                key, _, value = line.partition(":")
                value = value.strip().strip('"').strip("'")
                if value:
                    out[key.strip()] = value
    except (IOError, OSError):
        return {}
    return out


def save_master_yaml(master_uri=None, hostname=None, path=None):
    """Write ./master.yaml, keeping whichever value wasn't given this time."""
    path = path or master_yaml_path()
    cur = load_master_yaml(path)
    if master_uri:
        cur["ros_master_uri"] = master_uri
    if hostname:
        cur["ros_hostname"] = hostname
    with open(path, "w") as f:
        f.write("# nr_rostopic remembered settings. Delete this file to forget them.\n")
        f.write("# These win over $ROS_MASTER_URI / $ROS_IP / $ROS_HOSTNAME.\n")
        if "ros_master_uri" in cur:
            f.write("ros_master_uri: %s\n" % cur["ros_master_uri"])
        if "ros_hostname" in cur:
            f.write("ros_hostname: %s\n" % cur["ros_hostname"])
    return path, cur


# ------------------------------------------------------------ master glue ----
def resolve_master(master=None, port=None, saved=None):
    """Build the master URI from whatever the user gave us.

    `master` may be a full URI, a host:port, or just a host/IP -- so all of these
    mean the same thing:

        --master http://127.0.0.1:11311
        --master 127.0.0.1:11311
        --master 127.0.0.1 --port 11311
        --master 127.0.0.1                  (port defaults to 11311)

    Precedence: --master/--port > ./master.yaml > $ROS_MASTER_URI > localhost.
    """
    saved = load_master_yaml() if saved is None else saved
    uri = (master or saved.get("ros_master_uri")
           or os.environ.get("ROS_MASTER_URI") or "http://localhost:11311")
    if "://" not in uri:
        uri = "http://" + uri
    scheme, _, rest = uri.partition("://")
    rest = rest.rstrip("/")
    host, colon, uri_port = rest.partition(":")
    if port:                       # --port always wins
        uri_port = str(port)
    elif not colon:
        uri_port = "11311"
    return "%s://%s:%s" % (scheme, host, uri_port)


_inited = []


def _node(name):
    """init_node once, under a unique name (several nr_rostopic's can coexist)."""
    if not _inited:
        irap_noroslib.init_node("%s_%d" % (name, os.getpid()))
        _inited.append(True)
    return irap_noroslib.get_node()


def _topic_types():
    return dict(_node("nr_rostopic").master.get_topic_types())


def _system_state():
    pubs, subs, _srvs = _node("nr_rostopic").master.get_system_state()
    return {t: n for t, n in pubs}, {t: n for t, n in subs}


# -------------------------------------------------------------- commands ----
def cmd_list(args):
    types = _topic_types()
    if not args.verbose:
        for t in sorted(types):
            print(t)
        return 0
    pubs, subs = _system_state()
    print("\nPublished topics:")
    for t in sorted(pubs):
        print(" * %s [%s] %d publisher(s)" % (t, types.get(t, "?"), len(pubs[t])))
    print("\nSubscribed topics:")
    for t in sorted(subs):
        print(" * %s [%s] %d subscriber(s)" % (t, types.get(t, "?"), len(subs[t])))
    print()
    return 0


def cmd_type(args):
    t = _topic_types().get(args.topic)
    if not t:
        print("Unknown topic %s" % args.topic, file=sys.stderr)
        return 1
    print(t)
    return 0


def cmd_info(args):
    types = _topic_types()
    if args.topic not in types:
        print("Unknown topic %s" % args.topic, file=sys.stderr)
        return 1
    pubs, subs = _system_state()
    print("Type: %s\n" % types[args.topic])
    print("Publishers: ")
    for n in pubs.get(args.topic, []) or []:
        print(" * %s" % n)
    if not pubs.get(args.topic):
        print(" * None")
    print("\nSubscribers: ")
    for n in subs.get(args.topic, []) or []:
        print(" * %s" % n)
    if not subs.get(args.topic):
        print(" * None")
    print()
    return 0


def cmd_find(args):
    for t, ty in sorted(_topic_types().items()):
        if ty == args.type:
            print(t)
    return 0


def cmd_echo(args):
    n = 0
    stop = threading.Event()

    def cb(m):
        nonlocal n
        if isinstance(m, (bytes, bytearray)):       # no definition available
            print("<%d raw bytes -- publisher sent no message_definition>" % len(m))
        else:
            print(_fmt(m, noarr=args.noarr))
        print("---")
        sys.stdout.flush()
        n += 1
        if args.count and n >= args.count:
            stop.set()

    _node("nr_rostopic")
    # data_class=None: adopt whatever the publisher is -- type, md5 AND definition
    irap_noroslib.Subscriber(args.topic, None, cb)
    try:
        while not stop.is_set() and not irap_noroslib.is_shutdown():
            stop.wait(0.1)
    except KeyboardInterrupt:
        pass
    return 0


def cmd_hz(args):
    times, lock = [], threading.Lock()

    def cb(_m):
        with lock:
            times.append(time.time())

    _node("nr_rostopic")
    irap_noroslib.Subscriber(args.topic, None, cb)
    print("subscribed to [%s]" % args.topic, flush=True)
    try:
        while not irap_noroslib.is_shutdown():
            time.sleep(1.0)
            with lock:
                t = times[:]
                del times[:]
            if len(t) < 2:
                print("no new messages", flush=True)
                continue
            deltas = [t[i + 1] - t[i] for i in range(len(t) - 1)]
            mean = sum(deltas) / len(deltas)
            var = sum((d - mean) ** 2 for d in deltas) / len(deltas)
            print("average rate: %.3f\n\tmin: %.3fs max: %.3fs std dev: %.5fs window: %d"
                  % (1.0 / mean if mean else 0, min(deltas), max(deltas),
                     var ** 0.5, len(t)), flush=True)
    except KeyboardInterrupt:
        pass
    return 0


def cmd_bw(args):
    sizes, lock = [], threading.Lock()
    # subscribe raw: we only need the byte count, not the decode
    sub = _RawByteCounter(args.topic, sizes, lock)
    print("subscribed to [%s]" % args.topic, flush=True)
    try:
        while not irap_noroslib.is_shutdown():
            time.sleep(1.0)
            with lock:
                s = sizes[:]
                del sizes[:]
            if not s:
                print("no new messages", flush=True)
                continue
            total = sum(s)
            print("average: %s/s\n\tmean: %s min: %s max: %s window: %d"
                  % (_hsize(total), _hsize(total // len(s)), _hsize(min(s)),
                     _hsize(max(s)), len(s)), flush=True)
    except KeyboardInterrupt:
        pass
    finally:
        del sub
    return 0


def _hsize(n):
    for unit in ("B", "KB", "MB", "GB"):
        if n < 1024:
            return "%.2f%s" % (n, unit)
        n /= 1024.0
    return "%.2fTB" % n


class _RawByteCounter:
    """Counts bytes per message without decoding (bw doesn't need the fields)."""

    def __init__(self, topic, sizes, lock):
        _node("nr_rostopic")

        def cb(m):
            data = m if isinstance(m, (bytes, bytearray)) else m.serialize()
            with lock:
                sizes.append(len(data))
        self.sub = irap_noroslib.Subscriber(topic, None, cb)


def cmd_pub(args):
    cls = get_message_class(args.type)
    if cls is None:
        print("Unknown message type [%s]. It is not a built-in; load its .msg "
              "first with irap_noroslib.load_msg_file(), or publish a built-in type."
              % args.type, file=sys.stderr)
        return 1
    m = cls()
    if args.data:
        try:
            _apply_yaml(m, " ".join(args.data))
        except Exception as e:  # noqa
            print("could not parse the message data: %s" % e, file=sys.stderr)
            return 1

    _node("nr_rostopic")
    pub = irap_noroslib.Publisher(args.topic, cls, latch=args.once)
    if args.once:
        # give the master + any subscriber a moment to connect before we exit
        deadline = time.time() + 3.0
        while pub.get_num_connections() == 0 and time.time() < deadline:
            time.sleep(0.05)
        pub.publish(m)
        time.sleep(0.5)
        return 0

    rate = irap_noroslib.Rate(args.rate or 10)
    try:
        while not irap_noroslib.is_shutdown():
            pub.publish(m)
            rate.sleep()
    except KeyboardInterrupt:
        pass
    return 0


def _apply_yaml(m, text):
    """Fill a message from rostopic-style data: '{a: 1, b: {c: 2}}' or 'data: hi'."""
    import ast
    text = text.strip()
    if not text:
        return
    if not text.startswith("{"):
        text = "{%s}" % text
    # rostopic accepts unquoted keys/strings; make it Python-literal-able
    d = _loose_dict(text)
    _fill(m, d)


def _loose_dict(text):
    """A tiny YAML-flow-mapping parser: {a: 1, b: [1,2], c: {d: hi}}."""
    i = [0]

    def skip():
        while i[0] < len(text) and text[i[0]] in " \t\n,":
            i[0] += 1

    def parse_value():
        skip()
        c = text[i[0]]
        if c == "{":
            return parse_map()
        if c == "[":
            return parse_list()
        if c in "\"'":
            q = c
            i[0] += 1
            start = i[0]
            while text[i[0]] != q:
                i[0] += 1
            s = text[start:i[0]]
            i[0] += 1
            return s
        start = i[0]
        while i[0] < len(text) and text[i[0]] not in ",}]":
            i[0] += 1
        tok = text[start:i[0]].strip()
        low = tok.lower()
        if low in ("true", "false"):
            return low == "true"
        try:
            return int(tok)
        except ValueError:
            pass
        try:
            return float(tok)
        except ValueError:
            pass
        return tok

    def parse_list():
        i[0] += 1                       # [
        out = []
        skip()
        while text[i[0]] != "]":
            out.append(parse_value())
            skip()
        i[0] += 1
        return out

    def parse_map():
        i[0] += 1                       # {
        out = {}
        skip()
        while text[i[0]] != "}":
            start = i[0]
            while text[i[0]] != ":":
                i[0] += 1
            key = text[start:i[0]].strip().strip("\"'")
            i[0] += 1                   # :
            out[key] = parse_value()
            skip()
        i[0] += 1
        return out

    return parse_map()


def _fill(m, d):
    for k, v in d.items():
        cur = getattr(m, k, None)
        if isinstance(v, dict) and hasattr(cur, "_spec"):
            _fill(cur, v)
        elif isinstance(v, list) and cur is not None and isinstance(cur, bytes):
            setattr(m, k, bytes(bytearray(v)))
        else:
            setattr(m, k, v)


# ------------------------------------------------------------------ main ----
def main(argv=None):
    p = argparse.ArgumentParser(
        prog="nr_rostopic",
        description="rostopic, with no ROS installed. `echo` decodes any topic -- "
                    "even a custom type you have no .msg file for.")
    p.add_argument("--master", metavar="HOST|URI",
                   help="master host, IP, host:port or full URI "
                        "(default: ./master.yaml, then $ROS_MASTER_URI, else localhost)")
    p.add_argument("--port", type=int,
                   help="master port (default: 11311, or the port in --master)")
    p.add_argument("--host",
                   help="our hostname -- how other nodes reach us "
                        "(default: ./master.yaml, then $ROS_HOSTNAME / $ROS_IP)")
    p.add_argument("--set_ros_master_uri", metavar="URI",
                   help="remember this master URI in ./master.yaml and exit")
    p.add_argument("--set_ros_hostname", metavar="HOST",
                   help="remember this hostname in ./master.yaml and exit")
    sub = p.add_subparsers(dest="cmd")

    s = sub.add_parser("list", help="list topics")
    s.add_argument("-v", "--verbose", action="store_true")
    s.set_defaults(func=cmd_list)

    s = sub.add_parser("type", help="print a topic's type")
    s.add_argument("topic")
    s.set_defaults(func=cmd_type)

    s = sub.add_parser("info", help="type, publishers and subscribers")
    s.add_argument("topic")
    s.set_defaults(func=cmd_info)

    s = sub.add_parser("find", help="topics of a given type")
    s.add_argument("type")
    s.set_defaults(func=cmd_find)

    s = sub.add_parser("echo", help="print messages (any type, no .msg needed)")
    s.add_argument("topic")
    s.add_argument("-n", "--count", type=int, help="stop after N messages")
    s.add_argument("--noarr", action="store_true", help="don't print array contents")
    s.set_defaults(func=cmd_echo)

    s = sub.add_parser("hz", help="publishing rate")
    s.add_argument("topic")
    s.set_defaults(func=cmd_hz)

    s = sub.add_parser("bw", help="bandwidth used")
    s.add_argument("topic")
    s.set_defaults(func=cmd_bw)

    s = sub.add_parser("pub", help="publish a message")
    s.add_argument("topic")
    s.add_argument("type")
    s.add_argument("data", nargs="*")
    s.add_argument("-r", "--rate", type=float, help="publish at HZ (default 10)")
    s.add_argument("-1", "--once", action="store_true", help="publish once and exit")
    s.set_defaults(func=cmd_pub)

    args = p.parse_args(argv)

    # --set_ros_master_uri / --set_ros_hostname: remember, then exit.
    if args.set_ros_master_uri or args.set_ros_hostname:
        uri = args.set_ros_master_uri
        if uri:
            uri = resolve_master(uri, args.port, saved={})   # normalise to a full URI
        path, cur = save_master_yaml(uri, args.set_ros_hostname)
        print("saved to %s" % path)
        for key in ("ros_master_uri", "ros_hostname"):
            if key in cur:
                print("  %-15s %s" % (key + ":", cur[key]))
        print("\nnr_rostopic in this directory now uses these -- no flags needed.")
        return 0

    if not args.cmd:
        p.print_help()
        return 0

    saved = load_master_yaml()
    uri = resolve_master(args.master, args.port, saved)

    # where did that master come from? -- so a failure can say so.
    if args.master:
        origin = "--master"
    elif saved.get("ros_master_uri"):
        origin = "./master.yaml"
    elif os.environ.get("ROS_MASTER_URI"):
        origin = "$ROS_MASTER_URI"
    else:
        origin = "the default"

    # a CLI prints its data, not the library's chatter (warnings still show)
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
        # The master is unreachable. Say which master, and where it came from --
        # a stale $ROS_MASTER_URI is the usual culprit, and a traceback hides that.
        print("nr_rostopic: cannot reach the ROS master at %s\n"
              "             (from %s)\n"
              "             %s\n\n"
              "Is a master running? Start one with:  nr_roscore\n"
              "Point at a different one, and remember it here:\n"
              "    nr_rostopic --set_ros_master_uri http://HOST:11311 "
              "--set_ros_hostname YOUR_IP"
              % (uri, origin, e), file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
