"""nr_rosnode -- `rosnode`, without ROS installed.

Same subcommands, same arguments, same output shape as the real thing:

    nr_rosnode list [-a] [-u]
    nr_rosnode info   /talker
    nr_rosnode ping [-c N] [--all] /talker
    nr_rosnode machine [HOST]
    nr_rosnode kill   /talker [/talker2 ...]
    nr_rosnode cleanup

Node names, and which topics/services each node has, come from the master
(getSystemState). Everything else -- pid, connections, liveness, shutdown --
comes from calling the node's own slave API directly (getPid / getBusInfo /
shutdown), exactly as `rosnode` does.

Point it at a master with $ROS_MASTER_URI, or `--master http://host:11311`.
It shares ./master.yaml with nr_rostopic -- set the master once for all tools.
"""
import argparse
import os
import socket
import sys
import time
import xmlrpc.client as xmlrpc
from urllib.parse import urlparse

import irap_noroslib
from . import msg as _msg  # noqa: F401
# Reuse nr_rostopic's master/./master.yaml plumbing, so every nr_* tool agrees.
from .rostopic import (
    resolve_master, load_master_yaml, save_master_yaml, _node,
)

_PING_TIMEOUT = 3.0


# ------------------------------------------------------------ discovery ----
def _system_state():
    """[pubs, subs, srvs], each [[name, [node, ...]], ...]."""
    return _node("nr_rosnode").master.get_system_state()


def _all_nodes():
    """Every node name the master knows, from pubs, subs and services."""
    pubs, subs, srvs = _system_state()
    names = set()
    for section in (pubs, subs, srvs):
        for _name, providers in section:
            names.update(providers)
    return sorted(names)


def _node_api(name):
    """The node's slave-API URI (http://host:port), or None if unknown."""
    try:
        return _node("nr_rosnode").master.lookup_node(name)
    except Exception:  # noqa -- MasterError: no such node
        return None


def _slave(uri):
    return xmlrpc.ServerProxy(uri, allow_none=True)


def _node_regs(name):
    """(publications, subscriptions, services) topic-name lists for one node."""
    pubs, subs, srvs = _system_state()

    def owned(section):
        return sorted(n for n, providers in section if name in providers)
    return owned(pubs), owned(subs), owned(srvs)


def _topic_types():
    return dict(_node("nr_rosnode").master.get_topic_types())


# -------------------------------------------------------------- commands ----
def cmd_list(args):
    nodes = _all_nodes()
    if args.all or args.url:
        for n in nodes:
            uri = _node_api(n) or "(unknown)"
            if args.url and not args.all:
                print(uri)
            else:
                print("%-30s %s" % (n, uri))
    else:
        for n in nodes:
            print(n)
    return 0


def cmd_info(args):
    name = args.node
    if name not in _all_nodes():
        print("ERROR: Unknown node %s" % name, file=sys.stderr)
        return 1
    types = _topic_types()
    pubs, subs, srvs = _node_regs(name)
    print("-" * 80)
    print("Node [%s]" % name)
    print("\nPublications: ")
    for t in pubs:
        print(" * %s [%s]" % (t, types.get(t, "unknown type")))
    if not pubs:
        print(" * None")
    print("\nSubscriptions: ")
    for t in subs:
        print(" * %s [%s]" % (t, types.get(t, "unknown type")))
    if not subs:
        print(" * None")
    print("\nServices: ")
    for t in srvs:
        print(" * %s" % t)
    if not srvs:
        print(" * None")
    print()

    uri = _node_api(name)
    if not uri:
        print("cannot contact [%s]: not registered with an API uri" % name)
        return 0
    print("contacting node %s ..." % uri)
    try:
        proxy = _slave(uri)
        _c, _s, pid = proxy.getPid("/nr_rosnode")
        print("Pid: %s" % pid)
        _c, _s, businfo = proxy.getBusInfo("/nr_rosnode")
        print("Connections:")
        if not businfo:
            print(" * None")
        for conn in businfo:
            # [connId, destId, direction, transport, topic, connected, ...]
            direction = conn[2] if len(conn) > 2 else "?"
            topic = conn[4] if len(conn) > 4 else "?"
            dest = conn[1] if len(conn) > 1 else "?"
            print(" * topic: %s" % topic)
            print("    * to: %s" % dest)
            print("    * direction: %s" % direction)
    except (OSError, socket.error, xmlrpc.Fault, xmlrpc.ProtocolError) as e:
        print("cannot contact [%s]: %s" % (name, e))
    print()
    return 0


