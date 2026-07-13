// ROS UDPROS transport: the 8-byte per-datagram header, opcodes, message
// fragmentation on send, and in-order reassembly on receive.
//
// Verified against roscpp transport_udp.{h,cpp}. All header integers are
// LITTLE-ENDIAN (host order on x86/ARM-LE), NOT network order. A whole ROS
// message travels as [4B LE ros-len][body] split across DATA0 + DATAN blocks.
#pragma once

#include <sys/socket.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace irap_noroslib {

constexpr uint8_t kUdpOpData0 = 0;  // first (or only) datagram of a message
constexpr uint8_t kUdpOpDatan = 1;  // continuation datagram
constexpr uint8_t kUdpOpPing = 2;   // keepalive (unused here)
constexpr uint8_t kUdpOpErr = 3;    // error

constexpr size_t   kUdprosHeaderSize = 8;
constexpr uint32_t kUdprosDefaultMaxDatagram = 1500;

struct UdprosHeader {
  uint32_t connection_id = 0;
  uint8_t  op = 0;
  uint8_t  message_id = 0;
  uint16_t block = 0;

  void encode(uint8_t* out) const;  // 8 bytes, little-endian
  static bool decode(const uint8_t* buf, size_t len, UdprosHeader* out);
};

// Send one ROS message (already framed as [4B LE len][body] in `stream`) to dst
// as UDPROS DATA0/DATAN datagrams. message_id must be nonzero and identical for
// all fragments of this message. max_datagram_size bounds each datagram
// (payload per datagram = max_datagram_size - 8).
void udpros_send_stream(int fd, const sockaddr* dst, socklen_t dlen,
                        uint32_t connection_id, uint8_t message_id,
                        const uint8_t* stream, uint32_t stream_len,
                        uint32_t max_datagram_size);

// Reassembles UDPROS datagrams for a single connection (one publisher link).
// roscpp streams strictly in order; we mirror that (reset on DATA0, require the
// next block index to be contiguous, drop otherwise).
class UdprosReceiver {
 public:
  // Feed one received datagram. On completing a message, returns true and sets
  // *out_body to the raw ROS body (the leading 4-byte ROS length prefix stripped).
  bool feed(const uint8_t* datagram, size_t len, std::vector<uint8_t>* out_body);

 private:
  bool complete(std::vector<uint8_t>* out_body);
  bool in_progress_ = false;
  uint8_t message_id_ = 0;
  uint16_t total_blocks_ = 0;
  uint16_t next_block_ = 0;
  std::vector<uint8_t> buf_;  // accumulates [4B len][body]
};

}  // namespace irap_noroslib
