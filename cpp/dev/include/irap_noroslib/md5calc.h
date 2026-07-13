// md5calc.h - MD5 (RFC 1321), used to derive ROS message md5sums from .msg text.
//
// ROS identifies a message type by the md5 of a canonical form of its .msg
// definition. To load a .msg file at runtime we must compute that md5 ourselves,
// so this is a self-contained MD5 -- no OpenSSL, no third-party library, in
// keeping with the rest of irap_noroslib.
//
// Public-domain implementation (Colin Plumb's, as used by the RFC reference and
// countless projects since). Everything is inline and namespaced, so amalgamating
// irap_noroslib into a project that already has its own MD5 cannot collide.
//
//   irap_noroslib::md5_hex("abc") == "900150983cd24fb0d6963f7d28e17f72"
#pragma once
#include <cstdint>
#include <cstring>
#include <string>

namespace irap_noroslib {
namespace md5 {

struct Ctx {
  uint32_t a = 0x67452301, b = 0xefcdab89, c = 0x98badcfe, d = 0x10325476;
  uint64_t bits = 0;      // message length in bits
  uint8_t  buf[64] = {0};
  size_t   have = 0;      // bytes currently in buf
};

inline uint32_t rol(uint32_t x, int n) { return (x << n) | (x >> (32 - n)); }

inline void transform(Ctx& s, const uint8_t* p) {
  static const uint32_t K[64] = {
      0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a,
      0xa8304613, 0xfd469501, 0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
      0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821, 0xf61e2562, 0xc040b340,
      0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
      0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8,
      0x676f02d9, 0x8d2a4c8a, 0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
      0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70, 0x289b7ec6, 0xeaa127fa,
      0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
      0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92,
      0xffeff47d, 0x85845dd1, 0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
      0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391};
  static const int R[64] = {7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
                            5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20,
                            4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
                            6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};
  uint32_t m[16];
  for (int i = 0; i < 16; ++i)      // little-endian, per the RFC
    m[i] = static_cast<uint32_t>(p[i * 4]) | (static_cast<uint32_t>(p[i * 4 + 1]) << 8) |
           (static_cast<uint32_t>(p[i * 4 + 2]) << 16) |
           (static_cast<uint32_t>(p[i * 4 + 3]) << 24);

  uint32_t a = s.a, b = s.b, c = s.c, d = s.d;
  for (int i = 0; i < 64; ++i) {
    uint32_t f;
    int g;
    if (i < 16)      { f = (b & c) | (~b & d);            g = i; }
    else if (i < 32) { f = (d & b) | (~d & c);            g = (5 * i + 1) % 16; }
    else if (i < 48) { f = b ^ c ^ d;                     g = (3 * i + 5) % 16; }
    else             { f = c ^ (b | ~d);                  g = (7 * i) % 16; }
    uint32_t tmp = d;
    d = c;
    c = b;
    b = b + rol(a + f + K[i] + m[g], R[i]);
    a = tmp;
  }
  s.a += a; s.b += b; s.c += c; s.d += d;
}

inline void update(Ctx& s, const uint8_t* data, size_t n) {
  s.bits += static_cast<uint64_t>(n) * 8;
  while (n > 0) {
    size_t take = 64 - s.have;
    if (take > n) take = n;
    std::memcpy(s.buf + s.have, data, take);
    s.have += take;
    data += take;
    n -= take;
    if (s.have == 64) {
      transform(s, s.buf);
      s.have = 0;
    }
  }
}

inline void finish(Ctx& s, uint8_t out[16]) {
  uint64_t bits = s.bits;
  uint8_t pad = 0x80;
  update(s, &pad, 1);
  uint8_t zero = 0;
  while (s.have != 56) update(s, &zero, 1);
  uint8_t len[8];   // the ORIGINAL length, saved before the padding bumped s.bits
  for (int i = 0; i < 8; ++i) len[i] = static_cast<uint8_t>(bits >> (8 * i));
  update(s, len, 8);

  const uint32_t words[4] = {s.a, s.b, s.c, s.d};
  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 4; ++j) out[i * 4 + j] = static_cast<uint8_t>(words[i] >> (8 * j));
}

}  // namespace md5

/// Hex md5 digest of `data` (lowercase, 32 chars).
inline std::string md5_hex(const std::string& data) {
  md5::Ctx s;
  md5::update(s, reinterpret_cast<const uint8_t*>(data.data()), data.size());
  uint8_t out[16];
  md5::finish(s, out);
  static const char* H = "0123456789abcdef";
  std::string hex(32, '0');
  for (int i = 0; i < 16; ++i) {
    hex[i * 2]     = H[out[i] >> 4];
    hex[i * 2 + 1] = H[out[i] & 0x0f];
  }
  return hex;
}

}  // namespace irap_noroslib
