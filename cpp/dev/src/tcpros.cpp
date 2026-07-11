#include "noros/tcpros.hpp"

#include <cstring>

#include "noros/net_util.hpp"

namespace noros {
namespace {

void put_u32_le(std::vector<uint8_t>* v, uint32_t x) {
  v->push_back(static_cast<uint8_t>(x));
  v->push_back(static_cast<uint8_t>(x >> 8));
  v->push_back(static_cast<uint8_t>(x >> 16));
  v->push_back(static_cast<uint8_t>(x >> 24));
}

uint32_t get_u32_le(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
         (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

}  // namespace

std::vector<uint8_t> encode_tcpros_header_fields(const TcprosHeader& fields) {
  std::vector<uint8_t> body;
  for (const auto& kv : fields) {
    std::string field = kv.first + "=" + kv.second;
    put_u32_le(&body, static_cast<uint32_t>(field.size()));
    body.insert(body.end(), field.begin(), field.end());
  }
  return body;
}

std::vector<uint8_t> encode_tcpros_header(const TcprosHeader& fields) {
  std::vector<uint8_t> body = encode_tcpros_header_fields(fields);
  std::vector<uint8_t> out;
  put_u32_le(&out, static_cast<uint32_t>(body.size()));
  out.insert(out.end(), body.begin(), body.end());
  return out;
}

bool write_tcpros_header(int fd, const TcprosHeader& fields) {
  std::vector<uint8_t> buf = encode_tcpros_header(fields);
  return write_n(fd, buf.data(), buf.size());
}

// Parse the concatenated [4B field len]["key=value"] pairs from a body buffer.
static bool parse_header_fields(const uint8_t* body, size_t n, TcprosHeader* out) {
  size_t pos = 0;
  out->clear();
  while (pos + 4 <= n) {
    uint32_t flen = get_u32_le(body + pos);
    pos += 4;
    if (pos + flen > n) return false;
    std::string field(reinterpret_cast<const char*>(body + pos), flen);
    pos += flen;
    size_t eq = field.find('=');
    if (eq == std::string::npos) continue;
    (*out)[field.substr(0, eq)] = field.substr(eq + 1);
  }
  return true;
}

bool decode_tcpros_header(const uint8_t* buf, size_t len, TcprosHeader* out) {
  if (len < 4) return false;
  uint32_t total = get_u32_le(buf);
  if (total > len - 4) return false;
  return parse_header_fields(buf + 4, total, out);
}

bool decode_tcpros_header_fields(const uint8_t* buf, size_t len, TcprosHeader* out) {
  return parse_header_fields(buf, len, out);
}

bool read_tcpros_header(int fd, TcprosHeader* out) {
  uint8_t lenbuf[4];
  if (!read_n(fd, lenbuf, 4)) return false;
  uint32_t total = get_u32_le(lenbuf);
  if (total > 16u * 1024 * 1024) return false;  // sanity cap
  std::vector<uint8_t> body(total);
  if (total && !read_n(fd, body.data(), total)) return false;
  return parse_header_fields(body.data(), body.size(), out);
}

TcprosHeader make_subscriber_header(const std::string& caller_id, const std::string& topic,
                                    const std::string& md5sum, const std::string& type) {
  TcprosHeader h;
  h["callerid"] = caller_id;
  h["topic"] = topic;
  h["md5sum"] = md5sum;
  h["type"] = type;
  h["tcp_nodelay"] = "1";
  h["message_definition"] = "";  // empty is fine (required only for full tooling)
  return h;
}

TcprosHeader make_publisher_header(const std::string& caller_id, const std::string& topic,
                                   const std::string& md5sum, const std::string& type,
                                   const std::string& message_definition) {
  TcprosHeader h;
  h["callerid"] = caller_id;
  h["topic"] = topic;
  h["md5sum"] = md5sum;
  h["type"] = type;
  h["message_definition"] = message_definition;
  h["latching"] = "0";
  return h;
}

bool parse_type_md5_from_error(const std::string& error, std::string* type, std::string* md5) {
  // Locate the "our version has [ ... ]" bracket; fall back to the last
  // bracketed "pkg/Type/md5" token if the exact phrase isn't present.
  size_t open = std::string::npos;
  size_t phrase = error.find("our version has [");
  if (phrase != std::string::npos) {
    open = error.find('[', phrase);
  } else {
    open = error.rfind('[');  // last bracket = publisher's own datatype
  }
  if (open == std::string::npos) return false;
  size_t close = error.find(']', open);
  if (close == std::string::npos) return false;

  std::string token = error.substr(open + 1, close - open - 1);  // pkg/Type/md5
  size_t slash = token.rfind('/');
  if (slash == std::string::npos) return false;
  std::string t = token.substr(0, slash);
  std::string m = token.substr(slash + 1);
  // An md5 is 32 hex chars; require that to avoid mis-parsing.
  if (m.size() != 32) return false;
  *type = t;
  *md5 = m;
  return true;
}

}  // namespace noros