def cmd_ping(args):
    if args.all:
        targets = _all_nodes()
    else:
        targets = [args.node]
    rc = 0
    for name in targets:
        uri = _node_api(name)
        if not uri:
            print("ERROR: node [%s] is unknown or unreachable" % name, file=sys.stderr)
            rc = 1
            continue
        print("rosnode: node is [%s]" % name)
        print("pinging %s with a timeout of %.1fs" % (name, _PING_TIMEOUT))
        proxy = _slave_with_timeout(uri, _PING_TIMEOUT)
        n = 0
        acc = 0.0
        try:
            while args.count == 0 or n < args.count:
                t0 = time.time()
                try:
                    proxy.getPid("/nr_rosnode")
                except (OSError, socket.error, xmlrpc.Fault, xmlrpc.ProtocolError) as e:
                    print("connection to [%s] timed out: %s" % (name, e))
                    rc = 1
                    break
                dt = (time.time() - t0) * 1000.0
                acc += dt
                n += 1
                print("xmlrpc reply from %s\ttime=%.6fms" % (uri, dt))
                if args.count == 0 or n < args.count:
                    time.sleep(1.0)
        except KeyboardInterrupt:
            pass
        if n:
            print("\nping average: %.6fms" % (acc / n))
    return rc


def cmd_machine(args):
    nodes = _all_nodes()
    host_to_nodes = {}
    for n in nodes:
        uri = _node_api(n)
        host = urlparse(uri).hostname if uri else "(unknown)"
        host_to_nodes.setdefault(host, []).append(n)
    if args.machine:
        # nodes on this machine
        for n in sorted(host_to_nodes.get(args.machine, [])):
            print(n)
    else:
        for host in sorted(host_to_nodes):
            print(host)
    return 0


def cmd_kill(args):
    names = args.nodes
    if args.all:
        names = _all_nodes()
    killed = 0
    for name in names:
        uri = _node_api(name)
        if not uri:
            print("ERROR: Unknown node(s) specified: %s" % name, file=sys.stderr)
            continue
        try:
            _slave_with_timeout(uri, _PING_TIMEOUT).shutdown(
                "/nr_rosnode", "kill requested via nr_rosnode")
        except (OSError, socket.error, xmlrpc.Fault, xmlrpc.ProtocolError):
            # A node often drops the connection as it shuts down, before it can
            # reply -- so a transport error here is expected, not a failure. What
            # matters is whether it is actually gone now.
            pass
        time.sleep(0.3)
        if _alive(uri):
            print("ERROR: could not kill %s (still alive)" % name, file=sys.stderr)
        else:
            print("killed %s" % name)
            killed += 1
    return 0 if killed else 1


def cmd_cleanup(args):
    """Purge master registrations of nodes that no longer answer their slave API.
    Mirrors `rosnode cleanup`: contact every node; unregister the dead ones."""
    pubs, subs, srvs = _system_state()
    nodes = _all_nodes()
    master_uri = _node("nr_rosnode").master.uri
    unreachable = []
    for name in nodes:
        uri = _node_api(name)
        if not uri or not _alive(uri):
            unreachable.append((name, uri))
    if not unreachable:
        print("No unreachable nodes to purge.")
        return 0
    print("Unable to contact the following nodes:")
    for name, _uri in unreachable:
        print(" * %s" % name)
    print("\nUnregistering their topics and services from the master ...")
    m = xmlrpc.ServerProxy(master_uri, allow_none=True)
    for name, uri in unreachable:
        api = uri or ""
        for topic, providers in pubs:
            if name in providers:
                _safe(m.unregisterPublisher, name, topic, api)
        for topic, providers in subs:
            if name in providers:
                _safe(m.unregisterSubscriber, name, topic, api)
        for service, providers in srvs:
            if name in providers:
                # unregisterService(caller_id, service, service_api). We don't
                # know the rosrpc api; look it up, else skip.
                try:
                    svc_api = _node("nr_rosnode").master.lookup_service(service)
                except Exception:  # noqa
                    svc_api = api
                _safe(m.unregisterService, name, service, svc_api)
    print("done.")
    return 0


