"""nr_roscore -- a standalone ROS master (roscore) with no ROS installed.

This is the *server* side of ROS: an XML-RPC master + parameter server that real
ROS nodes (rospy/roscpp) and irap_noroslib nodes register with. It implements the ROS
Master API (registerPublisher/Subscriber/Service, lookup*, getSystemState, ...)
and the Parameter Server API (get/set/has/delete/search/list + subscribe), and
notifies subscribers via `publisherUpdate` when a topic's publisher set changes.

Run it directly:

    python3 -m irap_noroslib.roscore                 # binds :11311, advertises this host
    python3 -m irap_noroslib.roscore --port 11322
    ROS_MASTER_URI=http://myhost:11311 ROS_HOSTNAME=myhost python3 -m irap_noroslib.roscore

Then point nodes at it with the matching ROS_MASTER_URI. Uses only the standard
library -- no ROS, no irap_noroslib client code.
"""
import os
import sys
import socket
import threading
import xmlrpc.client
from xmlrpc.server import SimpleXMLRPCServer, SimpleXMLRPCRequestHandler
from socketserver import ThreadingMixIn


# ---- registry -------------------------------------------------------------
class _Registry:
    """Who publishes/subscribes/serves what. All access under one lock."""

    def __init__(self):
        self.lock = threading.RLock()
        self.publishers = {}    # topic -> {caller_id: caller_api}
        self.subscribers = {}   # topic -> {caller_id: caller_api}
        self.services = {}      # service -> (caller_id, service_api)
        self.topic_types = {}   # topic -> type
        self.node_apis = {}     # caller_id -> caller_api (slave XML-RPC uri)

    def set_type(self, topic, ttype):
        if ttype and ttype != "*":
            self.topic_types.setdefault(topic, ttype)

    def pub_apis(self, topic):
        return list(self.publishers.get(topic, {}).values())

    def sub_apis(self, topic):
        return list(self.subscribers.get(topic, {}).values())


class _ParamServer:
    """A ROS-style hierarchical parameter tree keyed by '/'-separated names."""

    def __init__(self):
        self.lock = threading.RLock()
        self.tree = {}                 # nested dict
        self.subscriptions = {}        # key -> {caller_id: caller_api}

    @staticmethod
    def _split(key):
        return [p for p in key.strip("/").split("/") if p]

    def set(self, key, value):
        parts = self._split(key)
        with self.lock:
            if not parts:                       # setting root
                self.tree = value if isinstance(value, dict) else {}
                return
            node = self.tree
            for p in parts[:-1]:
                nxt = node.get(p)
                if not isinstance(nxt, dict):
                    nxt = {}
                    node[p] = nxt
                node = nxt
            node[parts[-1]] = value

    def get(self, key):
        parts = self._split(key)
        with self.lock:
            node = self.tree
            for p in parts:
                if not isinstance(node, dict) or p not in node:
                    raise KeyError(key)
                node = node[p]
            return node

    def has(self, key):
        try:
            self.get(key)
            return True
        except KeyError:
            return False

    def delete(self, key):
        parts = self._split(key)
        with self.lock:
            if not parts:
                self.tree = {}
                return
            node = self.tree
            for p in parts[:-1]:
                if not isinstance(node, dict) or p not in node:
                    raise KeyError(key)
                node = node[p]
            if not isinstance(node, dict) or parts[-1] not in node:
                raise KeyError(key)
            del node[parts[-1]]

    def names(self):
        out = []

        def walk(node, prefix):
            if isinstance(node, dict):
                for k, v in node.items():
                    walk(v, prefix + "/" + k)
            else:
                out.append(prefix)
        with self.lock:
            walk(self.tree, "")
        return out

    def search(self, ns, key):
        """Search a key upward from namespace ns (ROS searchParam)."""
        leaf = self._split(key)
        if not leaf:
            return None
        head = leaf[0]
        scope = self._split(ns)
        with self.lock:
            while True:
                cand = "/" + "/".join(scope + [head]) if scope else "/" + head
                if self.has(cand):
                    return "/" + "/".join(scope + leaf) if scope else "/" + "/".join(leaf)
                if not scope:
                    return None
                scope.pop()


# ---- master ---------------------------------------------------------------
OK, ERROR, FAILURE = 1, 0, -1


