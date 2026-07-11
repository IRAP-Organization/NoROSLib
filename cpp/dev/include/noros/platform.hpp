// platform.hpp -- socket / OS compatibility layer.
//
// Lets the same source build on POSIX (Linux/macOS/WSL) and native Windows
// (Winsock2). On Windows you must link ws2_32 (MSVC does it via #pragma comment;
// with MinGW pass -lws2_32). Everything here is header-only and inline.
#pragma once

#if defined(_WIN32)
  #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
  #endif
  #ifndef NOMINMAX
  #define NOMINMAX
  #endif
  // winsock2.h MUST come before windows.h, or later <thread>/<mutex> (which pull
  // in windows.h on MSVC) would drag in the old winsock1 and conflict.
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #include <windows.h>
  #include <basetsd.h>
  #if defined(_MSC_VER)
    #pragma comment(lib, "ws2_32.lib")
  #endif
  // ssize_t isn't declared on Windows unless <sys/types.h> was pulled in (which
  // we don't include here); define it from Win32's SSIZE_T for MSVC and MinGW.
  #ifndef _SSIZE_T_DEFINED
    #define _SSIZE_T_DEFINED
    typedef SSIZE_T ssize_t;
  #endif
  #ifndef MSG_NOSIGNAL
  #define MSG_NOSIGNAL 0        // Windows never raises SIGPIPE
  #endif
  #ifndef SHUT_RDWR
  #define SHUT_RDWR SD_BOTH
  #endif
#else
  #include <arpa/inet.h>
  #include <fcntl.h>
  #include <netdb.h>
  #include <netinet/in.h>
  #include <netinet/tcp.h>
  #include <sys/socket.h>
  #include <sys/time.h>
  #include <time.h>
  #include <unistd.h>
#endif

#include <cerrno>
#include <cstddef>
#include <cstdint>

namespace noros {

// One-time Winsock startup (no-op on POSIX). Idempotent; call before any socket.
inline void net_startup() {
#if defined(_WIN32)
  static bool started = [] { WSADATA w; return WSAStartup(MAKEWORD(2, 2), &w) == 0; }();
  (void)started;
#endif
}

// Close a socket descriptor portably.
inline int net_close(int fd) {
#if defined(_WIN32)
  return ::closesocket(fd);
#else
  return ::close(fd);
#endif
}

// Toggle blocking mode. Returns true on success.
inline bool net_set_nonblocking(int fd, bool nonblocking) {
#if defined(_WIN32)
  u_long mode = nonblocking ? 1u : 0u;
  return ioctlsocket(fd, FIONBIO, &mode) == 0;
#else
  int flags = ::fcntl(fd, F_GETFL, 0);
  if (flags < 0) return false;
  flags = nonblocking ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
  return ::fcntl(fd, F_SETFL, flags) == 0;
#endif
}

// True if a non-blocking connect() is still in progress (EINPROGRESS / WSAEWOULDBLOCK).
inline bool net_connect_in_progress() {
#if defined(_WIN32)
  return WSAGetLastError() == WSAEWOULDBLOCK;
#else
  return errno == EINPROGRESS;
#endif
}

// True if the last recv/send failed with EINTR (never on Windows).
inline bool net_was_interrupted() {
#if defined(_WIN32)
  return false;
#else
  return errno == EINTR;
#endif
}

// recv/send/sendto/setsockopt/getsockopt: Winsock takes char* buffers and int
// lengths where POSIX takes void*/size_t. These thin wrappers hide the difference.
inline ssize_t net_recv(int fd, void* buf, size_t n, int flags) {
#if defined(_WIN32)
  return ::recv(fd, reinterpret_cast<char*>(buf), static_cast<int>(n), flags);
#else
  return ::recv(fd, buf, n, flags);
#endif
}

inline ssize_t net_send(int fd, const void* buf, size_t n, int flags) {
#if defined(_WIN32)
  return ::send(fd, reinterpret_cast<const char*>(buf), static_cast<int>(n), flags);
#else
  return ::send(fd, buf, n, flags);
#endif
}

inline ssize_t net_sendto(int fd, const void* buf, size_t n, int flags,
                          const sockaddr* dst, socklen_t dlen) {
#if defined(_WIN32)
  return ::sendto(fd, reinterpret_cast<const char*>(buf), static_cast<int>(n), flags, dst, dlen);
#else
  return ::sendto(fd, buf, n, flags, dst, dlen);
#endif
}

inline int net_setsockopt(int fd, int level, int opt, const void* val, socklen_t len) {
#if defined(_WIN32)
  return ::setsockopt(fd, level, opt, reinterpret_cast<const char*>(val), len);
#else
  return ::setsockopt(fd, level, opt, val, len);
#endif
}

inline int net_getsockopt(int fd, int level, int opt, void* val, socklen_t* len) {
#if defined(_WIN32)
  return ::getsockopt(fd, level, opt, reinterpret_cast<char*>(val), len);
#else
  return ::getsockopt(fd, level, opt, val, len);
#endif
}

// Set a receive timeout in milliseconds (SO_RCVTIMEO takes a DWORD on Windows,
// a struct timeval on POSIX).
inline int net_set_rcvtimeo_ms(int fd, int ms) {
#if defined(_WIN32)
  DWORD to = static_cast<DWORD>(ms);
  return ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO,
                      reinterpret_cast<const char*>(&to), sizeof(to));
#else
  timeval tv{ms / 1000, (ms % 1000) * 1000};
  return ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif
}

// Wall-clock time as (seconds, nanoseconds) since the Unix epoch -- a portable
// stand-in for clock_gettime(CLOCK_REALTIME).
inline void wall_time(int64_t* sec, int64_t* nsec) {
#if defined(_WIN32)
  FILETIME ft;
  GetSystemTimeAsFileTime(&ft);
  unsigned long long t = (static_cast<unsigned long long>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
  t -= 116444736000000000ULL;  // 1601-01-01 -> 1970-01-01, in 100ns ticks
  *sec = static_cast<int64_t>(t / 10000000ULL);
  *nsec = static_cast<int64_t>((t % 10000000ULL) * 100);
#else
  timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  *sec = ts.tv_sec;
  *nsec = ts.tv_nsec;
#endif
}

}  // namespace noros
