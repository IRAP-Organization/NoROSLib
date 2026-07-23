// node.cpp — the irap_noroslib node core: master registration, a slave XML-RPC server so
// the master + peers can reach us, TCPROS Publication (server) and Subscription
// (client with md5 discovery), and the process-wide singleton behind the
// roscpp-style front end in node.hpp.
#include "irap_noroslib/node.hpp"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <ctime>
#include <map>
#include <mutex>

#include "irap_noroslib/net_util.hpp"
#include "irap_noroslib/tcpros.hpp"
#include "irap_noroslib/udpros.hpp"
#include "irap_noroslib/xmlrpc.hpp"
#include "irap_noroslib/xmlrpc_client.hpp"
#include "irap_noroslib/xmlrpc_server.hpp"

namespace irap_noroslib {

// --------------------------------------------------------------------------
// logging + shutdown flag
// --------------------------------------------------------------------------
namespace {
std::atomic<bool> g_shutdown{false};
std::atomic<int> g_log_level{1};        // 0=debug 1=info 2=warn 3=error 4=none

void log(int level, const char* name, const std::string& msg) {
  if (level < g_log_level.load()) return;
  int64_t sec = 0, nsec = 0; irap_noroslib::wall_time(&sec, &nsec);
  // stderr, not stdout: stdout is the program's data (think `nr_rostopic echo
  // /topic > out.txt` -- log lines must not land in the file).
  std::fprintf(stderr, "[%s] [%ld.%06ld]: %s\n", name, (long)sec, (long)(nsec / 1000),
               msg.c_str());
  std::fflush(stderr);
}
void on_sigint(int) { g_shutdown.store(true); }
}  // namespace

void set_log_level(const std::string& level) {
  if (level == "debug") g_log_level.store(0);
  else if (level == "info") g_log_level.store(1);
  else if (level == "warn") g_log_level.store(2);
  else if (level == "error") g_log_level.store(3);
  else if (level == "none") g_log_level.store(4);
  else throw std::runtime_error("irap_noroslib: log level must be debug/info/warn/error/none");
}

void loginfo(const std::string& m) { log(1, "INFO", m); }
void logwarn(const std::string& m) { log(2, "WARN", m); }
void logerr(const std::string& m) { log(3, "ERROR", m); }

// --------------------------------------------------------------------------
// framed TCPROS message helpers: [4B LE length][body]
// --------------------------------------------------------------------------
namespace {
bool send_framed(int fd, const std::vector<uint8_t>& body) {
  uint8_t len[4];
  uint32_t n = (uint32_t)body.size();
  len[0] = n; len[1] = n >> 8; len[2] = n >> 16; len[3] = n >> 24;
  return write_n(fd, len, 4) && (body.empty() || write_n(fd, body.data(), body.size()));
}

// Read one framed message body. Returns false on EOF/error.
bool recv_framed(int fd, std::vector<uint8_t>* out) {
  uint8_t len[4];
  if (!read_n(fd, len, 4)) return false;
  uint32_t n = (uint32_t)len[0] | ((uint32_t)len[1] << 8) | ((uint32_t)len[2] << 16) |
               ((uint32_t)len[3] << 24);
  out->resize(n);
  return n == 0 || read_n(fd, out->data(), n);
}

// Service reply on the wire: [1B ok][4B LE length][body].
bool send_service_response(int fd, bool ok, const std::vector<uint8_t>& body) {
  uint8_t hdr[5];
  hdr[0] = ok ? 1 : 0;
  uint32_t n = (uint32_t)body.size();
  hdr[1] = n; hdr[2] = n >> 8; hdr[3] = n >> 16; hdr[4] = n >> 24;
  return write_n(fd, hdr, 5) && (body.empty() || write_n(fd, body.data(), body.size()));
}

bool recv_service_response(int fd, bool* ok, std::vector<uint8_t>* body) {
  uint8_t hdr[5];
  if (!read_n(fd, hdr, 5)) return false;
  *ok = hdr[0] != 0;
  uint32_t n = (uint32_t)hdr[1] | ((uint32_t)hdr[2] << 8) | ((uint32_t)hdr[3] << 16) |
               ((uint32_t)hdr[4] << 24);
  body->resize(n);
  return n == 0 || read_n(fd, body->data(), n);
}

// Resolve host (numeric IP or hostname) + port into a sockaddr_in.
bool resolve_ipv4(const std::string& host, uint16_t port, sockaddr_in* out) {
  std::memset(out, 0, sizeof(*out));
  out->sin_family = AF_INET;
  out->sin_port = htons(port);
  if (inet_pton(AF_INET, host.c_str(), &out->sin_addr) == 1) return true;
  addrinfo hints{}; hints.ai_family = AF_INET; hints.ai_socktype = SOCK_DGRAM;
  addrinfo* res = nullptr;
  if (getaddrinfo(host.c_str(), nullptr, &hints, &res) != 0 || !res) return false;
  out->sin_addr = reinterpret_cast<sockaddr_in*>(res->ai_addr)->sin_addr;
  freeaddrinfo(res);
  return true;
}

// Parse "rosrpc://host:port" -> host, port. Returns false on malformed input.
bool parse_rosrpc(const std::string& url, std::string* host, uint16_t* port) {
  auto pos = url.find("://");
  if (pos == std::string::npos) return false;
  std::string rest = url.substr(pos + 3);
  if (!rest.empty() && rest.back() == '/') rest.pop_back();
  auto colon = rest.rfind(':');
  if (colon == std::string::npos) return false;
  *host = rest.substr(0, colon);
  *port = (uint16_t)std::atoi(rest.c_str() + colon + 1);
  return true;
}
}  // namespace

namespace detail {

// --------------------------------------------------------------------------
// Publication: a TCPROS server that serves one topic to connected subscribers.
// --------------------------------------------------------------------------
class Publication {
 public:
  Publication(std::string topic, std::string type, std::string md5, std::string def, bool latch,
              std::string host)
      : topic_(std::move(topic)), type_(std::move(type)), md5_(std::move(md5)),
        def_(std::move(def)), latch_(latch), host_(std::move(host)) {}

