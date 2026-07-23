"""nr_rosservice -- `rosservice`, without ROS installed.

Same subcommands, same arguments, same output shape as the real thing:

    nr_rosservice list [-v]
    nr_rosservice type  /add_two_ints
    nr_rosservice uri   /add_two_ints
    nr_rosservice info  /add_two_ints
    nr_rosservice find  rospy_tutorials/AddTwoInts
    nr_rosservice args  /add_two_ints
    nr_rosservice call [-f service.srv] /add_two_ints "a: 3, b: 4"

`type` / `uri` / `info` / `find` work for ANY service: the type and md5 are not
kept by the master, so nr_rosservice probes the running service directly (the
same handshake `rosservice` uses) to learn them -- no .srv file needed.

`args` and `call` need the request's field layout, which a service does NOT send
over the wire. Point them at the `.srv` file with -f/--srv-file, or use a
built-in (std_srvs) or one already loaded with irap_noroslib.load_srv_file().

Point it at a master with $ROS_MASTER_URI, or `--master http://host:11311`.
It shares ./master.yaml with nr_rostopic -- set the master once for both.
"""
import argparse
import os
import socket
import sys

import irap_noroslib
from . import msg as _msg  # noqa: F401  -- registers the built-in catalog
from . import srv as _srv  # noqa: F401  -- registers built-in std_srvs
from .srv import get_service_class
from .msgfile import load_srv_file
# Reuse nr_rostopic's master/./master.yaml plumbing and YAML-ish data parser,
# so the two tools behave identically and share one saved master.
from .rostopic import (
    resolve_master, load_master_yaml, save_master_yaml, master_yaml_path,
    _apply_yaml, _node,
)


# ------------------------------------------------------------ discovery ----
def _services():
    """[[name, [provider_node, ...]], ...] from the master."""
    _pubs, _subs, srvs = _node("nr_rosservice").master.get_system_state()
    return {name: nodes for name, nodes in srvs}


def _probe(name):
    """Reply header from the running service: type / md5sum / request_type ..."""
    _node("nr_rosservice")
    return irap_noroslib.probe_service(name)


def _service_class(full_type, srv_file):
    """Turn a service type into its Request/Response classes, or None.

    A .srv file (-f) wins; otherwise fall back to whatever is registered
    (std_srvs, or anything already load_srv_file()'d).
    """
    if srv_file:
        return load_srv_file(srv_file)
    return get_service_class(full_type)


# -------------------------------------------------------------- commands ----
def cmd_list(args):
    services = _services()
    if not args.verbose:
        for name in sorted(services):
            print(name)
        return 0
    for name in sorted(services):
        nodes = services[name] or ["?"]
        print(" * %s [%s]" % (name, ", ".join(nodes)))
    return 0


def cmd_type(args):
    print(_probe(args.service)["type"])
    return 0


def cmd_uri(args):
    print(_node("nr_rosservice").master.lookup_service(args.service))
    return 0


def cmd_info(args):
    services = _services()
    if args.service not in services:
        print("Unknown service %s" % args.service, file=sys.stderr)
        return 1
    node = _node("nr_rosservice")
    hdr = _probe(args.service)
    print("Node: %s" % ", ".join(services[args.service] or ["?"]))
    print("URI: %s" % node.master.lookup_service(args.service))
    print("Type: %s" % hdr["type"])
    print("Args: %s" % " ".join(_request_args(hdr["type"], args.srv_file)))
    return 0


def cmd_find(args):
    for name in sorted(_services()):
        try:
            if _probe(name)["type"] == args.type:
                print(name)
        except Exception:  # noqa -- a service that vanished mid-scan, skip it
            continue
    return 0


def cmd_args(args):
    hdr = _probe(args.service)
    names = _request_args(hdr["type"], args.srv_file, strict=True)
    print(" ".join(names))
    return 0


def cmd_call(args):
    hdr = _probe(args.service)
    full_type = hdr["type"]
    cls = _service_class(full_type, args.srv_file)
    if cls is None:
        print("nr_rosservice: don't know the request layout for [%s].\n"
              "               A service doesn't send its .srv over the wire, so\n"
              "               point me at the file:  -f path/to/Type.srv\n"
              "               (std_srvs and already-loaded types need no file.)"
              % full_type, file=sys.stderr)
        return 1
    req = cls.Request()
    if args.data:
        try:
            _apply_yaml(req, " ".join(args.data))
        except Exception as e:  # noqa
            print("could not parse the request data: %s" % e, file=sys.stderr)
            return 1
    try:
        resp = irap_noroslib.ServiceProxy(args.service, cls).call(req)
    except irap_noroslib.ServiceException as e:
        print("nr_rosservice: call failed: %s" % e, file=sys.stderr)
        return 1
    from .rostopic import _fmt
    print(_fmt(resp))
    return 0


# ----------------------------------------------------------- helpers ----
def _request_args(full_type, srv_file, strict=False):
    """Field names of a service's request, or [] if the layout is unknown."""
    cls = _service_class(full_type, srv_file)
    if cls is None:
        if strict:
            print("nr_rosservice: unknown request layout for [%s]; give -f Type.srv"
                  % full_type, file=sys.stderr)
        return []
    return [f.name for f in cls.Request()._spec.fields if not f.is_constant]


# ------------------------------------------------------------------ main ----
def main(argv=None):
    p = argparse.ArgumentParser(
        prog="nr_rosservice",
        description="rosservice, with no ROS installed. type/uri/info/find probe "
                    "any running service; args/call need the .srv (-f).")
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

    s = sub.add_parser("list", help="list active services")
    s.add_argument("-v", "--verbose", action="store_true")
    s.set_defaults(func=cmd_list)

    s = sub.add_parser("type", help="print a service's type")
    s.add_argument("service")
    s.set_defaults(func=cmd_type)

    s = sub.add_parser("uri", help="print a service's ROSRPC uri")
    s.add_argument("service")
    s.set_defaults(func=cmd_uri)

    s = sub.add_parser("info", help="node, uri, type and args")
    s.add_argument("service")
    s.add_argument("-f", "--srv-file", help=".srv file, so Args can be shown")
    s.set_defaults(func=cmd_info)

    s = sub.add_parser("find", help="services of a given type")
    s.add_argument("type")
    s.set_defaults(func=cmd_find)

    s = sub.add_parser("args", help="print a service's request arguments")
    s.add_argument("service")
    s.add_argument("-f", "--srv-file", help=".srv file describing the request")
    s.set_defaults(func=cmd_args)

    s = sub.add_parser("call", help="call the service with the provided args")
    s.add_argument("service")
    s.add_argument("data", nargs="*", help="request as 'a: 3, b: 4' or {a: 3, b: 4}")
    s.add_argument("-f", "--srv-file", help=".srv file describing request/response")
    s.set_defaults(func=cmd_call)

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
        print("\nnr_rosservice in this directory now uses these -- no flags needed.")
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
        print("nr_rosservice: cannot reach the ROS master at %s\n"
              "               (from %s)\n"
              "               %s\n\n"
              "Is a master running? Start one with:  nr_roscore\n"
              "Point at a different one, and remember it here:\n"
              "    nr_rosservice --set_ros_master_uri http://HOST:11311 "
              "--set_ros_hostname YOUR_IP"
              % (uri, origin, e), file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