class RosMaster:
    """Implements the ROS Master + Parameter Server XML-RPC APIs."""

    def __init__(self, master_uri, quiet=False):
        self.uri = master_uri
        self.reg = _Registry()
        self.params = _ParamServer()
        self.quiet = quiet
        # roscore normally seeds these.
        self.params.set("/run_id", _make_run_id())
        self.params.set("/rosdistro", "irap_noroslib\n")
        self.params.set("/rosversion", "irap_noroslib 0.1.0\n")

    def _log(self, msg):
        if not self.quiet:
            print("[nr_roscore] " + msg, flush=True)

    # -- helpers -----------------------------------------------------------
    def _notify_subscribers(self, topic):
        """Call publisherUpdate on every subscriber of `topic`."""
        with self.reg.lock:
            subs = self.reg.sub_apis(topic)
            pubs = self.reg.pub_apis(topic)
        for api in subs:
            threading.Thread(target=self._publisher_update, args=(api, topic, pubs),
                             daemon=True).start()

    def _publisher_update(self, sub_api, topic, pub_apis):
        try:
            proxy = xmlrpc.client.ServerProxy(sub_api, allow_none=True)
            proxy._ServerProxy__transport.timeout = 3.0  # best-effort
            proxy.publisherUpdate("/master", topic, pub_apis)
        except Exception:
            pass  # subscriber may be gone; ignore

    def _notify_param(self, key, value):
        with self.params.lock:
            subs = dict(self.params.subscriptions.get(key, {}))
        for api in subs.values():
            threading.Thread(target=self._param_update, args=(api, key, value),
                             daemon=True).start()

    def _param_update(self, api, key, value):
        try:
            proxy = xmlrpc.client.ServerProxy(api, allow_none=True)
            proxy.paramUpdate("/master", key, value)
        except Exception:
            pass

    # -- master API --------------------------------------------------------
    def getUri(self, caller_id):
        return [OK, "", self.uri]

    def getPid(self, caller_id):
        return [OK, "", os.getpid()]

    def registerPublisher(self, caller_id, topic, topic_type, caller_api):
        with self.reg.lock:
            self.reg.publishers.setdefault(topic, {})[caller_id] = caller_api
            self.reg.node_apis[caller_id] = caller_api
            self.reg.set_type(topic, topic_type)
            subs = self.reg.sub_apis(topic)
        self._log("+pub  %-30s %-24s by %s" % (topic, topic_type, caller_id))
        self._notify_subscribers(topic)
        return [OK, "registered", subs]

    def unregisterPublisher(self, caller_id, topic, caller_api):
        with self.reg.lock:
            n = self.reg.publishers.get(topic, {})
            existed = n.pop(caller_id, None) is not None
        if existed:
            self._log("-pub  %-30s by %s" % (topic, caller_id))
            self._notify_subscribers(topic)
        return [OK, "", 1 if existed else 0]

    def registerSubscriber(self, caller_id, topic, topic_type, caller_api):
        with self.reg.lock:
            self.reg.subscribers.setdefault(topic, {})[caller_id] = caller_api
            self.reg.node_apis[caller_id] = caller_api
            self.reg.set_type(topic, topic_type)
            pubs = self.reg.pub_apis(topic)
        self._log("+sub  %-30s %-24s by %s" % (topic, topic_type, caller_id))
        return [OK, "registered", pubs]

    def unregisterSubscriber(self, caller_id, topic, caller_api):
        with self.reg.lock:
            n = self.reg.subscribers.get(topic, {})
            existed = n.pop(caller_id, None) is not None
        if existed:
            self._log("-sub  %-30s by %s" % (topic, caller_id))
        return [OK, "", 1 if existed else 0]

    def registerService(self, caller_id, service, service_api, caller_api):
        with self.reg.lock:
            self.reg.services[service] = (caller_id, service_api)
            self.reg.node_apis[caller_id] = caller_api
        self._log("+srv  %-30s at %s by %s" % (service, service_api, caller_id))
        return [OK, "registered", 1]

    def unregisterService(self, caller_id, service, service_api):
        with self.reg.lock:
            existed = service in self.reg.services
            if existed:
                del self.reg.services[service]
        if existed:
            self._log("-srv  %-30s by %s" % (service, caller_id))
        return [OK, "", 1 if existed else 0]

    def lookupService(self, caller_id, service):
        with self.reg.lock:
            entry = self.reg.services.get(service)
        if not entry:
            return [FAILURE, "no provider", ""]
        return [OK, "", entry[1]]

    def lookupNode(self, caller_id, node_name):
        with self.reg.lock:
            api = self.reg.node_apis.get(node_name)
        if not api:
            return [FAILURE, "unknown node", ""]
        return [OK, "", api]

    def getPublishedTopics(self, caller_id, subgraph):
        with self.reg.lock:
            out = [[t, self.reg.topic_types.get(t, "*")]
                   for t in self.reg.publishers if self.reg.publishers[t]]
        return [OK, "", out]

    def getTopicTypes(self, caller_id):
        with self.reg.lock:
            out = [[t, ty] for t, ty in self.reg.topic_types.items()]
        return [OK, "", out]

    def getSystemState(self, caller_id):
        with self.reg.lock:
            pubs = [[t, list(m.keys())] for t, m in self.reg.publishers.items() if m]
            subs = [[t, list(m.keys())] for t, m in self.reg.subscribers.items() if m]
            srvs = [[s, [v[0]]] for s, v in self.reg.services.items()]
        return [OK, "", [pubs, subs, srvs]]

    # -- parameter API -----------------------------------------------------
    def setParam(self, caller_id, key, value):
        self.params.set(key, value)
        self._log("param set %s" % key)
        self._notify_param(key, value)
        return [OK, "", 0]

    def getParam(self, caller_id, key):
        try:
            return [OK, "", self.params.get(key)]
        except KeyError:
            return [ERROR, "Parameter [%s] is not set" % key, 0]

    def hasParam(self, caller_id, key):
        return [OK, key, self.params.has(key)]

    def deleteParam(self, caller_id, key):
        try:
            self.params.delete(key)
            return [OK, "", 0]
        except KeyError:
            return [ERROR, "Parameter [%s] is not set" % key, 0]

    def searchParam(self, caller_id, key):
        ns = caller_id.rsplit("/", 1)[0] or "/"
        found = self.params.search(ns, key)
        if found is None:
            return [ERROR, "Cannot find parameter [%s]" % key, ""]
        return [OK, "", found]

    def getParamNames(self, caller_id):
        return [OK, "", self.params.names()]

    def subscribeParam(self, caller_id, caller_api, key):
        with self.params.lock:
            self.params.subscriptions.setdefault(key, {})[caller_id] = caller_api
        try:
            return [OK, "", self.params.get(key)]
        except KeyError:
            return [OK, "", {}]

    def unsubscribeParam(self, caller_id, caller_api, key):
        with self.params.lock:
            existed = self.params.subscriptions.get(key, {}).pop(caller_id, None)
        return [OK, "", 1 if existed else 0]


