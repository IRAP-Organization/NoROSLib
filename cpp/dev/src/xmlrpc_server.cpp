#include "irap_noroslib/xmlrpc_server.hpp"

#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>

#include "irap_noroslib/net_util.hpp"
#include "irap_noroslib/xmlrpc.hpp"

namespace irap_noroslib {

SlaveServer::SlaveServer(std::string node_name, std::string master_uri)
    : node_name_(std::move(node_name)), master_uri_(std::move(master_uri)) {}

SlaveServer::~SlaveServer() { stop(); }

std::string SlaveServer::caller_api() const {
  std::string host = advertised_host_.empty() ? "127.0.0.1" : advertised_host_;
  return "http://" + host + ":" + std::to_string(port_) + "/";
}

bool SlaveServer::start(const std::string& bind_ip, uint16_t port) {
  listen_fd_ = tcp_listen(bind_ip, port, &port_);
  if (listen_fd_ < 0) return false;
  running_ = true;
  thread_ = std::thread([this] { run(); });
  return true;
}

void SlaveServer::stop() {
  if (!running_.exchange(false)) return;
  if (listen_fd_ >= 0) {
    shutdown(listen_fd_, SHUT_RDWR);
    net_close(listen_fd_);
    listen_fd_ = -1;
  }
  if (thread_.joinable()) thread_.join();
}

void SlaveServer::run() {
  while (running_) {
    int fd = accept(listen_fd_, nullptr, nullptr);
    if (fd < 0) {
      if (!running_) break;
      continue;
    }
    handle_connection(fd);  // short request/response; handle inline
    net_close(fd);
  }
}

// Reads an HTTP request, returns the XML body. Handles Content-Length.
static bool read_http_request(int fd, std::string* body) {
  std::string data;
  char buf[4096];
  size_t header_end = std::string::npos;
  size_t content_length = 0;
  bool have_len = false;

  while (true) {
    if (header_end == std::string::npos) {
      header_end = data.find("\r\n\r\n");
      if (header_end != std::string::npos) {
        // Parse Content-Length (case-insensitive).
        std::string headers = data.substr(0, header_end);
        std::string lower = headers;
        for (auto& c : lower) c = static_cast<char>(tolower(c));
        size_t p = lower.find("content-length:");
        if (p != std::string::npos) {
          content_length = static_cast<size_t>(std::atoll(headers.c_str() + p + 15));
          have_len = true;
        }
      }
    }
    if (header_end != std::string::npos) {
      size_t body_have = data.size() - (header_end + 4);
      if (!have_len || body_have >= content_length) break;
    }
    ssize_t r = recv(fd, buf, sizeof(buf), 0);
    if (r <= 0) break;
    data.append(buf, static_cast<size_t>(r));
  }
  if (header_end == std::string::npos) return false;
  *body = data.substr(header_end + 4, have_len ? content_length : std::string::npos);
  return true;
}

static void send_http_response(int fd, const std::string& xml) {
  std::ostringstream resp;
  resp << "HTTP/1.0 200 OK\r\n"
       << "Content-Type: text/xml\r\n"
       << "Content-Length: " << xml.size() << "\r\n"
       << "\r\n"
       << xml;
  std::string s = resp.str();
  write_n(fd, s.data(), s.size());
}

void SlaveServer::handle_connection(int fd) {
  std::string body;
  if (!read_http_request(fd, &body)) return;
  std::string method;
  std::vector<XmlValue> params;
  if (!parse_method_call(body, &method, &params)) {
    send_http_response(fd, build_method_response(
        XmlValue::Array({XmlValue::Int(0), XmlValue::Str("parse error"), XmlValue::Int(0)})));
    return;
  }
  std::string xml = dispatch(method, params);
  send_http_response(fd, xml);
}

// Standard ROS slave response is [code, statusMessage, value].
static std::string ok_response(const XmlValue& value, const std::string& msg = "") {
  return build_method_response(XmlValue::Array({XmlValue::Int(1), XmlValue::Str(msg), value}));
}

std::string SlaveServer::dispatch(const std::string& method, const std::vector<XmlValue>& params) {
  if (method == "publisherUpdate") {
    // (caller_id, topic, publishers[])
    std::string topic = params.size() > 1 ? params[1].as_str() : "";
    std::vector<std::string> pubs;
    if (params.size() > 2 && params[2].type == XmlValue::Type::Array) {
      for (const auto& e : params[2].arr) pubs.push_back(e.as_str());
    }
    if (on_publisher_update_) on_publisher_update_(topic, pubs);
    return ok_response(XmlValue::Int(0));
  }

  if (method == "requestTopic") {
    // (caller_id, topic, protocols[]) -> [1, "", <chosen protocol params>]
    std::string topic = params.size() > 1 ? params[1].as_str() : "";
    std::vector<XmlValue> protocols;
    if (params.size() > 2 && params[2].type == XmlValue::Type::Array) protocols = params[2].arr;
    if (on_request_topic_) {
      XmlValue chosen = on_request_topic_(topic, protocols);
      if (chosen.type == XmlValue::Type::Array && !chosen.arr.empty()) return ok_response(chosen);
    }
    return build_method_response(
        XmlValue::Array({XmlValue::Int(0), XmlValue::Str("no supported protocol"),
                         XmlValue::Array({})}));
  }

  if (method == "getPid") {
    return ok_response(XmlValue::Int(static_cast<int64_t>(getpid())));
  }

  if (method == "getBusInfo") {
    return ok_response(XmlValue::Array({}));
  }
  if (method == "getBusStats") {
    return ok_response(XmlValue::Array({XmlValue::Array({}), XmlValue::Array({}),
                                        XmlValue::Array({})}));
  }
  if (method == "getSubscriptions" || method == "getPublications") {
    return ok_response(XmlValue::Array({}));
  }
  if (method == "getMasterUri") {
    return ok_response(XmlValue::Str(master_uri_));
  }
  if (method == "getName") {
    return ok_response(XmlValue::Str(node_name_));
  }
  if (method == "shutdown") {
    return ok_response(XmlValue::Int(0));
  }
  if (method == "paramUpdate") {
    return ok_response(XmlValue::Int(0));
  }

  // Unknown method: reply politely so callers don't error.
  return ok_response(XmlValue::Int(0), "unhandled: " + method);
}

}  // namespace irap_noroslib
