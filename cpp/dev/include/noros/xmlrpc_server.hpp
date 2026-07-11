// The bridge's own XML-RPC "slave" server. Real ROS nodes and the master call
// into this so the bridge looks like a legitimate node.
#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>
#include <vector>

#include "noros/xmlrpc.hpp"

namespace noros {

class SlaveServer {
 public:
  // Called when the master reports the publisher set for a subscribed topic
  // changed (publisherUpdate). `publishers` is the full current URI list.
  using PublisherUpdateFn =
      std::function<void(const std::string& topic, const std::vector<std::string>& publishers)>;

  // Called when a subscriber asks us (a fake publisher) for a topic connection
  // (requestTopic). `protocols` is the caller's protocol-preference list (each
  // element an array like ['TCPROS'] or ['UDPROS', hdr, host, port, max]).
  // Return the chosen protocol-params array (e.g. ['TCPROS', host, port] or the
  // 6-element UDPROS response), or an empty array if we can't serve the topic.
  using RequestTopicFn =
      std::function<XmlValue(const std::string& topic, const std::vector<XmlValue>& protocols)>;

  SlaveServer(std::string node_name, std::string master_uri);
  ~SlaveServer();

  void set_publisher_update(PublisherUpdateFn fn) { on_publisher_update_ = std::move(fn); }
  void set_request_topic(RequestTopicFn fn) { on_request_topic_ = std::move(fn); }

  // Bind + start the accept loop. bind_ip/port may be "0.0.0.0"/0.
  // Returns false on bind failure. The bound port is available via port().
  bool start(const std::string& bind_ip, uint16_t port);
  void stop();

  uint16_t port() const { return port_; }
  // The URI other nodes use to reach us, e.g. "http://<advertised>:<port>/".
  std::string caller_api() const;
  void set_advertised_host(std::string h) { advertised_host_ = std::move(h); }

 private:
  void run();
  void handle_connection(int fd);
  std::string dispatch(const std::string& method, const std::vector<class XmlValue>& params);

  std::string node_name_;
  std::string master_uri_;
  std::string advertised_host_;
  int listen_fd_ = -1;
  uint16_t port_ = 0;
  std::atomic<bool> running_{false};
  std::thread thread_;
  PublisherUpdateFn on_publisher_update_;
  RequestTopicFn on_request_topic_;
};

}  // namespace noros
