// TCPROS transport helpers: connection-header encode/decode and message framing.
// See http://wiki.ros.org/ROS/TCPROS and ROS/Connection%20Header.
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace noros {

// A TCPROS connection header is a set of key=value fields.
using TcprosHeader = std::map<std::string, std::string>;

// Encode a connection header for the TCPROS wire handshake:
//   [4B LE total len] then repeated [4B LE field len]["key=value"]
std::vector<uint8_t> encode_tcpros_header(const TcprosHeader& fields);

// Encode ONLY the fields (repeated [4B LE field len]["key=value"]) with NO
// leading total-length prefix. This is what roscpp's Header::write produces and
// what the UDPROS requestTopic header blobs carry.
std::vector<uint8_t> encode_tcpros_header_fields(const TcprosHeader& fields);

// Decode a fields-only header blob (no leading total length) — the counterpart
// to encode_tcpros_header_fields, for UDPROS negotiation blobs.
bool decode_tcpros_header_fields(const uint8_t* buf, size_t len, TcprosHeader* out);

// Read + decode a connection header from a socket. Returns false on error.
bool read_tcpros_header(int fd, TcprosHeader* out);

// Decode a connection header already in memory. `buf` points at the 4-byte
// length prefix + fields (the full encode_tcpros_header output). Used for the
// header blobs carried inside UDPROS requestTopic negotiation.
bool decode_tcpros_header(const uint8_t* buf, size_t len, TcprosHeader* out);

// Write a connection header to a socket. Returns false on error.
bool write_tcpros_header(int fd, const TcprosHeader& fields);

// Build the header a SUBSCRIBER sends to a publisher.
TcprosHeader make_subscriber_header(const std::string& caller_id,
                                    const std::string& topic,
                                    const std::string& md5sum,
                                    const std::string& type);

// Build the header a PUBLISHER sends back to a subscriber.
TcprosHeader make_publisher_header(const std::string& caller_id,
                                   const std::string& topic,
                                   const std::string& md5sum,
                                   const std::string& type,
                                   const std::string& message_definition);

// When a publisher rejects a subscribe due to an md5 mismatch, its error text
// looks like:
//   "... but our version has [pkg/Type/41936b74e168ba754279ae683ce3f121].
//    Dropping connection."
// Extract the publisher's REAL datatype + md5 from that "our version has [...]"
// bracket. Returns true if found. `type` gets "pkg/Type", `md5` the hex sum.
bool parse_type_md5_from_error(const std::string& error, std::string* type, std::string* md5);

}  // namespace noros
