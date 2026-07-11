#include "noros/udpros.hpp"
#include "noros/platform.hpp"

#include <cstring>

namespace noros {
namespace {

inline void put_u16(uint8_t* p, uint16_t v) {
  p[0] = static_cast<uint8_t>(v);
  p[1] = static_cast<uint8_t>(v >> 8);
}
inline void put_u32(uint8_t* p, uint32_t v) {
  p[0] = static_cast<uint8_t>(v);
  p[1] = static_cast<uint8_t>(v >> 8);
  p[2] = static_cast<uint8_t>(v >> 16);
  p[3] = static_cast<uint8_t>(v >> 24);
}
inline uint16_t get_u16(const uint8_t* p) {
  return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}
inline uint32_t get_u32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
         (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

}  // namespace

void UdprosHeader::encode(uint8_t* out) const {
  put_u32(out, connection_id);
  out[4] = op;
  out[5] = message_id;
  put_u16(out + 6, block);
}

bool UdprosHeader::decode(const uint8_t* buf, size_t len, UdprosHeader* out) {
  if (len < kUdprosHeaderSize) return false;
  out->connection_id = get_u32(buf);
  out->op = buf[4];
  out->message_id = buf[5];
  out->block = get_u16(buf + 6);
  return true;
}

void udpros_send_stream(int fd, const sockaddr* dst, socklen_t dlen, uint32_t connection_id,
                        uint8_t message_id, const uint8_t* stream, uint32_t stream_len,
                        uint32_t max_datagram_size) {
  if (max_datagram_size <= kUdprosHeaderSize) max_datagram_size = kUdprosDefaultMaxDatagram;
  uint32_t max_payload = max_datagram_size - kUdprosHeaderSize;
  uint32_t total_blocks = stream_len == 0 ? 1 : (stream_len + max_payload - 1) / max_payload;

  std::vector<uint8_t> dgram(kUdprosHeaderSize + max_payload);
  uint32_t offset = 0;
  for (uint32_t block = 0; block < total_blocks; ++block) {
    uint32_t chunk = stream_len - offset;
    if (chunk > max_payload) chunk = max_payload;

    UdprosHeader h;
    h.connection_id = connection_id;
    h.message_id = message_id;
    if (block == 0) {
      h.op = kUdpOpData0;
      h.block = static_cast<uint16_t>(total_blocks);  // DATA0: total block count
    } else {
      h.op = kUdpOpDatan;
      h.block = static_cast<uint16_t>(block);          // DATAN: 1-based index
    }
    h.encode(dgram.data());
    if (chunk) std::memcpy(dgram.data() + kUdprosHeaderSize, stream + offset, chunk);
    noros::net_sendto(fd, dgram.data(), kUdprosHeaderSize + chunk, MSG_NOSIGNAL, dst, dlen);
    offset += chunk;
  }
}

bool UdprosReceiver::complete(std::vector<uint8_t>* out_body) {
  in_progress_ = false;
  // buf_ = [4B LE ros-len][body]; strip the prefix and hand back the body.
  if (buf_.size() < 4) return false;
  uint32_t rlen = get_u32(buf_.data());
  if (rlen + 4u != buf_.size()) return false;  // inconsistent; drop
  out_body->assign(buf_.begin() + 4, buf_.end());
  return true;
}

bool UdprosReceiver::feed(const uint8_t* datagram, size_t len, std::vector<uint8_t>* out_body) {
  UdprosHeader h;
  if (!UdprosHeader::decode(datagram, len, &h)) return false;
  const uint8_t* payload = datagram + kUdprosHeaderSize;
  size_t payload_len = len - kUdprosHeaderSize;

  if (h.op == kUdpOpData0) {
    in_progress_ = true;
    message_id_ = h.message_id;
    total_blocks_ = h.block == 0 ? 1 : h.block;
    next_block_ = 1;
    buf_.assign(payload, payload + payload_len);
    if (total_blocks_ <= 1) return complete(out_body);
    return false;
  }

  if (h.op == kUdpOpDatan) {
    // Require an in-progress message, matching id, and the contiguous next block.
    if (!in_progress_ || h.message_id != message_id_ || h.block != next_block_) {
      in_progress_ = false;  // out of order / lost fragment -> drop the message
      return false;
    }
    buf_.insert(buf_.end(), payload, payload + payload_len);
    next_block_++;
    if (next_block_ >= total_blocks_) return complete(out_body);
    return false;
  }

  // PING / ERR / unknown: ignore.
  return false;
}

}  // namespace noros