  // stop() owns accept_thread_; without this, dropping the last shared_ptr to a
  // Publication (i.e. letting a Publisher go out of scope) would destroy a
  // joinable std::thread and abort the process. stop() is idempotent.
  ~Publication() { stop(); }

  bool start() {
    listen_fd_ = tcp_listen("0.0.0.0", 0, &port_);
    if (listen_fd_ < 0) return false;
    // Also open a UDP socket so we can offer UDPROS (roscpp offers both).
    udp_fd_ = udp_socket("0.0.0.0", 0, &udp_port_);
    running_.store(true);
    accept_thread_ = std::thread([this] { accept_loop(); });
    return true;
  }

  void stop() {
    if (!running_.exchange(false)) return;
    if (listen_fd_ >= 0) { ::shutdown(listen_fd_, SHUT_RDWR); irap_noroslib::net_close(listen_fd_); listen_fd_ = -1; }
    if (udp_fd_ >= 0) { irap_noroslib::net_close(udp_fd_); udp_fd_ = -1; }
    if (accept_thread_.joinable()) accept_thread_.join();
    std::lock_guard<std::mutex> lk(mu_);
    for (int fd : clients_) irap_noroslib::net_close(fd);
    clients_.clear();
  }

  const std::string& topic() const { return topic_; }
  const std::string& type() const { return type_; }
  std::string host() const { return host_; }
  uint16_t port() const { return port_; }

  // requestTopic(['UDPROS', <sub header>, sub_host, sub_port, max_dgram]) handler:
  // register the subscriber's UDP endpoint and return our UDPROS params array.
  XmlValue handle_udpros_request(const XmlValue& req) {
    if (udp_fd_ < 0 || req.arr.size() < 5) return XmlValue::Array({});
    const std::string& hdr_bytes = req.arr[1].s;   // Base64 -> raw header bytes
    std::string sub_host = req.arr[2].as_str();
    uint16_t sub_port = static_cast<uint16_t>(req.arr[3].as_int());
    uint32_t max_dgram = static_cast<uint32_t>(req.arr[4].as_int());
    if (max_dgram <= kUdprosHeaderSize) max_dgram = kUdprosDefaultMaxDatagram;

    TcprosHeader sub_hdr;
    decode_tcpros_header_fields(reinterpret_cast<const uint8_t*>(hdr_bytes.data()),
                               hdr_bytes.size(), &sub_hdr);
    std::string want = sub_hdr.count("md5sum") ? sub_hdr["md5sum"] : "*";
    if (want != "*" && !want.empty() && want != md5_) return XmlValue::Array({});

    UdprosSub s{};
    if (!resolve_ipv4(sub_host, sub_port, &s.dst)) return XmlValue::Array({});
    s.conn_id = udp_conn_next_++;
    s.max_dgram = max_dgram;
    {
      std::lock_guard<std::mutex> lk(mu_);
      udp_subs_.push_back(s);
    }
    std::vector<uint8_t> pub_hdr = encode_tcpros_header_fields(
        make_publisher_header(node_name_, topic_, md5_, type_, def_));
    return XmlValue::Array({XmlValue::Str("UDPROS"), XmlValue::Str(host_),
                            XmlValue::Int(udp_port_), XmlValue::Int(s.conn_id),
                            XmlValue::Int(static_cast<int64_t>(max_dgram)),
                            XmlValue::Base64Bytes(std::string(pub_hdr.begin(), pub_hdr.end()))});
  }

  void publish(const std::vector<uint8_t>& body) {
    std::lock_guard<std::mutex> lk(mu_);
    if (latch_) last_ = body;
    for (auto it = clients_.begin(); it != clients_.end();) {   // TCPROS
      if (send_framed(*it, body)) { ++it; }
      else { irap_noroslib::net_close(*it); it = clients_.erase(it); }
    }
    if (!udp_subs_.empty() && udp_fd_ >= 0) {                    // UDPROS
      // UDPROS carries the message as [4B LE len][body].
      std::vector<uint8_t> stream(4 + body.size());
      uint32_t n = (uint32_t)body.size();
      stream[0] = n; stream[1] = n >> 8; stream[2] = n >> 16; stream[3] = n >> 24;
      std::memcpy(stream.data() + 4, body.data(), body.size());
      for (auto& s : udp_subs_) {
        s.message_id = static_cast<uint8_t>(s.message_id + 1);
        if (s.message_id == 0) s.message_id = 1;   // message_id must be nonzero
        udpros_send_stream(udp_fd_, reinterpret_cast<sockaddr*>(&s.dst), sizeof(s.dst),
                           s.conn_id, s.message_id, stream.data(), (uint32_t)stream.size(),
                           s.max_dgram);
      }
    }
  }

  int num_connections() {
    std::lock_guard<std::mutex> lk(mu_);
    return (int)clients_.size();
  }

 private:
  void accept_loop() {
    while (running_.load()) {
      sockaddr_storage ss; socklen_t sl = sizeof(ss);
      int fd = ::accept(listen_fd_, (sockaddr*)&ss, &sl);
      if (fd < 0) { if (running_.load()) continue; else break; }
      set_tcp_nodelay(fd);
      std::thread([this, fd] { serve(fd); }).detach();
    }
  }

