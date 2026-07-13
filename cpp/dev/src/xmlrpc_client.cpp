#include "irap_noroslib/xmlrpc_client.hpp"

#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <sstream>

#include "irap_noroslib/net_util.hpp"

namespace irap_noroslib {
namespace {

// Very small HTTP/1.0 POST. Returns the response body, or empty on error.
bool http_post(const std::string& host, uint16_t port, const std::string& body,
               std::string* resp_body, std::string* err) {
  int fd = tcp_connect(host, port);
  if (fd < 0) {
    if (err) *err = "connect failed to " + host + ":" + std::to_string(port);
    return false;
  }
  std::ostringstream req;
  req << "POST /RPC2 HTTP/1.0\r\n"
      << "User-Agent: irap_noroslib/0.1\r\n"
      << "Host: " << host << ":" << port << "\r\n"
      << "Content-Type: text/xml\r\n"
      << "Content-Length: " << body.size() << "\r\n"
      << "\r\n"
      << body;
  std::string reqs = req.str();
  bool ok = write_n(fd, reqs.data(), reqs.size());
  if (!ok) {
    net_close(fd);
    if (err) *err = "write failed";
    return false;
  }

  // Read the full response (HTTP/1.0 => server closes at end).
  std::string raw;
  char buf[4096];
  while (true) {
    ssize_t r = recv(fd, buf, sizeof(buf), 0);
    if (r <= 0) break;
    raw.append(buf, static_cast<size_t>(r));
  }
  net_close(fd);

  size_t hdr_end = raw.find("\r\n\r\n");
  if (hdr_end == std::string::npos) {
    if (err) *err = "malformed HTTP response";
    return false;
  }
  *resp_body = raw.substr(hdr_end + 4);
  return true;
}

}  // namespace

bool xmlrpc_call(const std::string& uri, const std::string& method,
                 const std::vector<XmlValue>& params, XmlValue* result, std::string* err) {
  std::string host;
  uint16_t port = 0;
  if (!parse_http_uri(uri, &host, &port)) {
    if (err) *err = "bad uri: " + uri;
    return false;
  }
  std::string body = build_method_call(method, params);
  std::string resp;
  if (!http_post(host, port, body, &resp, err)) return false;

  std::string fault;
  if (!parse_method_response(resp, result, &fault)) {
    if (err) *err = fault.empty() ? "parse error" : ("fault: " + fault);
    return false;
  }
  return true;
}

// ROS wrappers return [code, statusMessage, value]. Extract that shape.
static bool ros_call(const std::string& uri, const std::string& method,
                     const std::vector<XmlValue>& params, XmlValue* value, std::string* err) {
  XmlValue result;
  if (!xmlrpc_call(uri, method, params, &result, err)) return false;
  if (result.type != XmlValue::Type::Array || result.arr.size() < 3) {
    if (err) *err = method + ": unexpected response shape";
    return false;
  }
  int64_t code = result.arr[0].as_int();
  if (code != 1) {
    if (err) *err = method + " failed: " + result.arr[1].as_str();
    return false;
  }
  *value = result.arr[2];
  return true;
}

static void collect_uris(const XmlValue& v, std::vector<std::string>* out) {
  if (v.type != XmlValue::Type::Array) return;
  for (const auto& e : v.arr) out->push_back(e.as_str());
}

bool register_subscriber(const std::string& master_uri, const std::string& caller_id,
                         const std::string& topic, const std::string& type,
                         const std::string& caller_api,
                         std::vector<std::string>* publisher_uris, std::string* err) {
  XmlValue value;
  if (!ros_call(master_uri, "registerSubscriber",
                {XmlValue::Str(caller_id), XmlValue::Str(topic), XmlValue::Str(type),
                 XmlValue::Str(caller_api)},
                &value, err)) {
    return false;
  }
  publisher_uris->clear();
  collect_uris(value, publisher_uris);
  return true;
}

bool unregister_subscriber(const std::string& master_uri, const std::string& caller_id,
                           const std::string& topic, const std::string& caller_api,
                           std::string* err) {
  XmlValue value;
  return ros_call(master_uri, "unregisterSubscriber",
                  {XmlValue::Str(caller_id), XmlValue::Str(topic), XmlValue::Str(caller_api)},
                  &value, err);
}

bool register_publisher(const std::string& master_uri, const std::string& caller_id,
                        const std::string& topic, const std::string& type,
                        const std::string& caller_api,
                        std::vector<std::string>* subscriber_uris, std::string* err) {
  XmlValue value;
  if (!ros_call(master_uri, "registerPublisher",
                {XmlValue::Str(caller_id), XmlValue::Str(topic), XmlValue::Str(type),
                 XmlValue::Str(caller_api)},
                &value, err)) {
    return false;
  }
  if (subscriber_uris) {
    subscriber_uris->clear();
    collect_uris(value, subscriber_uris);
  }
  return true;
}

bool unregister_publisher(const std::string& master_uri, const std::string& caller_id,
                          const std::string& topic, const std::string& caller_api,
                          std::string* err) {
  XmlValue value;
  return ros_call(master_uri, "unregisterPublisher",
                  {XmlValue::Str(caller_id), XmlValue::Str(topic), XmlValue::Str(caller_api)},
                  &value, err);
}

bool get_param(const std::string& master_uri, const std::string& caller_id,
               const std::string& key, XmlValue* out, std::string* err) {
  return ros_call(master_uri, "getParam", {XmlValue::Str(caller_id), XmlValue::Str(key)},
                  out, err);
}

bool set_param(const std::string& master_uri, const std::string& caller_id,
               const std::string& key, const XmlValue& value, std::string* err) {
  XmlValue ignore;
  return ros_call(master_uri, "setParam",
                  {XmlValue::Str(caller_id), XmlValue::Str(key), value}, &ignore, err);
}

bool has_param(const std::string& master_uri, const std::string& caller_id,
               const std::string& key, bool* out, std::string* err) {
  XmlValue v;
  if (!ros_call(master_uri, "hasParam", {XmlValue::Str(caller_id), XmlValue::Str(key)}, &v, err))
    return false;
  *out = (v.type == XmlValue::Type::Bool) ? v.b : (v.as_int() != 0);
  return true;
}

bool delete_param(const std::string& master_uri, const std::string& caller_id,
                  const std::string& key, std::string* err) {
  XmlValue ignore;
  return ros_call(master_uri, "deleteParam",
                  {XmlValue::Str(caller_id), XmlValue::Str(key)}, &ignore, err);
}

bool register_service(const std::string& master_uri, const std::string& caller_id,
                      const std::string& service, const std::string& service_api,
                      const std::string& caller_api, std::string* err) {
  XmlValue value;
  return ros_call(master_uri, "registerService",
                  {XmlValue::Str(caller_id), XmlValue::Str(service),
                   XmlValue::Str(service_api), XmlValue::Str(caller_api)},
                  &value, err);
}

bool unregister_service(const std::string& master_uri, const std::string& caller_id,
                        const std::string& service, const std::string& service_api,
                        std::string* err) {
  XmlValue value;
  return ros_call(master_uri, "unregisterService",
                  {XmlValue::Str(caller_id), XmlValue::Str(service), XmlValue::Str(service_api)},
                  &value, err);
}

bool lookup_service(const std::string& master_uri, const std::string& caller_id,
                    const std::string& service, std::string* service_url, std::string* err) {
  XmlValue value;
  if (!ros_call(master_uri, "lookupService",
                {XmlValue::Str(caller_id), XmlValue::Str(service)}, &value, err))
    return false;
  *service_url = value.as_str();
  return true;
}

bool request_topic_tcpros(const std::string& publisher_uri, const std::string& caller_id,
                          const std::string& topic, std::string* host, uint16_t* port,
                          std::string* err) {
  // protocols = [ ['TCPROS'] ]
  XmlValue tcpros = XmlValue::Array({XmlValue::Str("TCPROS")});
  XmlValue protocols = XmlValue::Array({tcpros});
  XmlValue value;
  if (!ros_call(publisher_uri, "requestTopic",
                {XmlValue::Str(caller_id), XmlValue::Str(topic), protocols}, &value, err)) {
    return false;
  }
  // value = ['TCPROS', host, port]
  if (value.type != XmlValue::Type::Array || value.arr.size() < 3) {
    if (err) *err = "requestTopic: unexpected protocol params";
    return false;
  }
  *host = value.arr[1].as_str();
  *port = static_cast<uint16_t>(value.arr[2].as_int());
  return true;
}

bool request_topic_udpros(const std::string& publisher_uri, const std::string& caller_id,
                          const std::string& topic, const std::string& sub_header,
                          const std::string& recv_host, uint16_t recv_port, uint32_t max_datagram,
                          std::string* pub_host, uint16_t* pub_port, uint32_t* conn_id,
                          uint32_t* neg_max_datagram, std::string* pub_header, std::string* err) {
  // protocols = [ ['UDPROS', <sub header base64>, recv_host, recv_port, max_datagram] ]
  XmlValue udpros = XmlValue::Array({XmlValue::Str("UDPROS"), XmlValue::Base64Bytes(sub_header),
                                     XmlValue::Str(recv_host), XmlValue::Int(recv_port),
                                     XmlValue::Int(static_cast<int64_t>(max_datagram))});
  XmlValue protocols = XmlValue::Array({udpros});
  XmlValue value;
  if (!ros_call(publisher_uri, "requestTopic",
                {XmlValue::Str(caller_id), XmlValue::Str(topic), protocols}, &value, err)) {
    return false;
  }
  // value = ['UDPROS', pub_host, pub_port, conn_id, max_datagram, pub_header]
  if (value.type != XmlValue::Type::Array || value.arr.size() < 6) {
    if (err) *err = "requestTopic(UDPROS): unexpected protocol params";
    return false;
  }
  *pub_host = value.arr[1].as_str();
  *pub_port = static_cast<uint16_t>(value.arr[2].as_int());
  *conn_id = static_cast<uint32_t>(value.arr[3].as_int());
  *neg_max_datagram = static_cast<uint32_t>(value.arr[4].as_int());
  *pub_header = value.arr[5].s;  // Base64 -> raw header bytes
  return true;
}

}  // namespace irap_noroslib
