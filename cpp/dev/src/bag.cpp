// bag.cpp -- ROS bag v2.0 reader/writer implementation.
#include "irap_noroslib/bag.hpp"

#include <cstring>
#include <stdexcept>

namespace irap_noroslib {
namespace {

const size_t kChunkThreshold = 768 * 1024;   // rosbag's default chunk size

// -- little-endian pack helpers (bag integers are always LE) -----------------
void put_u8(std::string* s, uint8_t v) { s->push_back((char)v); }
void put_u32(std::string* s, uint32_t v) {
  for (int i = 0; i < 4; ++i) s->push_back((char)((v >> (8 * i)) & 0xff));
}
void put_u64(std::string* s, uint64_t v) {
  for (int i = 0; i < 8; ++i) s->push_back((char)((v >> (8 * i)) & 0xff));
}
std::string u8(uint8_t v) { std::string s; put_u8(&s, v); return s; }
std::string u32(uint32_t v) { std::string s; put_u32(&s, v); return s; }
std::string u64(uint64_t v) { std::string s; put_u64(&s, v); return s; }
std::string tm(uint32_t s_, uint32_t n_) { std::string s; put_u32(&s, s_); put_u32(&s, n_); return s; }

uint32_t get_u32(const std::string& s, size_t off) {
  uint32_t v = 0;
  for (int i = 0; i < 4; ++i) v |= (uint32_t)(uint8_t)s[off + i] << (8 * i);
  return v;
}
uint64_t get_u64(const std::string& s, size_t off) {
  uint64_t v = 0;
  for (int i = 0; i < 8; ++i) v |= (uint64_t)(uint8_t)s[off + i] << (8 * i);
  return v;
}

// A single header field: <4-byte len><name=value>.
std::string enc_field(const std::string& name, const std::string& value) {
  std::string field = name + "=" + value;
  return u32((uint32_t)field.size()) + field;
}

// The raw field sequence (no outer length) -- a connection record's DATA.
std::string encode_header_fields(
    const std::vector<std::pair<std::string, std::string>>& fields) {
  std::string out;
  for (const auto& kv : fields) out += enc_field(kv.first, kv.second);
  return out;
}

// A length-prefixed header block: <4-byte len><fields> -- a record HEADER.
std::string encode_header(
    const std::vector<std::pair<std::string, std::string>>& fields) {
  std::string body = encode_header_fields(fields);
  return u32((uint32_t)body.size()) + body;
}

std::map<std::string, std::string> decode_header_fields(const std::string& blob) {
  std::map<std::string, std::string> out;
  size_t i = 0;
  while (i + 4 <= blob.size()) {
    uint32_t flen = get_u32(blob, i);
    i += 4;
    if (i + flen > blob.size()) break;
    std::string field = blob.substr(i, flen);
    i += flen;
    size_t eq = field.find('=');
    if (eq != std::string::npos) out[field.substr(0, eq)] = field.substr(eq + 1);
  }
  return out;
}

std::string conn_header_data(const BagConnection& c) {
  std::vector<std::pair<std::string, std::string>> f = {
      {"topic", c.topic}, {"type", c.type}, {"md5sum", c.md5},
      {"message_definition", c.definition}};
  if (!c.callerid.empty()) f.push_back({"callerid", c.callerid});
  return encode_header_fields(f);
}

std::string record_bytes(const std::vector<std::pair<std::string, std::string>>& fields,
                         const std::string& data) {
  return encode_header(fields) + u32((uint32_t)data.size()) + data;
}

}  // namespace

// ---------------------------------------------------------------- writer ----
BagWriter::BagWriter(const std::string& path) : path_(path) {
  f_.open(path, std::ios::binary | std::ios::out | std::ios::trunc);
  if (!f_) throw std::runtime_error("nr_rosbag: cannot open " + path + " for writing");
  f_.write("#ROSBAG V2.0\n", 13);
  bagheader_pos_ = f_.tellp();
  std::string hdr = bag_header_record(0, 0, 0);
  f_.write(hdr.data(), (std::streamsize)hdr.size());
}

BagWriter::~BagWriter() { close(); }

int BagWriter::connection(const std::string& topic, const std::string& type,
                          const std::string& md5, const std::string& definition,
                          const std::string& callerid) {
  auto it = conns_.find(topic);
  if (it != conns_.end()) return it->second.id;
  BagConnection c;
  c.id = (int)conns_.size();
  c.topic = topic; c.type = type; c.md5 = md5;
  c.definition = definition; c.callerid = callerid;
  auto& stored = conns_[topic] = c;
  by_id_[stored.id] = &stored;
  return stored.id;
}

void BagWriter::write(int conn, uint32_t secs, uint32_t nsecs,
                      const std::vector<uint8_t>& body) {
  chunk_.push_back({conn, secs, nsecs, body});
  chunk_bytes_ += body.size();
  if (by_id_.count(conn)) by_id_[conn]->count++;
  if (chunk_bytes_ >= kChunkThreshold) flush_chunk();
}

void BagWriter::write_record(
    const std::vector<std::pair<std::string, std::string>>& fields,
    const std::string& data) {
  std::string rec = record_bytes(fields, data);
  f_.write(rec.data(), (std::streamsize)rec.size());
}

std::string BagWriter::bag_header_record(uint64_t index_pos, uint32_t conn_count,
                                         uint32_t chunk_count) {
  std::string header = encode_header({
      {"op", u8(BAG_OP_BAG_HEADER)}, {"index_pos", u64(index_pos)},
      {"conn_count", u32(conn_count)}, {"chunk_count", u32(chunk_count)}});
  int pad = 4096 - (int)header.size() - 4;
  if (pad < 0) pad = 0;
  return header + u32((uint32_t)pad) + std::string(pad, ' ');
}

void BagWriter::flush_chunk() {
  if (chunk_.empty()) return;
  uint64_t chunk_pos = (uint64_t)f_.tellp();
  std::string data;
  std::map<int, bool> seen;
  std::map<int, std::vector<std::pair<std::pair<uint32_t, uint32_t>, uint32_t>>> index;
  std::map<int, uint32_t> counts;
  uint32_t ss = 0, sn = 0, es = 0, en = 0;
  bool first = true;
  for (const auto& m : chunk_) {
    if (!seen[m.conn]) {
      seen[m.conn] = true;
      const BagConnection& c = *by_id_[m.conn];
      data += record_bytes({{"op", u8(BAG_OP_CONNECTION)}, {"conn", u32(m.conn)},
                            {"topic", c.topic}}, conn_header_data(c));
    }
    uint32_t offset = (uint32_t)data.size();
    std::string body((const char*)m.body.data(), m.body.size());
    data += record_bytes({{"op", u8(BAG_OP_MSG_DATA)}, {"conn", u32(m.conn)},
                          {"time", tm(m.secs, m.nsecs)}}, body);
    index[m.conn].push_back({{m.secs, m.nsecs}, offset});
    counts[m.conn]++;
    if (first || m.secs < ss || (m.secs == ss && m.nsecs < sn)) { ss = m.secs; sn = m.nsecs; }
    if (first || m.secs > es || (m.secs == es && m.nsecs > en)) { es = m.secs; en = m.nsecs; }
    first = false;
  }
  write_record({{"op", u8(BAG_OP_CHUNK)}, {"compression", "none"},
                {"size", u32((uint32_t)data.size())}}, data);
  for (const auto& kv : index) {
    std::string idx;
    for (const auto& e : kv.second) idx += tm(e.first.first, e.first.second) + u32(e.second);
    write_record({{"op", u8(BAG_OP_IDX_DATA)}, {"ver", u32(1)},
                  {"conn", u32(kv.first)}, {"count", u32((uint32_t)kv.second.size())}}, idx);
  }
  chunk_infos_.push_back({chunk_pos, ss, sn, es, en, counts});
  chunk_.clear();
  chunk_bytes_ = 0;
}

void BagWriter::close() {
  if (closed_) return;
  closed_ = true;
  flush_chunk();
  uint64_t index_pos = (uint64_t)f_.tellp();
  for (const auto& kv : conns_) {
    const BagConnection& c = kv.second;
    write_record({{"op", u8(BAG_OP_CONNECTION)}, {"conn", u32(c.id)},
                  {"topic", c.topic}}, conn_header_data(c));
  }
  for (const auto& ci : chunk_infos_) {
    std::string data;
    for (const auto& cc : ci.counts) { data += u32(cc.first); data += u32(cc.second); }
    write_record({{"op", u8(BAG_OP_CHUNK_INFO)}, {"ver", u32(1)},
                  {"chunk_pos", u64(ci.pos)}, {"start_time", tm(ci.ss, ci.sn)},
                  {"end_time", tm(ci.es, ci.en)},
                  {"count", u32((uint32_t)ci.counts.size())}}, data);
  }
  f_.seekp(bagheader_pos_);
  std::string hdr = bag_header_record(index_pos, (uint32_t)conns_.size(),
                                      (uint32_t)chunk_infos_.size());
  f_.write(hdr.data(), (std::streamsize)hdr.size());
  f_.close();
}

// ---------------------------------------------------------------- reader ----
BagReader::BagReader(const std::string& path) : path_(path) {
  f_.open(path, std::ios::binary | std::ios::in);
  if (!f_) throw std::runtime_error("nr_rosbag: cannot open " + path);
  std::string line;
  std::getline(f_, line);
  if (line.rfind("#ROSBAG V2.0", 0) != 0)
    throw std::runtime_error(path + " is not a ROS bag v2.0 file");
  std::streampos pos = f_.tellg();
  std::map<std::string, std::string> header;
  std::string data;
  read_record(&header, &data);
  conn_count_ = get_u32(header["conn_count"], 0);
  chunk_count_ = get_u32(header["chunk_count"], 0);
  index_pos_ = get_u64(header["index_pos"], 0);
  data_start_ = f_.tellg();
  f_.seekg(pos);
}

bool BagReader::read_record(std::map<std::string, std::string>* header, std::string* data) {
  char lenbuf[4];
  f_.read(lenbuf, 4);
  if (f_.gcount() < 4) return false;
  uint32_t hlen = 0;
  for (int i = 0; i < 4; ++i) hlen |= (uint32_t)(uint8_t)lenbuf[i] << (8 * i);
  std::string hbytes(hlen, '\0');
  f_.read(&hbytes[0], hlen);
  *header = decode_header_fields(hbytes);
  f_.read(lenbuf, 4);
  if (f_.gcount() < 4) return false;
  uint32_t dlen = 0;
  for (int i = 0; i < 4; ++i) dlen |= (uint32_t)(uint8_t)lenbuf[i] << (8 * i);
  data->assign(dlen, '\0');
  f_.read(&(*data)[0], dlen);
  return true;
}

void BagReader::parse_connection(const std::map<std::string, std::string>& header,
                                 const std::string& data) {
  int cid = (int)get_u32(header.at("conn"), 0);
  std::map<std::string, std::string> f = decode_header_fields(data);
  BagConnection c;
  c.id = cid;
  auto it = header.find("topic");
  c.topic = it != header.end() ? it->second : f["topic"];
  c.type = f["type"];
  c.md5 = f["md5sum"];
  c.definition = f["message_definition"];
  c.callerid = f["callerid"];
  conns_[cid] = c;
}

BagMessage BagReader::msg_from(const std::map<std::string, std::string>& header,
                               const std::string& data) {
  BagMessage m;
  m.conn = (int)get_u32(header.at("conn"), 0);
  const std::string& t = header.at("time");
  m.secs = get_u32(t, 0);
  m.nsecs = get_u32(t, 4);
  auto it = conns_.find(m.conn);
  m.topic = it != conns_.end() ? it->second.topic : "?";
  m.data.assign(data.begin(), data.end());
  return m;
}

void BagReader::read_chunk(
    const std::map<std::string, std::string>& header, const std::string& data_in,
    const std::function<void(const BagMessage&, const BagConnection&)>& cb) {
  auto it = header.find("compression");
  std::string comp = it != header.end() ? it->second : "none";
  if (comp != "none")
    throw std::runtime_error("nr_rosbag: this build reads only uncompressed bags "
                             "(chunk compression=" + comp + ")");
  const std::string& data = data_in;
  size_t i = 0;
  while (i + 4 <= data.size()) {
    uint32_t hlen = get_u32(data, i); i += 4;
    std::map<std::string, std::string> rhdr = decode_header_fields(data.substr(i, hlen));
    i += hlen;
    uint32_t dlen = get_u32(data, i); i += 4;
    std::string rdata = data.substr(i, dlen);
    i += dlen;
    uint8_t op = (uint8_t)rhdr["op"][0];
    if (op == BAG_OP_CONNECTION) {
      parse_connection(rhdr, rdata);
    } else if (op == BAG_OP_MSG_DATA) {
      BagMessage m = msg_from(rhdr, rdata);
      cb(m, conns_[m.conn]);
    }
  }
}

void BagReader::read_messages(
    const std::function<void(const BagMessage&, const BagConnection&)>& cb) {
  f_.clear();
  f_.seekg(data_start_);
  while (true) {
    std::map<std::string, std::string> header;
    std::string data;
    if (!read_record(&header, &data)) break;
    if (header.find("op") == header.end()) break;
    uint8_t op = (uint8_t)header["op"][0];
    if (op == BAG_OP_CONNECTION) parse_connection(header, data);
    else if (op == BAG_OP_CHUNK) read_chunk(header, data, cb);
    else if (op == BAG_OP_MSG_DATA) { BagMessage m = msg_from(header, data); cb(m, conns_[m.conn]); }
  }
}

BagReader::Info BagReader::info() {
  Info out;
  out.path = path_;
  // connections (file-level) + chunk infos, from the index tail.
  f_.clear();
  f_.seekg((std::streamoff)index_pos_);
  for (uint32_t k = 0; k < conn_count_; ++k) {
    std::map<std::string, std::string> header;
    std::string data;
    if (!read_record(&header, &data)) break;
    if ((uint8_t)header["op"][0] == BAG_OP_CONNECTION) parse_connection(header, data);
  }
  bool have_time = false;
  f_.clear();
  f_.seekg((std::streamoff)index_pos_);
  while (true) {
    std::map<std::string, std::string> header;
    std::string data;
    if (!read_record(&header, &data)) break;
    if (header.find("op") == header.end()) break;
    if ((uint8_t)header["op"][0] != BAG_OP_CHUNK_INFO) continue;
    out.chunks++;
    double s = get_u32(header["start_time"], 0) + get_u32(header["start_time"], 4) * 1e-9;
    double e = get_u32(header["end_time"], 0) + get_u32(header["end_time"], 4) * 1e-9;
    if (!have_time || s < out.start) out.start = s;
    if (!have_time || e > out.end) out.end = e;
    have_time = true;
    for (size_t j = 0; j + 8 <= data.size(); j += 8) {
      int cid = (int)get_u32(data, j);
      uint32_t n = get_u32(data, j + 4);
      out.counts[cid] += n;
      out.messages += n;
    }
  }
  out.conns = conns_;
  std::ifstream sz(path_, std::ios::binary | std::ios::ate);
  out.size = (uint64_t)sz.tellg();
  return out;
}

}  // namespace irap_noroslib