  // Read the subscriber's connection header, validate md5, reply, then hold the
  // connection open (blocking on recv to detect disconnect) while publish()
  // pushes framed messages.
  void serve(int fd) {
    TcprosHeader sub;
    if (!read_tcpros_header(fd, &sub)) { irap_noroslib::net_close(fd); return; }
    std::string want = sub.count("md5sum") ? sub.at("md5sum") : "*";
    if (want != "*" && want != md5_) {
      // Reply with the classic error naming OUR real type/md5, so the peer can
      // discover it — symmetric to our own subscriber-side discovery.
      TcprosHeader err;
      err["error"] = "Client [" + (sub.count("callerid") ? sub.at("callerid") : std::string("?")) +
                     "] wants topic " + topic_ + " to have datatype/md5sum [" +
                     (sub.count("type") ? sub.at("type") : std::string("?")) + "/" + want +
                     "], but our version has [" + type_ + "/" + md5_ + "]. Dropping connection.";
      write_tcpros_header(fd, err);
      irap_noroslib::net_close(fd);
      return;
    }
    TcprosHeader resp = make_publisher_header(node_name_, topic_, md5_, type_, def_);
    resp["latching"] = latch_ ? "1" : "0";
    if (!write_tcpros_header(fd, resp)) { irap_noroslib::net_close(fd); return; }
    {
      std::lock_guard<std::mutex> lk(mu_);
      clients_.push_back(fd);
      if (latch_ && !last_.empty()) send_framed(fd, last_);
    }
    // block until the peer closes
    uint8_t scratch[256];
    while (running_.load()) {
      ssize_t r = irap_noroslib::net_recv(fd, scratch, sizeof(scratch), 0);
      if (r <= 0) break;
    }
    std::lock_guard<std::mutex> lk(mu_);
    for (auto it = clients_.begin(); it != clients_.end(); ++it)
      if (*it == fd) { clients_.erase(it); break; }
    irap_noroslib::net_close(fd);
  }

 public:
  std::string node_name_;

 private:
  struct UdprosSub { sockaddr_in dst; uint32_t conn_id; uint32_t max_dgram; uint8_t message_id; };

  std::string topic_, type_, md5_, def_, host_;
  bool latch_;
  int listen_fd_ = -1;
  uint16_t port_ = 0;
  int udp_fd_ = -1;
  uint16_t udp_port_ = 0;
  uint32_t udp_conn_next_ = 1;
  std::atomic<bool> running_{false};
  std::thread accept_thread_;
  std::mutex mu_;
  std::vector<int> clients_;
  std::vector<UdprosSub> udp_subs_;
  std::vector<uint8_t> last_;
};

// --------------------------------------------------------------------------
// Subscription: connects to each publisher over TCPROS, discovers md5, delivers.
// --------------------------------------------------------------------------
class Subscription {
 public:
  Subscription(std::string topic, std::string type, std::string md5, RawCallback cb,
               std::string node_name, std::string host, std::string transport)
      : topic_(std::move(topic)), type_(std::move(type)), md5_(std::move(md5)),
        cb_(std::move(cb)), node_name_(std::move(node_name)), host_(std::move(host)),
        transport_(std::move(transport)) {}

  const std::string& topic() const { return topic_; }
  const std::string& type() const { return type_; }
  const std::string& md5() const { return md5_; }
  const std::string& definition() const { return definition_; }

  /// Called once per connection with the publisher's type / md5 / definition.
  using OnConnected = std::function<void(const std::string&, const std::string&,
                                         const std::string&)>;
  void set_on_connected(OnConnected f) { on_connected_ = std::move(f); }
  void set_callback(RawCallback cb) { cb_ = std::move(cb); }

  void update_publishers(const std::vector<std::string>& uris) {
    std::lock_guard<std::mutex> lk(mu_);
    for (const auto& uri : uris) {
      if (links_.find(uri) == links_.end()) {
        links_[uri] = true;
        std::thread([this, uri] { run_link(uri); }).detach();
      }
    }
  }

  void stop() {
    running_.store(false);
    // detached link threads observe running_ and exit
  }

 private:
  void run_link(std::string publisher_uri) {
    int backoff_ms = 200;
    while (running_.load() && !g_shutdown.load()) {
      bool ok = (transport_ == "udpros") ? connect_once_udpros(publisher_uri)
                                         : connect_once(publisher_uri);
      if (ok) { backoff_ms = 200; }
      else {
        std::this_thread::sleep_for(std::chrono::milliseconds(backoff_ms));
        backoff_ms = std::min(backoff_ms * 2, 2000);
      }
    }
    std::lock_guard<std::mutex> lk(mu_);
    links_.erase(publisher_uri);
  }

  // UDPROS subscribe: bind a UDP recv socket, negotiate via requestTopic(UDPROS)
  // (with md5 discovery), then reassemble DATA0/DATAN datagrams into bodies.
  bool connect_once_udpros(const std::string& publisher_uri) {
    uint16_t recv_port = 0;
    int rfd = udp_socket("0.0.0.0", 0, &recv_port);
    if (rfd < 0) return false;
    irap_noroslib::net_set_rcvtimeo_ms(rfd, 200);  // 200ms so the recv loop can observe shutdown
    std::string md5 = md5_, type = type_;
    uint32_t conn_id = 0;
    bool negotiated = false;
    for (int attempt = 0; attempt < 2 && !negotiated; ++attempt) {
      std::vector<uint8_t> hdr = encode_tcpros_header_fields(
          make_subscriber_header(node_name_, topic_, md5, type));
      std::string pub_host, pub_header, err;
      uint16_t pub_port = 0;
      uint32_t neg_max = 0;
      if (request_topic_udpros(publisher_uri, node_name_, topic_,
                               std::string(hdr.begin(), hdr.end()), host_, recv_port,
                               kUdprosDefaultMaxDatagram, &pub_host, &pub_port, &conn_id,
                               &neg_max, &pub_header, &err)) {
        // adopt the publisher's authoritative md5/type from its header blob
        TcprosHeader ph;
        decode_tcpros_header_fields(reinterpret_cast<const uint8_t*>(pub_header.data()),
                                   pub_header.size(), &ph);
        if (ph.count("md5sum") && ph["md5sum"] != "*") {
          md5_ = ph["md5sum"];
          type_ = ph.count("type") ? ph["type"] : type_;
        }
        negotiated = true;
        break;
      }
      // rejected — the error may name the real md5 (same discovery as TCPROS)
      std::string real_type, real_md5;
      if (attempt == 0 && parse_type_md5_from_error(err, &real_type, &real_md5) &&
          !real_md5.empty()) {
        logwarn(">>> DISCOVERED real md5 for " + topic_ + " (UDPROS): " + real_md5 +
                " (type " + real_type + ") — reconnecting");
        md5_ = md5 = real_md5;
        type_ = type = real_type;
        continue;
      }
      irap_noroslib::net_close(rfd);
      return false;
    }
    if (!negotiated) { irap_noroslib::net_close(rfd); return false; }

    UdprosReceiver rx;
    std::vector<uint8_t> buf(65536), body;
    while (running_.load() && !g_shutdown.load()) {
      ssize_t r = irap_noroslib::net_recv(rfd, buf.data(), buf.size(), 0);
      if (r <= 0) { if (!running_.load()) break; else continue; }
      UdprosHeader h;
      if (!UdprosHeader::decode(buf.data(), (size_t)r, &h)) continue;
      if (h.connection_id != conn_id) continue;
      if (rx.feed(buf.data(), (size_t)r, &body) && cb_) cb_(body);
    }
    irap_noroslib::net_close(rfd);
    return true;
  }

