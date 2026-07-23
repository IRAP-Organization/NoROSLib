// bag.hpp -- read and write ROS bag files (format v2.0), no ROS installed.
//
// A bag is ROS's on-disk container for recorded messages. This implements the
// real, documented v2.0 layout (http://wiki.ros.org/Bags/Format/2.0), so a bag
// written here opens in genuine `rosbag info` / `rosbag play` / `rqt_bag`, and a
// bag recorded by real ROS reads back here. Only the uncompressed encoding is
// written; a bz2-compressed bag written elsewhere still reads.
#pragma once
#include <cstdint>
#include <fstream>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace irap_noroslib {

// Record op codes (the 'op' header field, one byte).
enum BagOp {
  BAG_OP_MSG_DATA = 0x02,
  BAG_OP_BAG_HEADER = 0x03,
  BAG_OP_IDX_DATA = 0x04,
  BAG_OP_CHUNK = 0x05,
  BAG_OP_CHUNK_INFO = 0x06,
  BAG_OP_CONNECTION = 0x07,
};

struct BagConnection {
  int id = 0;
  std::string topic, type, md5, definition, callerid;
  uint64_t count = 0;
};

struct BagMessage {
  std::string topic;
  int conn = 0;
  uint32_t secs = 0, nsecs = 0;
  std::vector<uint8_t> data;
  double t() const { return secs + nsecs * 1e-9; }
};

// -- writer ------------------------------------------------------------------
class BagWriter {
 public:
  explicit BagWriter(const std::string& path);
  ~BagWriter();

  // Get (or create) the connection for a topic; returns its id.
  int connection(const std::string& topic, const std::string& type,
                 const std::string& md5, const std::string& definition,
                 const std::string& callerid = "");
  void write(int conn, uint32_t secs, uint32_t nsecs, const std::vector<uint8_t>& body);
  void close();

 private:
  struct BufMsg { int conn; uint32_t secs, nsecs; std::vector<uint8_t> body; };
  void write_record(const std::vector<std::pair<std::string, std::string>>& fields,
                    const std::string& data);
  std::string bag_header_record(uint64_t index_pos, uint32_t conn_count,
                                uint32_t chunk_count);
  void flush_chunk();

  std::ofstream f_;
  std::string path_;
  std::streampos bagheader_pos_;
  std::map<std::string, BagConnection> conns_;   // topic -> conn
  std::map<int, BagConnection*> by_id_;
  std::vector<BufMsg> chunk_;
  size_t chunk_bytes_ = 0;
  // (chunk_pos, start_s, start_n, end_s, end_n, {conn:count})
  struct ChunkInfo { uint64_t pos; uint32_t ss, sn, es, en; std::map<int, uint32_t> counts; };
  std::vector<ChunkInfo> chunk_infos_;
  bool closed_ = false;
};

// -- reader ------------------------------------------------------------------
class BagReader {
 public:
  explicit BagReader(const std::string& path);

  // Iterate every message in file order; cb gets the message and its connection.
  void read_messages(const std::function<void(const BagMessage&,
                                              const BagConnection&)>& cb);

  struct Info {
    std::string path;
    double start = 0, end = 0;
    uint64_t size = 0, messages = 0;
    int chunks = 0;
    std::map<int, BagConnection> conns;   // id -> connection
    std::map<int, uint64_t> counts;       // id -> message count
  };
  Info info();

 private:
  bool read_record(std::map<std::string, std::string>* header, std::string* data);
  void parse_connection(const std::map<std::string, std::string>& header,
                        const std::string& data);
  void read_chunk(const std::map<std::string, std::string>& header,
                  const std::string& data,
                  const std::function<void(const BagMessage&, const BagConnection&)>& cb);
  BagMessage msg_from(const std::map<std::string, std::string>& header,
                      const std::string& data);

  std::ifstream f_;
  std::string path_;
  std::map<int, BagConnection> conns_;
  uint64_t index_pos_ = 0;
  uint32_t conn_count_ = 0, chunk_count_ = 0;
  std::streampos data_start_;
};

}  // namespace irap_noroslib
