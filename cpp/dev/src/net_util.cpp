#include "noros/net_util.hpp"
#include "noros/platform.hpp"

#include <cstdlib>
#include <cstring>

namespace noros {

void set_tcp_nodelay(int fd) {
  int one = 1;
  net_setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
}

// The kernel stores SO_{RCV,SND}BUF as double the requested value (bookkeeping
// overhead) and clamps to net.core.{r,w}mem_max, so getsockopt reports ~2x what
// we set. We return that granted value for logging.
static int set_sockbuf(int fd, int opt, int bytes) {
  net_setsockopt(fd, SOL_SOCKET, opt, &bytes, sizeof(bytes));
  int actual = 0;
  socklen_t len = sizeof(actual);
  if (net_getsockopt(fd, SOL_SOCKET, opt, &actual, &len) != 0) return -1;
  return actual;
}

int set_rcvbuf(int fd, int bytes) { return set_sockbuf(fd, SO_RCVBUF, bytes); }

int set_sndbuf(int fd, int bytes) { return set_sockbuf(fd, SO_SNDBUF, bytes); }

// Connect with a bounded timeout so an unreachable peer (e.g. a node that
// advertised an unroutable ROS_IP) can't block a worker for the kernel's
// default ~127s. Uses a non-blocking connect + select.
static bool connect_timeout(int fd, const sockaddr* addr, socklen_t alen, int timeout_ms) {
  net_set_nonblocking(fd, true);
  int rc = connect(fd, addr, alen);
  bool ok = false;
  if (rc == 0) {
    ok = true;
  } else if (net_connect_in_progress()) {
    fd_set wset;
    FD_ZERO(&wset);
    FD_SET(fd, &wset);
    timeval tv{timeout_ms / 1000, (timeout_ms % 1000) * 1000};
    if (select(fd + 1, nullptr, &wset, nullptr, &tv) > 0) {
      int err = 0;
      socklen_t len = sizeof(err);
      net_getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len);
      ok = (err == 0);
    }
  }
  net_set_nonblocking(fd, false);  // restore blocking mode
  return ok;
}

int tcp_connect(const std::string& host, uint16_t port, int timeout_ms) {
  net_startup();
  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  addrinfo* res = nullptr;
  std::string port_s = std::to_string(port);
  if (getaddrinfo(host.c_str(), port_s.c_str(), &hints, &res) != 0) return -1;

  int fd = -1;
  for (addrinfo* ai = res; ai; ai = ai->ai_next) {
    fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (fd < 0) continue;
    if (connect_timeout(fd, ai->ai_addr, static_cast<socklen_t>(ai->ai_addrlen), timeout_ms)) break;
    net_close(fd);
    fd = -1;
  }
  freeaddrinfo(res);
  if (fd >= 0) set_tcp_nodelay(fd);
  return fd;
}

static int make_and_bind(int type, const std::string& bind_ip, uint16_t port,
                         uint16_t* out_port) {
  net_startup();
  int fd = socket(AF_INET, type, 0);
  if (fd < 0) return -1;
  int one = 1;
  net_setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  if (bind_ip.empty()) {
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
  } else if (inet_pton(AF_INET, bind_ip.c_str(), &addr.sin_addr) != 1) {
    net_close(fd);
    return -1;
  }
  if (bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    net_close(fd);
    return -1;
  }
  if (out_port) {
    sockaddr_in bound{};
    socklen_t len = sizeof(bound);
    getsockname(fd, reinterpret_cast<sockaddr*>(&bound), &len);
    *out_port = ntohs(bound.sin_port);
  }
  return fd;
}

int tcp_listen(const std::string& bind_ip, uint16_t port, uint16_t* out_port, int backlog) {
  int fd = make_and_bind(SOCK_STREAM, bind_ip, port, out_port);
  if (fd < 0) return -1;
  if (listen(fd, backlog) != 0) {
    net_close(fd);
    return -1;
  }
  return fd;
}

int udp_socket(const std::string& bind_ip, uint16_t port, uint16_t* out_port) {
  if (bind_ip.empty() && port == 0) {
    // Send-only socket: no bind needed.
    net_startup();
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (out_port) *out_port = 0;
    return fd;
  }
  return make_and_bind(SOCK_DGRAM, bind_ip, port, out_port);
}

bool read_n(int fd, void* buf, size_t n) {
  auto* p = static_cast<uint8_t*>(buf);
  size_t got = 0;
  while (got < n) {
    ssize_t r = net_recv(fd, p + got, n - got, 0);
    if (r == 0) return false;  // peer closed
    if (r < 0) {
      if (net_was_interrupted()) continue;
      return false;
    }
    got += static_cast<size_t>(r);
  }
  return true;
}

bool write_n(int fd, const void* buf, size_t n) {
  const auto* p = static_cast<const uint8_t*>(buf);
  size_t sent = 0;
  while (sent < n) {
    ssize_t r = net_send(fd, p + sent, n - sent, MSG_NOSIGNAL);
    if (r < 0) {
      if (net_was_interrupted()) continue;
      return false;
    }
    sent += static_cast<size_t>(r);
  }
  return true;
}

std::string guess_advertised_host(const std::string& master_host) {
  // Connect a UDP socket toward the master and read back the chosen local IP.
  net_startup();
  int fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) return "127.0.0.1";

  addrinfo hints{};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;
  addrinfo* res = nullptr;
  std::string host = master_host.empty() ? "127.0.0.1" : master_host;
  std::string result = "127.0.0.1";
  if (getaddrinfo(host.c_str(), "11311", &hints, &res) == 0 && res) {
    if (connect(fd, res->ai_addr, static_cast<socklen_t>(res->ai_addrlen)) == 0) {
      sockaddr_in local{};
      socklen_t len = sizeof(local);
      if (getsockname(fd, reinterpret_cast<sockaddr*>(&local), &len) == 0) {
        char buf[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &local.sin_addr, buf, sizeof(buf))) result = buf;
      }
    }
    freeaddrinfo(res);
  }
  net_close(fd);
  return result;
}

bool parse_http_uri(const std::string& uri, std::string* host, uint16_t* port) {
  // Expect http://host:port/ (path optional).
  const std::string prefix = "http://";
  if (uri.compare(0, prefix.size(), prefix) != 0) return false;
  size_t start = prefix.size();
  size_t slash = uri.find('/', start);
  std::string authority = uri.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
  size_t colon = authority.rfind(':');
  if (colon == std::string::npos) return false;
  *host = authority.substr(0, colon);
  int p = std::atoi(authority.c_str() + colon + 1);
  if (p <= 0 || p > 65535) return false;
  *port = static_cast<uint16_t>(p);
  return true;
}

}  // namespace noros