  // requestTopic -> connect -> handshake (with md5 discovery) -> receive loop.
  // Returns true on a clean session, false on any error (caller backs off).
  bool connect_once(const std::string& publisher_uri) {
    std::string md5 = md5_, type = type_;
    for (int attempt = 0; attempt < 2; ++attempt) {
      std::string host, err;
      uint16_t port = 0;
      if (!request_topic_tcpros(publisher_uri, node_name_, topic_, &host, &port, &err))
        return false;
      int fd = tcp_connect(host, port);
      if (fd < 0) return false;
      TcprosHeader sub = make_subscriber_header(node_name_, topic_, md5, type);
      if (!write_tcpros_header(fd, sub)) { irap_noroslib::net_close(fd); return false; }
      TcprosHeader resp;
      if (!read_tcpros_header(fd, &resp)) { irap_noroslib::net_close(fd); return false; }
      if (!resp.count("error")) {
        if (resp.count("md5sum") && resp.at("md5sum") != "*") {
          md5_ = resp.at("md5sum");
          type_ = resp.count("type") ? resp.at("type") : type_;
        }
        // The publisher hands us its full message definition here. Keep it: it is
        // what lets a subscriber decode a type it has never seen (no .msg file).
        if (resp.count("message_definition"))
          definition_ = resp.at("message_definition");
        if (on_connected_) on_connected_(type_, md5_, definition_);
        bool clean = receive_loop(fd);
        irap_noroslib::net_close(fd);
        return clean;
      }
      // md5 mismatch: learn the publisher's real type+md5 from the error text
      irap_noroslib::net_close(fd);
      std::string real_type, real_md5;
      if (!parse_type_md5_from_error(resp.at("error"), &real_type, &real_md5) ||
          real_md5.empty() || attempt == 1) {
        logerr("handshake rejected on " + topic_ + ": " + resp.at("error"));
        return false;
      }
      logwarn(">>> DISCOVERED real md5 for " + topic_ + ": " + real_md5 + " (type " + real_type +
              ") — reconnecting");
      md5_ = md5 = real_md5;
      type_ = type = real_type;
    }
    return false;
  }

  bool receive_loop(int fd) {
    std::vector<uint8_t> body;
    while (running_.load() && !g_shutdown.load()) {
      if (!recv_framed(fd, &body)) return false;  // publisher closed
      if (cb_) cb_(body);
    }
    return true;
  }

  std::string topic_, type_, md5_;
  std::string definition_;          // learned from the publisher's handshake
  OnConnected on_connected_;
  RawCallback cb_;
  std::string node_name_, host_, transport_;
  std::atomic<bool> running_{true};
  std::mutex mu_;
  std::map<std::string, bool> links_;  // publisher_uri -> active
};

// --------------------------------------------------------------------------
// ServiceServer: a rosrpc TCP endpoint answering one service.
// --------------------------------------------------------------------------
class ServiceServer {
 public:
  ServiceServer(std::string name, std::string type, std::string md5, std::string req_type,
                std::string resp_type, RawServiceHandler handler, std::string node_name)
      : name_(std::move(name)), type_(std::move(type)), md5_(std::move(md5)),
        req_type_(std::move(req_type)), resp_type_(std::move(resp_type)),
        handler_(std::move(handler)), node_name_(std::move(node_name)) {}

  // Same as Publication: stop() owns accept_thread_, so it must run on destruction
  // or letting a ServiceServer go out of scope aborts the process.
  ~ServiceServer() { stop(); }

  bool start(const std::string& host) {
    listen_fd_ = tcp_listen("0.0.0.0", 0, &port_);
    if (listen_fd_ < 0) return false;
    uri_ = "rosrpc://" + host + ":" + std::to_string(port_);
    running_.store(true);
    accept_thread_ = std::thread([this] { accept_loop(); });
    return true;
  }

  void stop() {
    if (!running_.exchange(false)) return;
    if (listen_fd_ >= 0) { ::shutdown(listen_fd_, SHUT_RDWR); irap_noroslib::net_close(listen_fd_); listen_fd_ = -1; }
    if (accept_thread_.joinable()) accept_thread_.join();
  }

  const std::string& name() const { return name_; }
  const std::string& uri() const { return uri_; }

 private:
  void accept_loop() {
    while (running_.load()) {
      int fd = ::accept(listen_fd_, nullptr, nullptr);
      if (fd < 0) { if (running_.load()) continue; else break; }
      set_tcp_nodelay(fd);
      std::thread([this, fd] { serve(fd); }).detach();
    }
  }