# ---- XML-RPC plumbing -----------------------------------------------------
class _Handler(SimpleXMLRPCRequestHandler):
    rpc_paths = ("/", "/RPC2")
    # HTTP/1.1 keep-alive: clients (rospy/roscpp/irap_noroslib) reuse one connection for
    # many calls instead of opening a fresh socket each time -- far less
    # connection churn, so a registration storm can't overflow the listen queue.
    protocol_version = "HTTP/1.1"

    def log_message(self, *a):
        pass  # silence per-request HTTP logging


class _ThreadingServer(ThreadingMixIn, SimpleXMLRPCServer):
    daemon_threads = True
    allow_reuse_address = True
    request_queue_size = 128   # deep listen backlog for registration bursts (default 5)


def _make_run_id():
    # A stable-ish id without importing uuid's platform bits; good enough.
    import time
    return "%x-%x" % (os.getpid(), int(time.time()))


def _advertised_host():
    for var in ("ROS_HOSTNAME", "ROS_IP"):
        v = os.environ.get(var)
        if v:
            return v
    try:
        return socket.gethostname()
    except Exception:
        return "localhost"


def _port_from_env(default=11311):
    uri = os.environ.get("ROS_MASTER_URI", "")
    if "://" in uri:
        try:
            return int(uri.rsplit(":", 1)[1].strip("/"))
        except (ValueError, IndexError):
            pass
    return default


def serve(host=None, port=None, bind="0.0.0.0", quiet=False, rosout=True):
    """Start the master and block. Returns only on shutdown."""
    host = host or _advertised_host()
    port = port or _port_from_env()
    master_uri = "http://%s:%d/" % (host, port)

    server = _ThreadingServer((bind, port), requestHandler=_Handler,
                              allow_none=True, logRequests=False)
    master = RosMaster(master_uri, quiet=quiet)
    server.register_instance(master, allow_dotted_names=False)
    server.register_introspection_functions()

    print("[nr_roscore] ROS master online", flush=True)
    print("[nr_roscore] ROS_MASTER_URI=%s  (bind %s:%d)" % (master_uri, bind, port), flush=True)
    print("[nr_roscore] point nodes here:  export ROS_MASTER_URI=%s" % master_uri, flush=True)

    if rosout:
        # Auto-start the /rosout -> /rosout_agg aggregator, like a real roscore.
        try:
            from . import rosout as _rosout
            _rosout.start_in_background(master_uri="http://127.0.0.1:%d/" % port, host=host)
            print("[nr_roscore] started /rosout aggregator (-> /rosout_agg)", flush=True)
        except Exception as e:
            print("[nr_roscore] rosout aggregator unavailable: %s" % e, flush=True)

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n[nr_roscore] shutting down", flush=True)
    finally:
        server.shutdown()
        server.server_close()


def main(argv=None):
    import argparse
    ap = argparse.ArgumentParser(prog="nr_roscore", description="A standalone ROS master (no ROS installed).")
    ap.add_argument("-p", "--port", type=int, default=None,
                    help="port to bind/advertise (default: from $ROS_MASTER_URI or 11311)")
    ap.add_argument("--host", default=None,
                    help="hostname/IP to advertise (default: $ROS_HOSTNAME/$ROS_IP or system hostname)")
    ap.add_argument("--bind", default="0.0.0.0", help="interface to bind (default: 0.0.0.0)")
    ap.add_argument("-q", "--quiet", action="store_true", help="don't log registrations")
    ap.add_argument("--no-rosout", action="store_true", help="don't start the /rosout aggregator")
    args = ap.parse_args(argv)
    serve(host=args.host, port=args.port, bind=args.bind, quiet=args.quiet,
          rosout=not args.no_rosout)


if __name__ == "__main__":
    main()
