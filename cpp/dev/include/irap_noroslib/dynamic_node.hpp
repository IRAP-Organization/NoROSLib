// dynamic_node.hpp -- publish/subscribe/serve with types loaded at RUNTIME.
//
// Publisher<T>/Subscriber<T> read T::TYPE, T::MD5 and T::DEFINITION *statically*,
// so a type that only exists at runtime cannot use them. These are the same
// classes with those three constants replaced by a MsgType. They sit straight on
// top of the already type-erased detail:: layer -- the transport itself does not
// know or care which kind of message it is carrying, so nothing below changes.
//
//   MsgType T = load_msg_file("/home/me/msgs/Pose2D.msg", "my_pkg");
//   DynamicPublisher pub("/pose", T);
//   DynamicMessage m = T.create();
//   m.set("x", 1.0);
//   pub.publish(m);
//
//   DynamicSubscriber sub("/pose", T, [](const DynamicMessage& m) {
//     loginfo("x = " + std::to_string(m.get<double>("x")));
//   });
#pragma once

#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "irap_noroslib/msgfile.hpp"
#include "irap_noroslib/node.hpp"

namespace irap_noroslib {

class DynamicPublisher {
 public:
  DynamicPublisher(const std::string& topic, const MsgType& type, bool latch = false)
      : type_(type),
        pub_(detail::advertise(topic, type.type(), type.md5(), type.definition(), latch)),
        topic_(topic) {}

  void publish(const DynamicMessage& m) {
    if (m.type() != type_.type())
      throw std::runtime_error("irap_noroslib: cannot publish a " + m.type() +
                               " on " + topic_ + " (a " + type_.type() + " topic)");
    detail::publish_raw(*pub_, m.serialize());
  }

  /// A blank message of this topic's type, ready to fill in.
  DynamicMessage create() const { return type_.create(); }
  const MsgType& type() const { return type_; }
  int get_num_connections() const { return detail::num_connections(*pub_); }
  void shutdown() { detail::unadvertise(pub_); }

 private:
  MsgType type_;
  std::shared_ptr<detail::Publication> pub_;
  std::string topic_;
};

class DynamicSubscriber {
 public:
  using Callback = std::function<void(const DynamicMessage&)>;

  DynamicSubscriber(const std::string& topic, const MsgType& type, Callback cb,
                    const std::string& transport = "tcpros") {
    MsgType t = type;      // keeps the spec alive for the connection's lifetime
    sub_ = detail::subscribe(
        topic, type.type(), type.md5(),
        [t, cb](const std::vector<uint8_t>& body) {
          try {
            cb(t.decode(body));
          } catch (const std::exception& e) {
            logwarn(std::string("deserialize failed: ") + e.what());
          }
        },
        transport);
  }

  void shutdown() { detail::unsubscribe(sub_); }

 private:
  std::shared_ptr<detail::Subscription> sub_;
};

class DynamicServiceServer {
 public:
  /// Return false from the handler to signal failure to the caller.
  using Handler = std::function<bool(const DynamicMessage& req, DynamicMessage& resp)>;

  DynamicServiceServer(const std::string& name, const SrvType& srv, Handler h) {
    SrvType s = srv;
    srv_ = detail::advertise_service(
        name, srv.type(), srv.md5(), srv.request().type(), srv.response().type(),
        [s, h](const std::vector<uint8_t>& req_bytes, std::vector<uint8_t>& resp_bytes) {
          try {
            DynamicMessage resp = s.response().create();
            if (!h(s.request().decode(req_bytes), resp)) return false;
            resp_bytes = resp.serialize();
            return true;
          } catch (const std::exception& e) {
            logwarn(std::string("service handler failed: ") + e.what());
            return false;
          }
        });
  }

  void shutdown() { detail::unadvertise_service(srv_); }

 private:
  std::shared_ptr<detail::ServiceServer> srv_;
};

class DynamicServiceClient {
 public:
  DynamicServiceClient(const std::string& name, const SrvType& srv)
      : name_(name), srv_(srv) {}

  /// A blank request, ready to fill in.
  DynamicMessage request() const { return srv_.request().create(); }

  bool call(const DynamicMessage& req, DynamicMessage& resp) {
    std::vector<uint8_t> out;
    std::string err;
    if (!detail::call_service(name_, srv_.md5(), req.serialize(), &out, &err)) {
      logwarn("service call " + name_ + " failed: " + err);
      return false;
    }
    resp = srv_.response().decode(out);
    return true;
  }

  bool waitForExistence(double timeout_s = -1) {
    return detail::wait_for_service(name_, timeout_s);
  }

 private:
  std::string name_;
  SrvType srv_;
};

}  // namespace irap_noroslib