  void serve(int fd) {
    TcprosHeader hdr;
    if (!read_tcpros_header(fd, &hdr)) { irap_noroslib::net_close(fd); return; }
    std::string want = hdr.count("md5sum") ? hdr.at("md5sum") : "*";
    if (want != "*" && want != md5_) {
      TcprosHeader err;
      err["error"] = "service " + name_ + " md5 mismatch: client wants [" + want +
                     "], server has [" + md5_ + "]";
      write_tcpros_header(fd, err);
      irap_noroslib::net_close(fd);
      return;
    }
    TcprosHeader resp;
    resp["callerid"] = node_name_;
    resp["md5sum"] = md5_;
    resp["type"] = type_;
    resp["request_type"] = req_type_;
    resp["response_type"] = resp_type_;
    if (!write_tcpros_header(fd, resp)) { irap_noroslib::net_close(fd); return; }
    if (hdr.count("probe") && hdr.at("probe") == "1") { irap_noroslib::net_close(fd); return; }
    bool persistent = hdr.count("persistent") &&
                      (hdr.at("persistent") == "1" || hdr.at("persistent") == "true");
    std::vector<uint8_t> req_body, resp_body;
    do {
      if (!recv_framed(fd, &req_body)) break;  // caller closed
      resp_body.clear();
      bool ok = false;
      try {
        ok = handler_(req_body, resp_body);
      } catch (const std::exception& e) {
        ok = false;
        resp_body.assign((const uint8_t*)e.what(), (const uint8_t*)e.what() + std::strlen(e.what()));
      }
      if (!ok && resp_body.empty()) {
        const char* m = "service handler failed";
        resp_body.assign((const uint8_t*)m, (const uint8_t*)m + std::strlen(m));
      }
      if (!send_service_response(fd, ok, resp_body)) break;
    } while (persistent && running_.load());
    irap_noroslib::net_close(fd);
  }

  std::string name_, type_, md5_, req_type_, resp_type_, node_name_, uri_;
  RawServiceHandler handler_;
  int listen_fd_ = -1;
  uint16_t port_ = 0;
  std::atomic<bool> running_{false};
  std::thread accept_thread_;
};

}  // namespace detail

// --------------------------------------------------------------------------
// Node singleton
// --------------------------------------------------------------------------
namespace {
using detail::Publication;
using detail::ServiceServer;
using detail::Subscription;

class Node {
 public:
  Node(const std::string& name, const std::string& master_uri, const std::string& host)
      : name_(name.empty() || name[0] == '/' ? name : "/" + name),
        slave_(name_, master_uri) {
    master_uri_ = master_uri;
    host_ = host;
    slave_.set_advertised_host(host_);
    slave_.set_request_topic([this](const std::string& topic,
                                     const std::vector<XmlValue>& protocols) {
      return on_request_topic(topic, protocols);
    });
    slave_.set_publisher_update([this](const std::string& topic,
                                       const std::vector<std::string>& pubs) {
      std::lock_guard<std::mutex> lk(mu_);
      auto it = subs_.find(topic);
      if (it != subs_.end()) it->second->update_publishers(pubs);
    });
    slave_.start("0.0.0.0", 0);
    caller_api_ = slave_.caller_api();
    std::signal(SIGINT, on_sigint);
    loginfo("node " + name_ + ": master=" + master_uri_ + " slave=" + caller_api_);
    // Watch the master so the node survives it going away and rejoins when it
    // returns -- exactly like a real roscpp/rospy node. P2P TCPROS links already
    // outlive the master on their own; this re-registers us so peers can find us
    // again after a roscore restart (or a roscore that starts after we do).
    keeper_ = std::thread([this] { master_keeper(); });
  }

  ~Node() {
    keeper_stop_.store(true);
    if (keeper_.joinable()) keeper_.join();
  }

  const std::string& name() const { return name_; }
  const std::string& master_uri() const { return master_uri_; }
  const std::string& caller_api() const { return caller_api_; }
  const std::string& host() const { return host_; }

  std::shared_ptr<Publication> advertise(const std::string& topic, const std::string& type,
                                         const std::string& md5, const std::string& def,
                                         bool latch) {
    auto pub = std::make_shared<Publication>(topic, type, md5, def, latch, host_);
    pub->node_name_ = name_;
    if (!pub->start()) { logerr("failed to open TCPROS server for " + topic); return pub; }
    {
      std::lock_guard<std::mutex> lk(mu_);
      pubs_[topic] = pub;
    }
    do_register_publisher(pub);
    return pub;
  }

  // Resilient master registrations. None of these throw or abort the node: if the
  // master is down the local pub/sub/service still exists, and the keeper thread
  // re-registers it when the master is reachable again.
  bool do_register_publisher(const std::shared_ptr<Publication>& pub) {
    std::vector<std::string> subs; std::string err;
    if (register_publisher(master_uri_, name_, pub->topic(), pub->type(), caller_api_,
                           &subs, &err)) {
      master_recovered();
      return true;
    }
    master_lost(err);
    return false;
  }

  bool do_register_subscriber(const std::shared_ptr<Subscription>& sub) {
    std::vector<std::string> pubs; std::string err;
    if (register_subscriber(master_uri_, name_, sub->topic(), sub->type(), caller_api_,
                            &pubs, &err)) {
      sub->update_publishers(pubs);
      master_recovered();
      return true;
    }
    master_lost(err);
    return false;
  }

  bool do_register_service(const std::shared_ptr<ServiceServer>& srv) {
    std::string err;
    if (register_service(master_uri_, name_, srv->name(), srv->uri(), caller_api_, &err)) {
      master_recovered();
      return true;
    }
    master_lost(err);
    return false;
  }

