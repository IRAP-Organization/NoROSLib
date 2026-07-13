"""Thin client for the ROS master API (standard XML-RPC).

Every master call returns [code, statusMessage, value]; code==1 is success.
Uses Python's stdlib xmlrpc.client -- no ROS libraries.
"""
import threading
import xmlrpc.client as xmlrpc


class MasterError(RuntimeError):
    pass


class Master:
    def __init__(self, master_uri, caller_id):
        self.uri = master_uri
        self.caller_id = caller_id
        self._proxy = xmlrpc.ServerProxy(master_uri, allow_none=True)
        # xmlrpc.client.ServerProxy keeps a single persistent HTTP connection
        # that is NOT thread-safe; a node's subscriber/service/param threads all
        # share this Master, so serialize calls to avoid interleaved requests.
        self._lock = threading.Lock()

    def _call(self, method, *args):
        with self._lock:
            code, status, value = getattr(self._proxy, method)(self.caller_id, *args)
        if code != 1:
            raise MasterError("%s failed: %s" % (method, status))
        return value

    def register_publisher(self, topic, type_name, caller_api):
        """Returns the current list of subscriber APIs for the topic."""
        return self._call("registerPublisher", topic, type_name, caller_api)

    def unregister_publisher(self, topic, caller_api):
        return self._call("unregisterPublisher", topic, caller_api)

    def register_subscriber(self, topic, type_name, caller_api):
        """Returns the current list of publisher APIs for the topic."""
        return self._call("registerSubscriber", topic, type_name, caller_api)

    def unregister_subscriber(self, topic, caller_api):
        return self._call("unregisterSubscriber", topic, caller_api)

    def register_service(self, service, service_api, caller_api):
        return self._call("registerService", service, service_api, caller_api)

    def unregister_service(self, service, service_api):
        return self._call("unregisterService", service, service_api)

    def lookup_service(self, service):
        """Return the rosrpc:// URI of the service provider, or raise."""
        return self._call("lookupService", service)

    def lookup_node(self, node_name):
        return self._call("lookupNode", node_name)

    # -- parameter server ---------------------------------------------------
    def get_param(self, key):
        return self._call("getParam", key)

    def set_param(self, key, value):
        return self._call("setParam", key, value)

    def has_param(self, key):
        return self._call("hasParam", key)

    def delete_param(self, key):
        return self._call("deleteParam", key)

    def search_param(self, key):
        return self._call("searchParam", key)

    def get_param_names(self):
        return self._call("getParamNames")

    def get_published_topics(self, subgraph=""):
        return self._call("getPublishedTopics", subgraph)

    def get_topic_types(self):
        return self._call("getTopicTypes")

    def get_system_state(self):
        """[publishers, subscribers, services], each [[name, [node, ...]], ...]."""
        return self._call("getSystemState")


def request_topic_tcpros(publisher_uri, caller_id, topic):
    """Call a publisher's slave API requestTopic; return (host, port)."""
    proxy = xmlrpc.ServerProxy(publisher_uri, allow_none=True)
    code, status, proto = proxy.requestTopic(caller_id, topic, [["TCPROS"]])
    if code != 1:
        raise MasterError("requestTopic to %s failed: %s" % (publisher_uri, status))
    if not proto or proto[0] != "TCPROS":
        raise MasterError("requestTopic: publisher did not offer TCPROS: %r" % (proto,))
    return proto[1], int(proto[2])


def request_topic_udpros(publisher_uri, caller_id, topic, sub_header_bytes,
                         recv_host, recv_port, max_datagram):
    """Negotiate UDPROS with a publisher. Returns a dict with pub_host, pub_port,
    conn_id, max_datagram, pub_header (bytes). Raises MasterError on rejection
    (whose text may name the real md5, enabling discovery)."""
    proxy = xmlrpc.ServerProxy(publisher_uri, allow_none=True)
    proto = ["UDPROS", xmlrpc.Binary(sub_header_bytes), recv_host, recv_port, max_datagram]
    code, status, res = proxy.requestTopic(caller_id, topic, [proto])
    if code != 1:
        raise MasterError(status)
    if not res or res[0] != "UDPROS":
        raise MasterError("publisher did not offer UDPROS: %r" % (res,))
    pub_header = res[5].data if hasattr(res[5], "data") else bytes(res[5])
    return {"pub_host": res[1], "pub_port": int(res[2]), "conn_id": int(res[3]),
            "max_datagram": int(res[4]), "pub_header": pub_header}
