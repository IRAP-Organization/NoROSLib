// node.hpp — roscpp-style front end for irap_noroslib: init_node, Publisher<Msg>,
// Subscriber<Msg>, Rate, spin. Talks to a real roscore + real ROS nodes over
// XML-RPC + TCPROS, with automatic md5 discovery. No ROS libraries linked.
//
//   #include "irap_noroslib/irap_noroslib.hpp"
//   irap_noroslib::init_node("talker");
//   irap_noroslib::Publisher<std_msgs::String> pub("/chatter");
//   irap_noroslib::Rate rate(10);
//   while (irap_noroslib::ok()) { std_msgs::String m; m.data = "hi"; pub.publish(m); rate.sleep(); }
#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace irap_noroslib {

// Configure the master URI and advertised hostname BEFORE init_node, without
// environment variables. An explicit init_node argument still takes precedence.
//   master_uri precedence: init_node arg > set_master_uri > $ROS_MASTER_URI > default
//   host       precedence: init_node arg > set_hostname   > $ROS_IP > $ROS_HOSTNAME > auto
void set_master_uri(const std::string& uri);
void set_hostname(const std::string& host);
void configure(const std::string& master_uri, const std::string& host);

// Register this process as a ROS node with the master. `master_uri` and `host`
// override the configured/env values (see precedence above). Call once before
// creating Publisher/Subscriber.
void init_node(const std::string& name, const std::string& master_uri = "",
               const std::string& host = "");
bool ok();                       // false after shutdown / SIGINT
void spin();                     // block until shutdown
void shutdown(const std::string& reason = "");
void loginfo(const std::string& msg);
void logwarn(const std::string& msg);
void logerr(const std::string& msg);
/// Silence the library's own logging: "debug", "info" (default), "warn", "error",
/// "none". Handy for a CLI, where only the program's real output should show.
void set_log_level(const std::string& level);

// Like ros::Rate.
class Rate {
 public:
  explicit Rate(double hz)
      : period_(std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            std::chrono::duration<double>(hz > 0 ? 1.0 / hz : 0.0))),
        next_(std::chrono::steady_clock::now() + period_) {}
  void sleep() {
    auto now = std::chrono::steady_clock::now();
    if (now < next_) { std::this_thread::sleep_until(next_); next_ += period_; }
    else next_ = now + period_;
  }
 private:
  std::chrono::steady_clock::duration period_;
  std::chrono::steady_clock::time_point next_;
};

namespace detail {
class Publication;
class Subscription;
class ServiceServer;
using RawCallback = std::function<void(const std::vector<uint8_t>&)>;
// A raw service handler: given request bytes, fill response bytes; return false
// to signal failure to the caller.
using RawServiceHandler = std::function<bool(const std::vector<uint8_t>&, std::vector<uint8_t>&)>;

std::shared_ptr<Publication> advertise(const std::string& topic, const std::string& type,
                                       const std::string& md5, const std::string& definition,
                                       bool latch);
void publish_raw(Publication& pub, const std::vector<uint8_t>& body);
int num_connections(Publication& pub);
void unadvertise(const std::shared_ptr<Publication>& pub);

std::shared_ptr<Subscription> subscribe(const std::string& topic, const std::string& type,
                                        const std::string& md5, RawCallback cb,
                                        const std::string& transport = "tcpros");
void unsubscribe(const std::shared_ptr<Subscription>& sub);

// Subscribe to a topic of ANY type: connect with a wildcard md5 and let the
// publisher tell us what it is. The callback gets the type, md5 and the full
// message definition from the handshake, plus the raw body -- enough to decode a
// message we have never seen and have no .msg file for. See AnySubscriber.
using AnyCallback = std::function<void(const std::string& type, const std::string& md5,
                                       const std::string& definition,
                                       const std::vector<uint8_t>& body)>;
std::shared_ptr<Subscription> subscribe_any(const std::string& topic, AnyCallback cb);

std::shared_ptr<ServiceServer> advertise_service(const std::string& name, const std::string& type,
                                                 const std::string& md5,
                                                 const std::string& req_type,
                                                 const std::string& resp_type,
                                                 RawServiceHandler handler);
void unadvertise_service(const std::shared_ptr<ServiceServer>& srv);
bool call_service(const std::string& name, const std::string& md5,
                  const std::vector<uint8_t>& request, std::vector<uint8_t>* response,
                  std::string* err);
bool wait_for_service(const std::string& name, double timeout_s);
}  // namespace detail

