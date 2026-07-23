"""The irap_noroslib node: registration with a real roscore, a slave XML-RPC server so
the master and peer nodes can reach us, and Publisher / Subscriber that speak
TCPROS to real ROS nodes -- with automatic md5 discovery.

Public API (rospy-flavoured):  init_node, Publisher, Subscriber, Rate, spin,
is_shutdown, signal_shutdown, loginfo/logwarn/logerr, get_node.
"""
import os
import socket
import sys
import threading
import time
from xmlrpc.server import SimpleXMLRPCServer
from socketserver import ThreadingMixIn

from . import _tcpros as tcpros
from . import _udpros as udpros
from ._master import (Master, MasterError, request_topic_tcpros,
                      request_topic_udpros)
from .message import registry, get_message_class, define_message_from_definition
from .srv import ServiceException

_OK = lambda value=0, msg="": [1, msg, value]  # noqa: E731


# --------------------------------------------------------------------------
# logging
# --------------------------------------------------------------------------
_LEVELS = {"debug": 0, "info": 1, "warn": 2, "error": 3, "none": 4}
_log_level = _LEVELS["info"]


def set_log_level(level):
    """Silence the library's own logging: "info" (default), "warn", "error", "none".

    Useful for a CLI, where only the program's real output should be visible.
    """
    global _log_level
    if level not in _LEVELS:
        raise ValueError("log level must be one of %s" % ", ".join(_LEVELS))
    _log_level = _LEVELS[level]


def _log(level, msg):
    if _LEVELS[level.lower()] < _log_level:
        return
    # stderr, not stdout: stdout is the program's data (think `nr_rostopic echo
    # /topic > out.txt` -- log lines must not end up in the file).
    print("[%s] [%.6f]: %s" % (level, time.time(), msg), file=sys.stderr, flush=True)


def loginfo(msg):
    _log("INFO", msg)


def logwarn(msg):
    _log("WARN", msg)


def logerr(msg):
    _log("ERROR", msg)


# --------------------------------------------------------------------------
# configuration (settable before init_node) + host / URI resolution
# --------------------------------------------------------------------------
# Library-level defaults. Set these before init_node() to configure the master
# and the address peers reach you at, without touching environment variables.
_config = {"master_uri": None, "hostname": None}


def set_master_uri(uri):
    """Set the ROS master URI irap_noroslib registers with (e.g. 'http://10.0.0.5:11311').
    Call before init_node. Overrides $ROS_MASTER_URI; an explicit init_node arg
    still wins over this."""
    _config["master_uri"] = uri


def set_hostname(host):
    """Set the hostname/IP irap_noroslib advertises to the master and peers (like
    $ROS_IP / $ROS_HOSTNAME). Call before init_node. Overrides those env vars;
    an explicit init_node arg still wins over this."""
    _config["hostname"] = host


def configure(master_uri=None, hostname=None):
    """Set master_uri and/or hostname in one call, before init_node."""
    if master_uri is not None:
        _config["master_uri"] = master_uri
    if hostname is not None:
        _config["hostname"] = hostname


def _resolve_master_uri(explicit):
    return (explicit or _config["master_uri"] or os.environ.get("ROS_MASTER_URI")
            or "http://127.0.0.1:11311/")