  std::shared_ptr<Subscription> subscribe(const std::string& topic, const std::string& type,
                                          const std::string& md5, detail::RawCallback cb,
                                          const std::string& transport,
                                          Subscription::OnConnected on_connected = nullptr) {
    auto sub = std::make_shared<Subscription>(topic, type, md5, std::move(cb), name_, host_,
                                              transport);
    // installed BEFORE we register, so no handshake can outrun it
    if (on_connected) sub->set_on_connected(std::move(on_connected));
    {
      std::lock_guard<std::mutex> lk(mu_);
      subs_[topic] = sub;
    }
    do_register_subscriber(sub);
    return sub;
  }

  void unadvertise(const std::shared_ptr<Publication>& pub) {
    if (!pub) return;
    std::string err;
    unregister_publisher(master_uri_, name_, pub->topic(), caller_api_, &err);
    pub->stop();
    std::lock_guard<std::mutex> lk(mu_);
    pubs_.erase(pub->topic());
  }

  void unsubscribe(const std::shared_ptr<Subscription>& sub) {
    if (!sub) return;
    std::string err;
    unregister_subscriber(master_uri_, name_, sub->topic(), caller_api_, &err);
    sub->stop();
    std::lock_guard<std::mutex> lk(mu_);
    subs_.erase(sub->topic());
  }

  std::shared_ptr<ServiceServer> advertise_service(const std::string& name, const std::string& type,
                                                   const std::string& md5,
                                                   const std::string& req_type,
                                                   const std::string& resp_type,
                                                   detail::RawServiceHandler handler) {
    auto srv = std::make_shared<ServiceServer>(name, type, md5, req_type, resp_type,
                                               std::move(handler), name_);
    if (!srv->start(host_)) { logerr("failed to open rosrpc server for " + name); return srv; }
    {
      std::lock_guard<std::mutex> lk(mu_);
      services_[name] = srv;
    }
    do_register_service(srv);
    return srv;
  }

  void unadvertise_service(const std::shared_ptr<ServiceServer>& srv) {
    if (!srv) return;
    std::string err;
    unregister_service(master_uri_, name_, srv->name(), srv->uri(), &err);
    srv->stop();
    std::lock_guard<std::mutex> lk(mu_);
    services_.erase(srv->name());
  }

  bool call_service(const std::string& name, const std::string& md5,
                    const std::vector<uint8_t>& request, std::vector<uint8_t>* response,
                    std::string* err) {
    std::string url;
    if (!lookup_service(master_uri_, name_, name, &url, err)) return false;
    std::string host; uint16_t port = 0;
    if (!parse_rosrpc(url, &host, &port)) { *err = "bad rosrpc uri: " + url; return false; }
    int fd = tcp_connect(host, port);
    if (fd < 0) { *err = "connect failed to " + url; return false; }
    struct Guard { int f; ~Guard() { if (f >= 0) irap_noroslib::net_close(f); } } guard{fd};
    TcprosHeader h;
    h["callerid"] = name_;
    h["service"] = name;
    h["md5sum"] = md5;
    if (!write_tcpros_header(fd, h)) { *err = "write header failed"; return false; }
    TcprosHeader resp_hdr;
    if (!read_tcpros_header(fd, &resp_hdr)) { *err = "read header failed"; return false; }
    if (resp_hdr.count("error")) { *err = resp_hdr.at("error"); return false; }
    if (!send_framed(fd, request)) { *err = "send request failed"; return false; }
    bool ok = false;
    if (!recv_service_response(fd, &ok, response)) { *err = "read response failed"; return false; }
    if (!ok) {
      *err = "service error: " + std::string(response->begin(), response->end());
      return false;
    }
    return true;
  }

  bool probe_service(const std::string& name,
                     std::map<std::string, std::string>* reply, std::string* err) {
    std::string url;
    if (!lookup_service(master_uri_, name_, name, &url, err)) return false;
    std::string host; uint16_t port = 0;
    if (!parse_rosrpc(url, &host, &port)) { *err = "bad rosrpc uri: " + url; return false; }
    int fd = tcp_connect(host, port);
    if (fd < 0) { *err = "connect failed to " + url; return false; }
    struct Guard { int f; ~Guard() { if (f >= 0) irap_noroslib::net_close(f); } } guard{fd};
    TcprosHeader h;
    h["callerid"] = name_;
    h["service"] = name;
    h["md5sum"] = "*";          // wildcard: the probe must never md5-mismatch
    h["probe"] = "1";
    if (!write_tcpros_header(fd, h)) { *err = "write header failed"; return false; }
    if (!read_tcpros_header(fd, reply)) { *err = "read header failed"; return false; }
    if (reply->count("error")) { *err = reply->at("error"); return false; }
    return true;
  }

  bool wait_for_service(const std::string& name, double timeout_s) {
    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                        std::chrono::duration<double>(timeout_s < 0 ? 0 : timeout_s));
    while (!g_shutdown.load()) {
      std::string url, err;
      if (lookup_service(master_uri_, name_, name, &url, &err) && !url.empty()) return true;
      if (timeout_s >= 0 && std::chrono::steady_clock::now() >= deadline) return false;
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return false;
  }

  void shutdown() {
    keeper_stop_.store(true);          // stop re-registering as we tear down
    std::lock_guard<std::mutex> lk(mu_);
    std::string err;
    for (auto& kv : pubs_) {
      unregister_publisher(master_uri_, name_, kv.first, caller_api_, &err);
      kv.second->stop();
    }
    for (auto& kv : subs_) {
      unregister_subscriber(master_uri_, name_, kv.first, caller_api_, &err);
      kv.second->stop();
    }
    for (auto& kv : services_) {
      unregister_service(master_uri_, name_, kv.first, kv.second->uri(), &err);
      kv.second->stop();
    }
    pubs_.clear(); subs_.clear(); services_.clear();
    slave_.stop();
  }

 private:
  void master_lost(const std::string& err) {
    std::lock_guard<std::mutex> lk(state_mu_);
    master_up_ = false;
    if (!warned_lost_) {
      warned_lost_ = true;
      logwarn("lost the ROS master at " + master_uri_ + " (" + err + "); the node "
              "keeps running and will re-register when it returns");
    }
  }

