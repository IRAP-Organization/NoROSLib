// Small socket helpers shared across the bridge. Header-only, no ROS deps.
#pragma once

#include "noros/platform.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace noros {

// Enable TCP_NODELAY (disable Nagle) — important for low-latency TCPROS.
void set_tcp_nodelay(int fd);

// Request a larger SO_RCVBUF (kernel clamps to net.core.rmem_max). Important on
// the UDP publish path: one large ROS message arrives as a burst of fragment
// datagrams, so a small receive buffer drops whole frames. Best-effort.
// Returns the buffer size the kernel actually granted (via getsockopt), or -1.
int set_rcvbuf(int fd, int bytes);

// Request a larger SO_SNDBUF (kernel clamps to net.core.wmem_max). On the UDP
// subscribe path a large ROS message is emitted as a burst of fragment
// datagrams; a bigger send buffer smooths that burst. Best-effort.
// Returns the buffer size the kernel actually granted, or -1.
int set_sndbuf(int fd, int bytes);

// Blocking connect to host:port (numeric or hostname). Returns fd or -1.
// The returned socket has TCP_NODELAY set.
int tcp_connect(const std::string& host, uint16_t port, int timeout_ms = 5000);

// Create a listening TCP socket bound to bind_ip:port. port may be 0 for an
// ephemeral port; the actual bound port is written back to *out_port.
// Returns fd or -1.
int tcp_listen(const std::string& bind_ip, uint16_t port, uint16_t* out_port, int backlog = 16);

// Create a UDP socket. If bind_ip is non-empty, bind to bind_ip:port
// (port may be 0). Returns fd or -1. Actual bound port -> *out_port if given.
int udp_socket(const std::string& bind_ip, uint16_t port, uint16_t* out_port = nullptr);

// Read exactly n bytes into buf. Returns true on success, false on EOF/error.
bool read_n(int fd, void* buf, size_t n);

// Write exactly n bytes from buf. Returns true on success.
bool write_n(int fd, const void* buf, size_t n);

// Best-effort local IP the bridge should advertise to other ROS nodes.
// Resolves the outbound interface toward master_host; falls back to hostname.
std::string guess_advertised_host(const std::string& master_host);

// Split a "http://host:port/" URI into host + port. Returns false on parse error.
bool parse_http_uri(const std::string& uri, std::string* host, uint16_t* port);

}  // namespace noros