def _resolve_host(explicit, master_uri):
    # precedence: explicit arg > configure()/set_hostname > $ROS_IP > $ROS_HOSTNAME
    host = (explicit or _config["hostname"]
            or os.environ.get("ROS_IP") or os.environ.get("ROS_HOSTNAME"))
    if host:
        return host
    # otherwise auto-detect the outbound source address toward the master
    try:
        mhost = master_uri.split("//", 1)[1].split(":")[0].split("/")[0]
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect((mhost, 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except Exception:
        return "127.0.0.1"


# --------------------------------------------------------------------------
# threaded XML-RPC slave server
# --------------------------------------------------------------------------
class _ThreadingXMLRPCServer(ThreadingMixIn, SimpleXMLRPCServer):
    daemon_threads = True
    allow_reuse_address = True


class _SlaveServer:
    """Serves the ROS slave API that master + peers call on us."""

    def __init__(self, node):
        self.node = node
        self.server = _ThreadingXMLRPCServer(("0.0.0.0", 0), allow_none=True,
                                             logRequests=False)
        self.port = self.server.server_address[1]
        for m in ("getPid", "getBusStats", "getBusInfo", "getMasterUri",
                  "shutdown", "requestTopic", "publisherUpdate", "paramUpdate",
                  "getSubscriptions", "getPublications"):
            self.server.register_function(getattr(self, m), m)
        self._thread = threading.Thread(target=self.server.serve_forever,
                                         name="irap_noroslib-slave", daemon=True)

    def start(self):
        self._thread.start()

    def stop(self):
        self.server.shutdown()
        self.server.server_close()

    # -- slave API methods (all take caller_id first) ----------------------
    def getPid(self, caller_id):
        return _OK(os.getpid())

    def getBusStats(self, caller_id):
        return _OK([[], [], []])

    def getBusInfo(self, caller_id):
        return _OK([])

    def getMasterUri(self, caller_id):
        return _OK(self.node.master.uri)

    def shutdown(self, caller_id, msg=""):
        self.node.signal_shutdown("remote shutdown: " + msg)
        return _OK()

    def paramUpdate(self, caller_id, key, value):
        return _OK()

    def getSubscriptions(self, caller_id):
        return _OK([[t, s.type_name] for t, s in self.node.subscriptions.items()])

    def getPublications(self, caller_id):
        return _OK([[t, p.type_name] for t, p in self.node.publications.items()])

    def requestTopic(self, caller_id, topic, protocols):
        pub = self.node.publications.get(topic)
        if pub is None:
            return [0, "no publisher for [%s]" % topic, []]
        for proto in protocols:
            if not proto:
                continue
            if proto[0] == "UDPROS":
                resp = pub.handle_udpros_request(proto)
                if resp:
                    return _OK(resp)
            elif proto[0] == "TCPROS":
                host, port = pub.endpoint()
                return _OK(["TCPROS", host, port])
        return [0, "no supported protocol in %r" % (protocols,), []]

    def publisherUpdate(self, caller_id, topic, publishers):
        sub = self.node.subscriptions.get(topic)
        if sub is not None:
            sub.update_publishers(publishers)
        return _OK()


# --------------------------------------------------------------------------
# publication: TCPROS server + connected subscriber sinks
# --------------------------------------------------------------------------
class _Publication:
    def __init__(self, node, topic, msg_class, latch):
        self.node = node
        self.topic = topic
        self.msg_class = msg_class
        self.type_name = msg_class.data_type()
        self.md5 = msg_class.md5sum()
        self.definition = msg_class.message_definition()
        self.latch = latch
        self._last = None                 # latched message bytes
        self._sinks = []                  # list of connected TCPROS sockets
        self._lock = threading.Lock()
        self._srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self._srv.bind(("0.0.0.0", 0))
        self._srv.listen(16)
        self._port = self._srv.getsockname()[1]
        # UDP socket so we can also offer UDPROS (roscpp offers both).
        self._udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self._udp.bind(("0.0.0.0", 0))
        self._udp_port = self._udp.getsockname()[1]
        self._udp_subs = []               # [dst, conn_id, max_dgram, message_id]
        self._udp_conn_next = 1
        self._running = True
        threading.Thread(target=self._accept_loop,
                         name="irap_noroslib-pub-%s" % topic, daemon=True).start()

    def endpoint(self):
        return self.node.host, self._port

    def handle_udpros_request(self, proto):
        """requestTopic(['UDPROS', <sub header>, sub_host, sub_port, max_dgram]).
        Register the subscriber's UDP endpoint; return the UDPROS params list."""
        if len(proto) < 5:
            return None
        hdr_bytes = proto[1].data if hasattr(proto[1], "data") else bytes(proto[1])
        sub_host, sub_port, max_dgram = proto[2], int(proto[3]), int(proto[4])
        if max_dgram <= udpros.HEADER_SIZE:
            max_dgram = udpros.DEFAULT_MAX_DATAGRAM
        sub_hdr = tcpros.decode_header(hdr_bytes)
        want = sub_hdr.get("md5sum", "*")
        if want not in ("*", "", self.md5):
            return None
        try:
            dst = udpros.resolve_ipv4(sub_host, sub_port)
        except OSError:
            return None
        conn_id = self._udp_conn_next
        self._udp_conn_next += 1
        with self._lock:
            self._udp_subs.append([dst, conn_id, max_dgram, 0])
        pub_hdr = tcpros.encode_header_fields(tcpros.make_publisher_header(
            self.node.name, self.topic, self.md5, self.type_name, self.definition, self.latch))
        import xmlrpc.client as _xc
        return ["UDPROS", self.node.host, self._udp_port, conn_id, max_dgram,
                _xc.Binary(pub_hdr)]

    def _accept_loop(self):
        while self._running:
            try:
                conn, _ = self._srv.accept()
            except OSError:
                break
            threading.Thread(target=self._handshake, args=(conn,),
                             daemon=True).start()

    def _handshake(self, conn):
        try:
            tcpros.set_nodelay(conn)
            hdr = tcpros.read_header(conn)
            sub_md5 = hdr.get("md5sum", "*")
            if sub_md5 not in ("*", self.md5):
                # tell the peer our real type/md5 -- this is what lets a peer
                # DISCOVER our md5 from the error, symmetric to our own logic.
                err = ("Client [%s] wants topic %s to have datatype/md5sum "
                       "[%s/%s], but our version has [%s/%s]. Dropping connection."
                       % (hdr.get("callerid", "?"), self.topic,
                          hdr.get("type", "?"), sub_md5, self.type_name, self.md5))
                tcpros.write_header(conn, {"error": err})
                conn.close()
                return
            tcpros.write_header(conn, tcpros.make_publisher_header(
                self.node.name, self.topic, self.md5, self.type_name,
                self.definition, self.latch))
            with self._lock:
                self._sinks.append(conn)
                if self.latch and self._last is not None:
                    try:
                        conn.sendall(tcpros.frame_message(self._last))
                    except OSError:
                        pass
            # block reading to detect disconnect; subscribers send nothing
            while self._running:
                if not conn.recv(1024):
                    break
        except (OSError, ConnectionError):
            pass
        finally:
            with self._lock:
                if conn in self._sinks:
                    self._sinks.remove(conn)
            try:
                conn.close()
            except OSError:
                pass

    def publish(self, msg):
        body = msg.serialize() if hasattr(msg, "serialize") else bytes(msg)
        frame = tcpros.frame_message(body)
        with self._lock:
            if self.latch:
                self._last = body
            dead = []
            for s in self._sinks:                      # TCPROS
                try:
                    s.sendall(frame)
                except OSError:
                    dead.append(s)
            for s in dead:
                self._sinks.remove(s)
                try:
                    s.close()
                except OSError:
                    pass
            if self._udp_subs:                          # UDPROS
                stream = frame  # [4B len][body] is exactly the TCPROS frame
                for entry in self._udp_subs:
                    dst, conn_id, max_dgram, mid = entry
                    mid = (mid % 255) + 1               # nonzero, wraps 1..255
                    entry[3] = mid
                    udpros.send_stream(self._udp, dst, conn_id, mid, stream, max_dgram)

    def num_connections(self):
        with self._lock:
            return len(self._sinks)

    def close(self):
        self._running = False
        for sk in (self._srv, self._udp):
            try:
                sk.close()
            except OSError:
                pass
        with self._lock:
            for s in self._sinks:
                try:
                    s.close()
                except OSError:
                    pass
            self._sinks = []
            self._udp_subs = []


# --------------------------------------------------------------------------
# subscription: connect to each publisher over TCPROS, discover md5, deliver
# --------------------------------------------------------------------------
class _Subscription:
    def __init__(self, node, topic, type_name, md5, callback, msg_class,
                 transport="tcpros"):
        self.node = node
        self.topic = topic
        self.type_name = type_name          # may be "*" if unknown
        self.md5 = md5                       # may be "*" to discover
        self.callback = callback
        self.msg_class = msg_class           # may be None (raw/wildcard)
        self.transport = transport           # "tcpros" or "udpros"
        self.definition = ""                 # publisher's message_definition (for bags)
        # Optional low-level recording hook. When set, every message is also
        # delivered raw as (type_name, md5, definition, body_bytes) -- what a bag
        # needs: the exact wire bytes plus the connection header, for ANY topic.
        self.record_cb = None
        self._links = {}                     # publisher_uri -> thread
        self._lock = threading.Lock()
        self._running = True

    def update_publishers(self, publisher_uris):
        with self._lock:
            current = set(publisher_uris)
            for uri in current:
                if uri not in self._links:
                    t = threading.Thread(target=self._run_link, args=(uri,),
                                          name="irap_noroslib-sub-%s" % self.topic,
                                          daemon=True)
                    self._links[uri] = t
                    t.start()
            # links to publishers that vanished exit on their own via _running

    def _run_link(self, publisher_uri):
        connect = (self._connect_once_udpros if self.transport == "udpros"
                   else self._connect_once)
        backoff = 0.2
        while self._running and self.node.ok():
            try:
                connect(publisher_uri)
                backoff = 0.2
            except (OSError, ConnectionError, MasterError):
                if self._running:
                    time.sleep(backoff)
                    backoff = min(backoff * 2, 2.0)
            else:
                break  # publisher gone / clean exit
        with self._lock:
            self._links.pop(publisher_uri, None)

    def _connect_once_udpros(self, publisher_uri):
        """UDPROS: bind a UDP recv socket, negotiate via requestTopic(UDPROS)
        with md5 discovery, then reassemble DATA0/DATAN datagrams to bodies."""
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.bind(("0.0.0.0", 0))
        recv_port = sock.getsockname()[1]
        sock.settimeout(0.2)
        md5, type_name = self.md5, self.type_name
        conn_id = None
        try:
            for attempt in (1, 2):
                sub_hdr = tcpros.encode_header_fields(tcpros.make_subscriber_header(
                    self.node.name, self.topic, md5, type_name))
                try:
                    res = request_topic_udpros(publisher_uri, self.node.name, self.topic,
                                               sub_hdr, self.node.host, recv_port,
                                               udpros.DEFAULT_MAX_DATAGRAM)
                except MasterError as e:
                    real_type, real_md5 = tcpros.parse_type_md5_from_error(str(e))
                    if attempt == 1 and real_md5:
                        logwarn(">>> DISCOVERED real md5 for %s (UDPROS): %s (type %s) -- reconnecting"
                                % (self.topic, real_md5, real_type))
                        self.md5, self.type_name = real_md5, real_type
                        md5, type_name = real_md5, real_type
                        continue
                    raise
                conn_id = res["conn_id"]
                ph = tcpros.decode_header(res["pub_header"])
                if ph.get("md5sum", "*") != "*":
                    self.md5, self.type_name = ph["md5sum"], ph.get("type", type_name)
                    if self.msg_class is None:
                        self.msg_class = get_message_class(self.type_name)
                break
            if conn_id is None:
                return
            rx = udpros.Receiver()
            while self._running and self.node.ok():
                try:
                    data, _ = sock.recvfrom(65536)
                except socket.timeout:
                    continue
                h = udpros.decode_header(data)
                if h is None or h[0] != conn_id:
                    continue
                body = rx.feed(data)
                if body is not None and self.callback is not None:
                    try:
                        self.callback(self.msg_class.deserialize(body)
                                      if self.msg_class else body)
                    except Exception as e:  # noqa
                        logwarn("callback/deserialize error on %s: %s" % (self.topic, e))
        finally:
            sock.close()

    def _connect_once(self, publisher_uri):
        sock, msg_class = self._connect_and_handshake(publisher_uri)
        try:
            self._receive_loop(sock, msg_class)
        finally:
            try:
                sock.close()
            except OSError:
                pass

    def _connect_and_handshake(self, publisher_uri):
        """requestTopic -> connect -> TCPROS handshake. If the publisher rejects
        us with an md5 error, DISCOVER its real md5 from that error text, adopt
        it, and reconnect once. Returns (live socket, msg_class). This automatic
        recovery is the headline irap_noroslib feature: a wrong/unknown md5 self-heals."""
        md5, type_name = self.md5, self.type_name
        for attempt in (1, 2):
            host, port = request_topic_tcpros(publisher_uri, self.node.name, self.topic)
            sock = socket.create_connection((host, port), timeout=5.0)
            tcpros.set_nodelay(sock)
            sock.settimeout(None)
            tcpros.write_header(sock, tcpros.make_subscriber_header(
                self.node.name, self.topic, md5, type_name))
            resp = tcpros.read_header(sock)
            if "error" not in resp:
                real_type = resp.get("type", type_name)
                real_md5 = resp.get("md5sum", md5)
                if real_md5 != "*":
                    self.md5, self.type_name = real_md5, real_type
                # Always keep the publisher's full definition (bags need it, even
                # for built-in types whose class we already had).
                if resp.get("message_definition"):
                    self.definition = resp["message_definition"]
                cls = self.msg_class or get_message_class(real_type)
                if cls is None and resp.get("message_definition"):
                    # We have never seen this type -- but the publisher just handed
                    # us its full definition in the handshake, so build it on the
                    # spot. This is what lets us decode a topic with no .msg file.
                    try:
                        cls = define_message_from_definition(
                            real_type, resp["message_definition"])
                        self.definition = resp["message_definition"]
                    except Exception as e:  # noqa -- fall back to raw bytes
                        logwarn("could not build %s from its definition: %s"
                                % (real_type, e))
                return sock, cls
            # md5 mismatch: learn the publisher's real type+md5 from the error
            sock.close()
            real_type, real_md5 = tcpros.parse_type_md5_from_error(resp["error"])
            if not real_md5 or attempt == 2:
                raise ConnectionError("handshake rejected: %s" % resp["error"])
            logwarn(">>> DISCOVERED real md5 for %s: %s (type %s) -- reconnecting"
                    % (self.topic, real_md5, real_type))
            self.md5 = md5 = real_md5
            self.type_name = type_name = real_type
            self.msg_class = self.msg_class or get_message_class(real_type)
        raise ConnectionError("handshake failed")

    def _receive_loop(self, sock, msg_class):
        while self._running and self.node.ok():
            body = tcpros.read_message(sock)
            if body is None:
                return
            if self.record_cb is not None:
                try:
                    self.record_cb(self.type_name, self.md5, self.definition, body)
                except Exception as e:  # noqa -- recorder guard
                    logwarn("record error on %s: %s" % (self.topic, e))
            if self.callback is None:
                continue
            try:
                self.callback(msg_class.deserialize(body) if msg_class else body)
            except Exception as e:  # noqa -- user callback / decode guard
                logwarn("callback/deserialize error on %s: %s" % (self.topic, e))

    def close(self):
        self._running = False


# --------------------------------------------------------------------------
# service server: a rosrpc TCP endpoint that answers one service
# --------------------------------------------------------------------------
class _ServiceServer:
    def __init__(self, node, name, srv_class, handler):
        self.node = node
        self.name = name
        self.srv = srv_class
        self.md5 = srv_class.md5sum()
        self.handler = handler
        self._running = True
        self._srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self._srv.bind(("0.0.0.0", 0))
        self._srv.listen(16)
        self._port = self._srv.getsockname()[1]
        self.uri = "rosrpc://%s:%d" % (node.host, self._port)
        threading.Thread(target=self._accept_loop,
                         name="irap_noroslib-srv-%s" % name, daemon=True).start()

    def _accept_loop(self):
        while self._running:
            try:
                conn, _ = self._srv.accept()
            except OSError:
                break
            threading.Thread(target=self._serve, args=(conn,), daemon=True).start()

    def _serve(self, conn):
        try:
            tcpros.set_nodelay(conn)
            hdr = tcpros.read_header(conn)
            want = hdr.get("md5sum", "*")
            if want not in ("*", self.md5):
                tcpros.write_header(conn, {"error":
                    "service %s md5 mismatch: client [%s] wants [%s], server has [%s]"
                    % (self.name, hdr.get("callerid", "?"), want, self.md5)})
                return
            tcpros.write_header(conn, tcpros.make_service_server_header(
                self.node.name, self.md5, self.srv.data_type(),
                self.srv.Request.data_type(), self.srv.Response.data_type()))
            if hdr.get("probe") == "1":
                return
            persistent = hdr.get("persistent") in ("1", "true")
            while self._running:
                req_body = tcpros.read_message(conn)
                if req_body is None:
                    break
                try:
                    resp = self.handler(self.srv.Request.deserialize(req_body))
                    if resp is None:
                        raise ServiceException("service handler returned None")
                    conn.sendall(tcpros.frame_service_response(True, resp.serialize()))
                except Exception as e:  # noqa -- report handler failure to caller
                    logerr("service %s handler error: %s" % (self.name, e))
                    conn.sendall(tcpros.frame_service_response(
                        False, ("%s" % e).encode("utf-8")))
                if not persistent:
                    break
        except (OSError, ConnectionError):
            pass
        finally:
            try:
                conn.close()
            except OSError:
                pass

    def close(self):
        self._running = False
        try:
            self._srv.close()
        except OSError:
            pass


# --------------------------------------------------------------------------
# node singleton
# --------------------------------------------------------------------------
# How often the keeper thread pings the master to notice it going / coming back.
_MASTER_PING_PERIOD = 2.0


class Node:
    def __init__(self, name, master_uri=None, host=None):
        self.name = name if name.startswith("/") else "/" + name
        self.master = Master(_resolve_master_uri(master_uri), self.name)
        self.host = _resolve_host(host, self.master.uri)
        self.slave = _SlaveServer(self)
        self.slave.start()
        self.caller_api = "http://%s:%d/" % (self.host, self.slave.port)
        self.publications = {}
        self.subscriptions = {}
        self.services = {}
        self._shutdown = threading.Event()
        # Master-connection state. A node must survive the master going away and
        # rejoin when it returns -- just like a real roscpp/rospy node. We never
        # let a master call crash the user's node: registrations that fail are
        # remembered and retried, and everything is re-registered when the master
        # comes back (P2P TCPROS links themselves already survive independently).
        self._reg_lock = threading.RLock()
        self._master_up = True             # optimistic until proven otherwise
        self._warned_lost = False
        threading.Thread(target=self._master_keeper,
                         name="irap_noroslib-master-keeper", daemon=True).start()
        loginfo("node %s: master=%s slave=%s"
                % (self.name, self.master.uri, self.caller_api))

    def ok(self):
        return not self._shutdown.is_set()

    def signal_shutdown(self, reason=""):
        if self._shutdown.is_set():
            return
        loginfo("shutting down: %s" % reason)
        for p in list(self.publications.values()):
            try:
                self.master.unregister_publisher(p.topic, self.caller_api)
            except Exception:
                pass
            p.close()
        for s in list(self.subscriptions.values()):
            try:
                self.master.unregister_subscriber(s.topic, self.caller_api)
            except Exception:
                pass
            s.close()
        for sv in list(self.services.values()):
            try:
                self.master.unregister_service(sv.name, sv.uri)
            except Exception:
                pass
            sv.close()
        self._shutdown.set()
        try:
            self.slave.stop()
        except Exception:
            pass

    # -- registration ------------------------------------------------------
    # Every registration goes through the master, which may be down. We never
    # raise from these: the local pub/sub/service is created regardless, and the
    # keeper thread (re)registers with the master whenever it is reachable. This
    # is what lets a node start before roscore, or outlive a roscore restart.
    def advertise(self, topic, msg_class, latch=False):
        topic = _norm(topic)
        pub = _Publication(self, topic, msg_class, latch)
        with self._reg_lock:
            self.publications[topic] = pub
            self._register_publisher(pub)
        return pub

    def subscribe(self, topic, type_name, md5, callback, msg_class, transport="tcpros"):
        topic = _norm(topic)
        sub = _Subscription(self, topic, type_name, md5, callback, msg_class, transport)
        with self._reg_lock:
            self.subscriptions[topic] = sub
            self._register_subscriber(sub)
        return sub

    def advertise_service(self, name, srv_class, handler):
        name = _norm(name)
        srv = _ServiceServer(self, name, srv_class, handler)
        with self._reg_lock:
            self.services[name] = srv
            self._register_service(srv)
        return srv

    # -- resilient master calls (never raise; report reachability) ---------
    def _register_publisher(self, pub):
        try:
            self.master.register_publisher(pub.topic, pub.type_name, self.caller_api)
            self._master_recovered()
            return True
        except (OSError, ConnectionError, MasterError) as e:
            self._master_lost(e)
            return False

    def _register_subscriber(self, sub):
        try:
            pubs = self.master.register_subscriber(sub.topic, sub.type_name,
                                                   self.caller_api)
            sub.update_publishers(pubs)
            self._master_recovered()
            return True
        except (OSError, ConnectionError, MasterError) as e:
            self._master_lost(e)
            return False

    def _register_service(self, srv):
        try:
            self.master.register_service(srv.name, srv.uri, self.caller_api)
            self._master_recovered()
            return True
        except (OSError, ConnectionError, MasterError) as e:
            self._master_lost(e)
            return False

    def _master_lost(self, err):
        """A master call just failed. Warn once per outage, and arm the keeper."""
        with self._reg_lock:
            self._master_up = False
            if not self._warned_lost:
                self._warned_lost = True
                logwarn("lost the ROS master at %s (%s); the node keeps running and "
                        "will re-register when it returns" % (self.master.uri, err))

    def _master_recovered(self):
        """A master call just succeeded. Announce recovery if we had been down."""
        with self._reg_lock:
            self._master_up = True
            self._warned_lost = False

    def _reregister_all(self):
        """Re-announce every publisher, subscriber and service to the master.
        Master registration is idempotent, so this is safe to repeat; it is how a
        node rejoins a roscore that restarted (or started after the node did)."""
        ok = True
        with self._reg_lock:
            pubs = list(self.publications.values())
            subs = list(self.subscriptions.values())
            srvs = list(self.services.values())
        for p in pubs:
            ok = self._register_publisher(p) and ok
        for s in subs:
            ok = self._register_subscriber(s) and ok
        for sv in srvs:
            ok = self._register_service(sv) and ok
        return ok

    def _master_keeper(self):
        """Watch the master. While it is reachable, ping quietly; the moment it
        comes back after an outage (or an initial registration that failed while
        it was down), re-register everything so peers can find us again."""
        while not self._shutdown.is_set():
            self._shutdown.wait(_MASTER_PING_PERIOD)
            if self._shutdown.is_set():
                break
            with self._reg_lock:
                has_regs = bool(self.publications or self.subscriptions
                                or self.services)
                was_up = self._master_up
            try:
                self.master.get_pid()
            except (OSError, ConnectionError, MasterError) as e:
                self._master_lost(e)
                continue
            # Master answered. If it had been down, re-register everything so
            # peers can find us again, then announce the recovery.
            if not was_up:
                if has_regs and self._reregister_all():
                    loginfo("ROS master back online at %s -- re-registered %d "
                            "publisher(s), %d subscriber(s), %d service(s)"
                            % (self.master.uri, len(self.publications),
                               len(self.subscriptions), len(self.services)))
            self._master_recovered()


_NODE = None


def _norm(topic):
    return topic if topic.startswith("/") else "/" + topic


def init_node(name, master_uri=None, host=None):
    """Register this process as a ROS node with the master. Call once.

    master_uri: the ROS master to register with. Precedence:
        this arg > irap_noroslib.set_master_uri() > $ROS_MASTER_URI > http://127.0.0.1:11311.
    host: the address peers reach you at (advertised to master + nodes). Precedence:
        this arg > irap_noroslib.set_hostname() > $ROS_IP > $ROS_HOSTNAME > auto-detected.
    """
    global _NODE
    if _NODE is None:
        _NODE = Node(name, master_uri, host)
    return _NODE


def get_node():
    if _NODE is None:
        raise RuntimeError("call irap_noroslib.init_node(...) first")
    return _NODE


def is_shutdown():
    return _NODE is None or not _NODE.ok()


def signal_shutdown(reason=""):
    if _NODE is not None:
        _NODE.signal_shutdown(reason)


# --------------------------------------------------------------------------
# Publisher / Subscriber / Rate / spin  (rospy-style front end)
# --------------------------------------------------------------------------
class Publisher:
    """Publish messages on a topic to real ROS subscribers.

        pub = irap_noroslib.Publisher("/chatter", msg.String)
        pub.publish(msg.String(data="hello"))
    """

    def __init__(self, topic, msg_class, latch=False, queue_size=None):
        self._pub = get_node().advertise(topic, msg_class, latch)
        self.msg_class = msg_class

    def publish(self, message):
        self._pub.publish(message)

    def get_num_connections(self):
        return self._pub.num_connections()

    def unregister(self):
        node = get_node()
        try:
            node.master.unregister_publisher(self._pub.topic, node.caller_api)
        except Exception:
            pass
        self._pub.close()
        node.publications.pop(self._pub.topic, None)


class Subscriber:
    """Subscribe to a topic. `data_class` may be a message class, or None to
    receive raw bytes and auto-discover the type/md5 from the publisher.

        irap_noroslib.Subscriber("/chatter", msg.String, cb)
        irap_noroslib.Subscriber("/anything", None, cb)              # wildcard: raw bytes
        irap_noroslib.Subscriber("/chatter", msg.String, cb, transport="udpros")  # UDP
    """

    def __init__(self, topic, data_class, callback, queue_size=None, transport="tcpros"):
        if data_class is None:
            type_name, md5, cls = "*", "*", None
        else:
            type_name, md5, cls = (data_class.data_type(),
                                   data_class.md5sum(), data_class)
        self._sub = get_node().subscribe(topic, type_name, md5, callback, cls, transport)

    def unregister(self):
        node = get_node()
        try:
            node.master.unregister_subscriber(self._sub.topic, node.caller_api)
        except Exception:
            pass
        self._sub.close()
        node.subscriptions.pop(self._sub.topic, None)


def subscribe_raw(topic, callback):
    """Subscribe and receive each message raw, as
    (type_name, md5, message_definition, body_bytes).

    The low-level hook nr_rosbag records from: it discovers the type from the
    publisher, so it captures the exact wire bytes -- and the connection header
    needed to replay them -- for ANY topic, with no .msg file. Returns a
    Subscriber; call .unregister() to stop."""
    sub = Subscriber(topic, None, None)
    sub._sub.record_cb = callback
    return sub


class Service:
    """Advertise a service and answer calls, like rospy.Service.

        def handle(req):
            return AddTwoInts.Response(sum=req.a + req.b)
        irap_noroslib.Service("/add_two_ints", AddTwoInts, handle)

    `handler` receives a srv_class.Request and returns a srv_class.Response
    (raising to signal failure to the caller).
    """

    def __init__(self, name, srv_class, handler):
        self._srv = get_node().advertise_service(name, srv_class, handler)

    def shutdown(self):
        node = get_node()
        try:
            node.master.unregister_service(self._srv.name, self._srv.uri)
        except Exception:
            pass
        self._srv.close()
        node.services.pop(self._srv.name, None)


class ServiceProxy:
    """Call a service, like rospy.ServiceProxy.

        add = irap_noroslib.ServiceProxy("/add_two_ints", AddTwoInts)
        resp = add(AddTwoInts.Request(a=3, b=4))   # or add.call(req)
        print(resp.sum)
    """

    def __init__(self, name, srv_class):
        self.name = _norm(name)
        self.srv = srv_class
        self.md5 = srv_class.md5sum()

    def __call__(self, request):
        return self.call(request)

    def call(self, request):
        node = get_node()
        url = node.master.lookup_service(self.name)     # rosrpc://host:port
        host, port = _parse_rosrpc(url)
        sock = socket.create_connection((host, port), timeout=5.0)
        tcpros.set_nodelay(sock)
        try:
            tcpros.write_header(sock, tcpros.make_service_client_header(
                node.name, self.name, self.md5))
            resp_hdr = tcpros.read_header(sock)
            if "error" in resp_hdr:
                raise ServiceException("service %s rejected call: %s"
                                       % (self.name, resp_hdr["error"]))
            sock.sendall(tcpros.frame_message(request.serialize()))
            ok, body = tcpros.read_service_response(sock)
            if not ok:
                raise ServiceException("service %s failed: %s"
                                       % (self.name, body.decode("utf-8", "replace")))
            return self.srv.Response.deserialize(body)
        finally:
            try:
                sock.close()
            except OSError:
                pass


def _parse_rosrpc(url):
    # "rosrpc://host:port"
    rest = url.split("://", 1)[1].rstrip("/")
    host, _, port = rest.rpartition(":")
    return host, int(port)


def probe_service(name):
    """Ask a running service what it is, without calling it.

    Connects, sends a probe handshake (md5sum=* so it never mismatches) and
    reads the reply header the server always sends back. Returns a dict with
    'type', 'md5sum', 'request_type', 'response_type' -- exactly what
    `rosservice type` / `info` need, and which the master does NOT record.
    Raises if the service is unknown or unreachable.
    """
    node = get_node()
    name = _norm(name)
    url = node.master.lookup_service(name)              # rosrpc://host:port
    host, port = _parse_rosrpc(url)
    sock = socket.create_connection((host, port), timeout=5.0)
    tcpros.set_nodelay(sock)
    try:
        hdr = tcpros.make_service_client_header(node.name, name, "*")
        hdr["probe"] = "1"
        tcpros.write_header(sock, hdr)
        reply = tcpros.read_header(sock)
        if "error" in reply:
            raise ServiceException("service %s rejected probe: %s"
                                   % (name, reply["error"]))
        return reply
    finally:
        try:
            sock.close()
        except OSError:
            pass


def wait_for_service(name, timeout=None):
    """Block until a service is registered with the master (or timeout).

    Returns True if available, False on timeout."""
    node = get_node()
    name = _norm(name)
    deadline = None if timeout is None else time.monotonic() + timeout
    while node.ok():
        try:
            node.master.lookup_service(name)
            return True
        except MasterError:
            pass
        if deadline is not None and time.monotonic() >= deadline:
            return False
        time.sleep(0.1)
    return False


class Rate:
    """Sleep to maintain a fixed loop frequency, like rospy.Rate."""

    def __init__(self, hz):
        self._period = 1.0 / hz if hz > 0 else 0.0
        self._next = time.monotonic() + self._period

    def sleep(self):
        now = time.monotonic()
        if now < self._next:
            time.sleep(self._next - now)
            self._next += self._period
        else:
            self._next = now + self._period


def spin():
    """Block until shutdown (Ctrl-C), while background threads do the work."""
    node = get_node()
    try:
        while node.ok():
            time.sleep(0.1)
    except KeyboardInterrupt:
        node.signal_shutdown("KeyboardInterrupt")


def sleep(seconds):
    time.sleep(seconds)


def now():
    """Current time as a ROS (secs, nsecs) tuple, for stamping a Header."""
    t = time.time()
    secs = int(t)
    return secs, int(round((t - secs) * 1e9))


# --------------------------------------------------------------------------
# parameter server (rospy-style)
# --------------------------------------------------------------------------
_PARAM_UNSET = object()


def get_param(key, default=_PARAM_UNSET):
    """Get a parameter from the master. Returns `default` if given and the
    parameter is missing; otherwise raises KeyError."""
    try:
        return get_node().master.get_param(key)
    except MasterError:
        if default is not _PARAM_UNSET:
            return default
        raise KeyError("parameter %r is not set" % key)


def set_param(key, value):
    """Set a parameter on the master (int/float/str/bool/list/dict)."""
    get_node().master.set_param(key, value)


def has_param(key):
    return bool(get_node().master.has_param(key))


def delete_param(key):
    try:
        get_node().master.delete_param(key)
    except MasterError:
        raise KeyError("parameter %r is not set" % key)


def search_param(key):
    """Search the caller's namespaces upward for `key`; returns the full name
    or None if not found."""
    try:
        return get_node().master.search_param(key)
    except MasterError:
        return None


def get_param_names():
    return list(get_node().master.get_param_names())