  void master_recovered() {
    std::lock_guard<std::mutex> lk(state_mu_);
    master_up_ = true;
    warned_lost_ = false;
  }

  // Re-announce every publisher, subscriber and service. Master registration is
  // idempotent, so repeating it is safe; this is how a node rejoins a roscore
  // that restarted (or started after the node did).
  bool reregister_all() {
    std::vector<std::shared_ptr<Publication>> pubs;
    std::vector<std::shared_ptr<Subscription>> subs;
    std::vector<std::shared_ptr<ServiceServer>> srvs;
    {
      std::lock_guard<std::mutex> lk(mu_);
      for (auto& kv : pubs_) pubs.push_back(kv.second);
      for (auto& kv : subs_) subs.push_back(kv.second);
      for (auto& kv : services_) srvs.push_back(kv.second);
    }
    bool ok = true;
    for (auto& p : pubs) ok = do_register_publisher(p) && ok;
    for (auto& s : subs) ok = do_register_subscriber(s) && ok;
    for (auto& s : srvs) ok = do_register_service(s) && ok;
    return ok;
  }

  void master_keeper() {
    while (!keeper_stop_.load() && !g_shutdown.load()) {
      for (int i = 0; i < 20 && !keeper_stop_.load() && !g_shutdown.load(); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));   // ~2s, responsive to stop
      if (keeper_stop_.load() || g_shutdown.load()) break;
      bool was_up, has_regs;
      { std::lock_guard<std::mutex> lk(state_mu_); was_up = master_up_; }
      { std::lock_guard<std::mutex> lk(mu_);
        has_regs = !pubs_.empty() || !subs_.empty() || !services_.empty(); }
      int pid = 0; std::string err;
      if (!master_get_pid(master_uri_, name_, &pid, &err)) { master_lost(err); continue; }
      // Master answered. If it had been down, re-register everything so peers can
      // find us again, then announce the recovery.
      if (!was_up) {
        size_t np, ns, nk;
        { std::lock_guard<std::mutex> lk(mu_);
          np = pubs_.size(); ns = subs_.size(); nk = services_.size(); }
        if (has_regs && reregister_all())
          loginfo("ROS master back online at " + master_uri_ + " -- re-registered " +
                  std::to_string(np) + " publisher(s), " + std::to_string(ns) +
                  " subscriber(s), " + std::to_string(nk) + " service(s)");
      }
      master_recovered();
    }
  }

  XmlValue on_request_topic(const std::string& topic, const std::vector<XmlValue>& protocols) {
    std::shared_ptr<Publication> pub;
    {
      std::lock_guard<std::mutex> lk(mu_);
      auto it = pubs_.find(topic);
      if (it != pubs_.end()) pub = it->second;
    }
    if (!pub) return XmlValue::Array({});
    // Honor the subscriber's protocol preference order (UDPROS or TCPROS).
    for (const auto& proto : protocols) {
      if (proto.type != XmlValue::Type::Array || proto.arr.empty()) continue;
      std::string name = proto.arr[0].as_str();
      if (name == "UDPROS") {
        XmlValue resp = pub->handle_udpros_request(proto);
        if (resp.type == XmlValue::Type::Array && !resp.arr.empty()) return resp;
      } else if (name == "TCPROS") {
        return XmlValue::Array({XmlValue::Str("TCPROS"), XmlValue::Str(pub->host()),
                                XmlValue::Int(pub->port())});
      }
    }
    return XmlValue::Array({});
  }

  std::string name_, master_uri_, caller_api_, host_;
  SlaveServer slave_;
  std::mutex mu_;
  std::map<std::string, std::shared_ptr<Publication>> pubs_;
  std::map<std::string, std::shared_ptr<Subscription>> subs_;
  std::map<std::string, std::shared_ptr<ServiceServer>> services_;
  // Master-connection state, watched by the keeper thread.
  std::mutex state_mu_;
  bool master_up_ = true;             // optimistic until proven otherwise
  bool warned_lost_ = false;
  std::thread keeper_;
  std::atomic<bool> keeper_stop_{false};
};

std::unique_ptr<Node> g_node;

// Library-level configuration, settable before init_node.
std::string g_cfg_master_uri;
std::string g_cfg_hostname;

std::string resolve_master_uri(const std::string& explicit_uri) {
  if (!explicit_uri.empty()) return explicit_uri;
  if (!g_cfg_master_uri.empty()) return g_cfg_master_uri;
  const char* env = std::getenv("ROS_MASTER_URI");
  return env ? std::string(env) : "http://127.0.0.1:11311/";
}

std::string resolve_host(const std::string& explicit_host, const std::string& master_uri) {
  if (!explicit_host.empty()) return explicit_host;
  if (!g_cfg_hostname.empty()) return g_cfg_hostname;
  if (const char* ip = std::getenv("ROS_IP")) return ip;
  if (const char* hn = std::getenv("ROS_HOSTNAME")) return hn;
  std::string mhost; uint16_t mport = 0;
  parse_http_uri(master_uri, &mhost, &mport);
  return guess_advertised_host(mhost);
}
}  // namespace

// --------------------------------------------------------------------------
// public API
// --------------------------------------------------------------------------
void set_master_uri(const std::string& uri) { g_cfg_master_uri = uri; }
void set_hostname(const std::string& host) { g_cfg_hostname = host; }
void configure(const std::string& master_uri, const std::string& host) {
  if (!master_uri.empty()) g_cfg_master_uri = master_uri;
  if (!host.empty()) g_cfg_hostname = host;
}

void init_node(const std::string& name, const std::string& master_uri, const std::string& host) {
  if (g_node) return;
  irap_noroslib::net_startup();  // WSAStartup on Windows; no-op on POSIX
  std::string uri = resolve_master_uri(master_uri);
  std::string h = resolve_host(host, uri);
  g_node.reset(new Node(name, uri, h));
}