# ----------------------------------------------------------- helpers ----
def _slave_with_timeout(uri, timeout):
    """A ServerProxy whose socket honours `timeout` (stdlib default is blocking)."""
    return xmlrpc.ServerProxy(uri, allow_none=True,
                              transport=_TimeoutTransport(timeout))


class _TimeoutTransport(xmlrpc.Transport):
    def __init__(self, timeout):
        super().__init__()
        self._timeout = timeout

    def make_connection(self, host):
        conn = super().make_connection(host)
        conn.timeout = self._timeout
        return conn


def _alive(uri):
    try:
        _slave_with_timeout(uri, _PING_TIMEOUT).getPid("/nr_rosnode")
        return True
    except (OSError, socket.error, xmlrpc.Fault, xmlrpc.ProtocolError):
        return False


def _safe(fn, *a):
    try:
        fn(*a)
    except Exception:  # noqa -- best-effort purge
        pass


# ------------------------------------------------------------------ main ----
def main(argv=None):
    p = argparse.ArgumentParser(
        prog="nr_rosnode",
        description="rosnode, with no ROS installed. Node info comes from the "
                    "master; liveness/pid/kill come from each node's slave API.")
    p.add_argument("--master", metavar="HOST|URI",
                   help="master host, IP, host:port or full URI "
                        "(default: ./master.yaml, then $ROS_MASTER_URI, else localhost)")
    p.add_argument("--port", type=int, help="master port (default: 11311)")
    p.add_argument("--host", help="our hostname -- how other nodes reach us")
    p.add_argument("--set_ros_master_uri", metavar="URI",
                   help="remember this master URI in ./master.yaml and exit")
    p.add_argument("--set_ros_hostname", metavar="HOST",
                   help="remember this hostname in ./master.yaml and exit")
    sub = p.add_subparsers(dest="cmd")

    s = sub.add_parser("list", help="list active nodes")
    s.add_argument("-a", "--all", action="store_true", help="list node name and URI")
    s.add_argument("-u", "--url", action="store_true", help="list node URIs")
    s.set_defaults(func=cmd_list)

    s = sub.add_parser("info", help="print information about a node")
    s.add_argument("node")
    s.set_defaults(func=cmd_info)

    s = sub.add_parser("ping", help="test connectivity to a node")
    s.add_argument("node", nargs="?")
    s.add_argument("-c", "--count", type=int, default=0, help="ping N times, then stop")
    s.add_argument("-a", "--all", action="store_true", help="ping all nodes")
    s.set_defaults(func=cmd_ping)

    s = sub.add_parser("machine", help="list nodes on a machine, or list machines")
    s.add_argument("machine", nargs="?")
    s.set_defaults(func=cmd_machine)

    s = sub.add_parser("kill", help="kill a running node")
    s.add_argument("nodes", nargs="*")
    s.add_argument("-a", "--all", action="store_true", help="kill all nodes")
    s.set_defaults(func=cmd_kill)

    s = sub.add_parser("cleanup", help="purge unreachable nodes from the master")
    s.set_defaults(func=cmd_cleanup)

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
        print("\nnr_rosnode in this directory now uses these -- no flags needed.")
        return 0

    if not args.cmd:
        p.print_help()
        return 0
    if args.cmd == "ping" and not args.node and not args.all:
        print("nr_rosnode ping: give a NODE, or --all", file=sys.stderr)
        return 1
    if args.cmd == "kill" and not args.nodes and not args.all:
        print("nr_rosnode kill: give one or more NODEs, or --all", file=sys.stderr)
        return 1

    saved = load_master_yaml()
    uri = resolve_master(args.master, args.port, saved)
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
        print("nr_rosnode: cannot reach the ROS master at %s\n"
              "            (from %s)\n"
              "            %s\n\n"
              "Is a master running? Start one with:  nr_roscore"
              % (uri, origin, e), file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
