// XML-RPC client for talking to the ROS master and to publisher slave APIs.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "irap_noroslib/xmlrpc.hpp"

namespace irap_noroslib {

// Perform one XML-RPC call to the given "http://host:port/" URI. Returns true
// on transport success and fills *result with the returned value.
bool xmlrpc_call(const std::string& uri, const std::string& method,
                 const std::vector<XmlValue>& params, XmlValue* result,
                 std::string* err);

// --- ROS master API wrappers -------------------------------------------------

// getTopicTypes -> [(topic, type), ...]. Returns false on error.
bool get_topic_types(const std::string& master_uri, const std::string& caller_id,
                     std::vector<std::pair<std::string, std::string>>* topics,
                     std::string* err);

// getPid -> the master's process id. A cheap liveness ping: it succeeds only
// while the master is reachable, so it is how a node notices the master going
// away and coming back. Returns false (with *err) if the master is unreachable.
bool master_get_pid(const std::string& master_uri, const std::string& caller_id,
                    int* pid, std::string* err);

// getSystemState -> publishers / subscribers, each [(name, [node, ...]), ...].
using GraphMap = std::vector<std::pair<std::string, std::vector<std::string>>>;
bool get_system_state(const std::string& master_uri, const std::string& caller_id,
                      GraphMap* publishers, GraphMap* subscribers,
                      GraphMap* services, std::string* err);

// registerSubscriber -> list of current publisher URIs. Returns false on error.
bool register_subscriber(const std::string& master_uri, const std::string& caller_id,
                         const std::string& topic, const std::string& type,
                         const std::string& caller_api,
                         std::vector<std::string>* publisher_uris, std::string* err);

bool unregister_subscriber(const std::string& master_uri, const std::string& caller_id,
                           const std::string& topic, const std::string& caller_api,
                           std::string* err);

// registerPublisher -> list of current subscriber URIs (usually ignored).
bool register_publisher(const std::string& master_uri, const std::string& caller_id,
                        const std::string& topic, const std::string& type,
                        const std::string& caller_api,
                        std::vector<std::string>* subscriber_uris, std::string* err);

bool unregister_publisher(const std::string& master_uri, const std::string& caller_id,
                          const std::string& topic, const std::string& caller_api,
                          std::string* err);

// --- Parameter server master API wrappers ------------------------------------
bool get_param(const std::string& master_uri, const std::string& caller_id,
               const std::string& key, XmlValue* out, std::string* err);
bool set_param(const std::string& master_uri, const std::string& caller_id,
               const std::string& key, const XmlValue& value, std::string* err);
bool has_param(const std::string& master_uri, const std::string& caller_id,
               const std::string& key, bool* out, std::string* err);
bool delete_param(const std::string& master_uri, const std::string& caller_id,
                  const std::string& key, std::string* err);

// --- Service master API wrappers ---------------------------------------------

// registerService(caller_id, service, service_api, caller_api). service_api is
// the "rosrpc://host:port" URI of our service TCP endpoint.
bool register_service(const std::string& master_uri, const std::string& caller_id,
                      const std::string& service, const std::string& service_api,
                      const std::string& caller_api, std::string* err);

bool unregister_service(const std::string& master_uri, const std::string& caller_id,
                        const std::string& service, const std::string& service_api,
                        std::string* err);

// lookupService -> the provider's "rosrpc://host:port" URI. Returns false (with
// *err set) if no provider is registered.
bool lookup_service(const std::string& master_uri, const std::string& caller_id,
                    const std::string& service, std::string* service_url, std::string* err);

// --- Publisher slave API wrapper ---------------------------------------------

// requestTopic(caller_id, topic, [['TCPROS']]) -> TCPROS host+port.
bool request_topic_tcpros(const std::string& publisher_uri, const std::string& caller_id,
                          const std::string& topic, std::string* host, uint16_t* port,
                          std::string* err);

// requestTopic with UDPROS. `sub_header` is the serialized subscriber connection
// header; recv_host/recv_port is OUR UDP endpoint the publisher should send to.
// On success fills the publisher's UDP endpoint, the publisher-assigned conn_id,
// the negotiated max datagram size, and the publisher's serialized header bytes
// (from which the caller reads the real md5/type). Returns false on error; *err
// carries the publisher's message (which, on md5 mismatch, names the real md5).
bool request_topic_udpros(const std::string& publisher_uri, const std::string& caller_id,
                          const std::string& topic, const std::string& sub_header,
                          const std::string& recv_host, uint16_t recv_port, uint32_t max_datagram,
                          std::string* pub_host, uint16_t* pub_port, uint32_t* conn_id,
                          uint32_t* neg_max_datagram, std::string* pub_header, std::string* err);

}  // namespace irap_noroslib