bool ok() { return !g_shutdown.load(); }

void shutdown(const std::string& reason) {
  if (g_shutdown.exchange(true)) return;
  if (!reason.empty()) loginfo("shutting down: " + reason);
  if (g_node) g_node->shutdown();
}

void spin() {
  std::signal(SIGINT, on_sigint);
  while (!g_shutdown.load()) std::this_thread::sleep_for(std::chrono::milliseconds(100));
  shutdown("spin exit");
}

namespace detail {
std::shared_ptr<Publication> advertise(const std::string& topic, const std::string& type,
                                       const std::string& md5, const std::string& definition,
                                       bool latch) {
  return g_node->advertise(topic, type, md5, definition, latch);
}
void publish_raw(Publication& pub, const std::vector<uint8_t>& body) { pub.publish(body); }
int num_connections(Publication& pub) { return pub.num_connections(); }
void unadvertise(const std::shared_ptr<Publication>& pub) { if (g_node) g_node->unadvertise(pub); }

std::shared_ptr<Subscription> subscribe(const std::string& topic, const std::string& type,
                                        const std::string& md5, RawCallback cb,
                                        const std::string& transport) {
  return g_node->subscribe(topic, type, md5, std::move(cb), transport);
}
void unsubscribe(const std::shared_ptr<Subscription>& sub) { if (g_node) g_node->unsubscribe(sub); }

std::shared_ptr<Subscription> subscribe_any(const std::string& topic, AnyCallback cb) {
  // "*" is the ROS wildcard: the publisher accepts it and answers with its real
  // type, md5 and message_definition. Both callbacks are installed before the
  // subscription registers, so the handshake cannot outrun them.
  struct Learned {
    std::mutex mu;
    std::string type, md5, def;
  };
  auto st = std::make_shared<Learned>();
  return g_node->subscribe(
      topic, "*", "*",
      [st, cb](const std::vector<uint8_t>& body) {
        std::string t, m, d;
        {
          std::lock_guard<std::mutex> lk(st->mu);
          t = st->type; m = st->md5; d = st->def;
        }
        cb(t, m, d, body);
      },
      "tcpros",
      [st](const std::string& t, const std::string& m, const std::string& d) {
        std::lock_guard<std::mutex> lk(st->mu);
        st->type = t; st->md5 = m; st->def = d;
      });
}

std::shared_ptr<ServiceServer> advertise_service(const std::string& name, const std::string& type,
                                                 const std::string& md5,
                                                 const std::string& req_type,
                                                 const std::string& resp_type,
                                                 RawServiceHandler handler) {
  return g_node->advertise_service(name, type, md5, req_type, resp_type, std::move(handler));
}
void unadvertise_service(const std::shared_ptr<ServiceServer>& srv) {
  if (g_node) g_node->unadvertise_service(srv);
}
bool call_service(const std::string& name, const std::string& md5,
                  const std::vector<uint8_t>& request, std::vector<uint8_t>* response,
                  std::string* err) {
  return g_node->call_service(name, md5, request, response, err);
}
bool wait_for_service(const std::string& name, double timeout_s) {
  return g_node ? g_node->wait_for_service(name, timeout_s) : false;
}
bool probe_service(const std::string& name,
                   std::map<std::string, std::string>* reply, std::string* err) {
  if (!g_node) { *err = "call init_node() first"; return false; }
  return g_node->probe_service(name, reply, err);
}
}  // namespace detail

// --- parameter server ------------------------------------------------------
namespace {
bool param_get(const std::string& key, XmlValue* v) {
  std::string err;
  return g_node && get_param(g_node->master_uri(), g_node->name(), key, v, &err);
}
void param_set(const std::string& key, const XmlValue& v) {
  std::string err;
  if (g_node && !set_param(g_node->master_uri(), g_node->name(), key, v, &err))
    logwarn("set_param(" + key + ") failed: " + err);
}
}  // namespace

bool get_param(const std::string& key, int* out) {
  XmlValue v;
  if (!param_get(key, &v) || (v.type != XmlValue::Type::Int && v.type != XmlValue::Type::Bool))
    return false;
  *out = static_cast<int>(v.as_int());
  return true;
}
bool get_param(const std::string& key, double* out) {
  XmlValue v;
  if (!param_get(key, &v)) return false;
  if (v.type == XmlValue::Type::Double) { *out = v.d; return true; }
  if (v.type == XmlValue::Type::Int) { *out = static_cast<double>(v.i); return true; }
  return false;
}
bool get_param(const std::string& key, bool* out) {
  XmlValue v;
  if (!param_get(key, &v)) return false;
  if (v.type == XmlValue::Type::Bool) { *out = v.b; return true; }
  if (v.type == XmlValue::Type::Int) { *out = v.i != 0; return true; }
  return false;
}
bool get_param(const std::string& key, std::string* out) {
  XmlValue v;
  if (!param_get(key, &v) || v.type != XmlValue::Type::String) return false;
  *out = v.s;
  return true;
}
void set_param(const std::string& key, int value) { param_set(key, XmlValue::Int(value)); }
void set_param(const std::string& key, double value) { param_set(key, XmlValue::Double(value)); }
void set_param(const std::string& key, bool value) { param_set(key, XmlValue::Bool(value)); }
void set_param(const std::string& key, const std::string& value) {
  param_set(key, XmlValue::Str(value));
}
bool has_param(const std::string& key) {
  std::string err;
  bool present = false;
  return g_node && irap_noroslib::has_param(g_node->master_uri(), g_node->name(), key, &present, &err)
         && present;
}
bool delete_param(const std::string& key) {
  std::string err;
  return g_node && irap_noroslib::delete_param(g_node->master_uri(), g_node->name(), key, &err);
}

}  // namespace irap_noroslib