// Poll the master until `name` is registered (timeout_s < 0 => forever).
inline bool wait_for_service(const std::string& name, double timeout_s = -1) {
  return detail::wait_for_service(name, timeout_s);
}

// --- parameter server ------------------------------------------------------
// get_param overloads return false if the parameter is missing or the type
// doesn't match. get_param_or returns `def` on any failure.
bool get_param(const std::string& key, int* out);
bool get_param(const std::string& key, double* out);
bool get_param(const std::string& key, bool* out);
bool get_param(const std::string& key, std::string* out);
void set_param(const std::string& key, int value);
void set_param(const std::string& key, double value);
void set_param(const std::string& key, bool value);
void set_param(const std::string& key, const std::string& value);
inline void set_param(const std::string& key, const char* value) {
  set_param(key, std::string(value));
}
bool has_param(const std::string& key);
bool delete_param(const std::string& key);

template <typename T>
T get_param_or(const std::string& key, T def) {
  T v = def;
  get_param(key, &v);
  return v;
}

// --- Publisher<Msg> --------------------------------------------------------
template <typename Msg>
class Publisher {
 public:
  explicit Publisher(const std::string& topic, bool latch = false)
      : pub_(detail::advertise(topic, Msg::TYPE, Msg::MD5, Msg::DEFINITION, latch)) {}
  void publish(const Msg& m) { detail::publish_raw(*pub_, m.serialize()); }
  int get_num_connections() const { return detail::num_connections(*pub_); }
  void shutdown() { detail::unadvertise(pub_); }

 private:
  std::shared_ptr<detail::Publication> pub_;
};

// --- Subscriber<Msg> -------------------------------------------------------
// callback receives a decoded Msg. If the publisher's md5 differs from Msg::MD5,
// irap_noroslib discovers the real md5 from the rejection error and reconnects.
// transport "tcpros" (default, reliable) or "udpros" (unreliable UDP).
template <typename Msg>
class Subscriber {
 public:
  using Callback = std::function<void(const Msg&)>;
  Subscriber(const std::string& topic, Callback cb, const std::string& transport = "tcpros") {
    sub_ = detail::subscribe(topic, Msg::TYPE, Msg::MD5,
        [cb](const std::vector<uint8_t>& body) {
          try { cb(Msg::deserialize(body)); }
          catch (const std::exception& e) { logwarn(std::string("deserialize failed: ") + e.what()); }
        }, transport);
  }
  void shutdown() { detail::unsubscribe(sub_); }

 private:
  std::shared_ptr<detail::Subscription> sub_;
};

// --- ServiceServer<Srv> ----------------------------------------------------
// A service type Srv provides: static TYPE/MD5, and nested Request/Response
// message structs (each a normal irap_noroslib message). The handler fills Response and
// returns true (false => report failure to the caller).
template <typename Srv>
class ServiceServer {
 public:
  using Handler = std::function<bool(const typename Srv::Request&, typename Srv::Response&)>;
  ServiceServer(const std::string& name, Handler h) {
    srv_ = detail::advertise_service(
        name, Srv::TYPE, Srv::MD5, Srv::Request::TYPE, Srv::Response::TYPE,
        [h](const std::vector<uint8_t>& req_bytes, std::vector<uint8_t>& resp_bytes) {
          typename Srv::Response resp;
          bool ok = h(Srv::Request::deserialize(req_bytes), resp);
          if (ok) resp_bytes = resp.serialize();
          return ok;
        });
  }
  void shutdown() { detail::unadvertise_service(srv_); }

 private:
  std::shared_ptr<detail::ServiceServer> srv_;
};

// --- ServiceClient<Srv> ----------------------------------------------------
template <typename Srv>
class ServiceClient {
 public:
  explicit ServiceClient(const std::string& name) : name_(name) {}

  // Look up + call the service. Returns true and fills `resp` on success.
  bool call(const typename Srv::Request& req, typename Srv::Response& resp) {
    std::vector<uint8_t> resp_bytes;
    std::string err;
    if (!detail::call_service(name_, Srv::MD5, req.serialize(), &resp_bytes, &err)) {
      logwarn("service call " + name_ + " failed: " + err);
      return false;
    }
    resp = Srv::Response::deserialize(resp_bytes);
    return true;
  }

  bool waitForExistence(double timeout_s = -1) { return detail::wait_for_service(name_, timeout_s); }
  const std::string& name() const { return name_; }

 private:
  std::string name_;
};

}  // namespace irap_noroslib
