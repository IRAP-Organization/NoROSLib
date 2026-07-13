// nr_roscore -- a standalone ROS master (roscore) in C++, no ROS installed.
//
// Implements the ROS Master API (register/unregister publisher/subscriber/
// service, lookupService/Node, getSystemState, getPublishedTopics, ...) and a
// scalar/array Parameter Server, and notifies subscribers via publisherUpdate.
// Real ROS nodes (rospy/roscpp) and irap_noroslib nodes register with it.
//
//   ./nr_roscore                 # binds :11311, advertises this host
//   ./nr_roscore --port 11322
//   ROS_MASTER_URI=http://host:11311 ROS_HOSTNAME=host ./nr_roscore
//
// Config: port  = --port | port in $ROS_MASTER_URI | 11311
//         host  = --host | $ROS_HOSTNAME | $ROS_IP | system hostname
//
// NOTE: parameters are scalar/array only (the Python nr_roscore does full dict
// trees). Enough for topics, services, actions and typical scalar params.
#include "irap_noroslib.hpp"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using irap_noroslib::XmlValue;

// Portable getpid (defined after main's helpers); declared here for early use.
int getpid_portable();

// ---- registry -------------------------------------------------------------
namespace {

std::mutex g_mu;
std::map<std::string, std::map<std::string, std::string>> g_pubs;   // topic -> {node: api}
std::map<std::string, std::map<std::string, std::string>> g_subs;   // topic -> {node: api}
std::map<std::string, std::pair<std::string, std::string>> g_srvs;  // service -> (node, rosrpc)
std::map<std::string, std::string> g_types;                         // topic -> type
std::map<std::string, std::string> g_node_apis;                     // node -> slave api
XmlValue g_param_root = XmlValue::StructVal({});                    // hierarchical param tree
std::string g_master_uri;
bool g_quiet = false;

void logline(const std::string& s) {
  if (!g_quiet) { std::printf("[nr_roscore] %s\n", s.c_str()); std::fflush(stdout); }
}

XmlValue reply(int code, const std::string& msg, XmlValue val) {
  return XmlValue::Array({XmlValue::Int(code), XmlValue::Str(msg), std::move(val)});
}

std::vector<std::string> map_values(const std::map<std::string, std::string>& m) {
  std::vector<std::string> v;
  for (auto& kv : m) v.push_back(kv.second);
  return v;
}

// Fire publisherUpdate(caller=/master, topic, [pub apis]) at every subscriber.
void notify_subscribers(const std::string& topic) {
  std::vector<std::string> subs, pubs;
  {
    std::lock_guard<std::mutex> lk(g_mu);
    subs = map_values(g_subs[topic]);
    pubs = map_values(g_pubs[topic]);
  }
  std::vector<XmlValue> pub_arr;
  for (auto& p : pubs) pub_arr.push_back(XmlValue::Str(p));
  for (auto& api : subs) {
    std::thread([api, topic, pub_arr] {
      XmlValue res; std::string err;
      irap_noroslib::xmlrpc_call(api, "publisherUpdate",
                         {XmlValue::Str("/master"), XmlValue::Str(topic), XmlValue::Array(pub_arr)},
                         &res, &err);
    }).detach();
  }
}

// ---- parameter tree (full dict support via XmlValue::Struct) ---------------
std::string norm_key(std::string k) {
  if (k.empty() || k[0] != '/') k = "/" + k;
  while (k.size() > 1 && k.back() == '/') k.pop_back();
  return k;
}

std::vector<std::string> split_key(const std::string& key) {
  std::vector<std::string> parts;
  std::string cur;
  for (char ch : key) {
    if (ch == '/') { if (!cur.empty()) { parts.push_back(cur); cur.clear(); } }
    else cur += ch;
  }
  if (!cur.empty()) parts.push_back(cur);
  return parts;
}

// Find (or, if create, make) the named child of a Struct node.
XmlValue* child_of(XmlValue* node, const std::string& name, bool create) {
  if (node->type != XmlValue::Type::Struct) {
    if (!create) return nullptr;
    *node = XmlValue::StructVal({});
  }
  for (auto& kv : node->members) if (kv.first == name) return &kv.second;
  if (!create) return nullptr;
  node->members.emplace_back(name, XmlValue());
  return &node->members.back().second;
}

// caller must hold g_mu
void param_set(const std::string& key, const XmlValue& val) {
  auto parts = split_key(key);
  if (parts.empty()) { g_param_root = val; return; }
  XmlValue* node = &g_param_root;
  for (size_t i = 0; i + 1 < parts.size(); ++i) node = child_of(node, parts[i], true);
  *child_of(node, parts.back(), true) = val;
}

bool param_get(const std::string& key, XmlValue* out) {
  auto parts = split_key(key);
  XmlValue* node = &g_param_root;
  for (const auto& p : parts) {
    node = child_of(node, p, false);
    if (!node) return false;
  }
  *out = *node;
  return true;
}

bool param_has(const std::string& key) { XmlValue tmp; return param_get(key, &tmp); }

bool param_delete(const std::string& key) {
  auto parts = split_key(key);
  if (parts.empty()) { g_param_root = XmlValue::StructVal({}); return true; }
  XmlValue* node = &g_param_root;
  for (size_t i = 0; i + 1 < parts.size(); ++i) {
    node = child_of(node, parts[i], false);
    if (!node) return false;
  }
  if (node->type != XmlValue::Type::Struct) return false;
  for (auto it = node->members.begin(); it != node->members.end(); ++it) {
    if (it->first == parts.back()) { node->members.erase(it); return true; }
  }
  return false;
}

void param_names(const XmlValue& node, const std::string& prefix, std::vector<std::string>* out) {
  if (node.type == XmlValue::Type::Struct && !node.members.empty()) {
    for (const auto& kv : node.members) param_names(kv.second, prefix + "/" + kv.first, out);
  } else if (!prefix.empty()) {
    out->push_back(prefix);
  }
}

// ---- master method dispatch -----------------------------------------------
XmlValue dispatch(const std::string& method, const std::vector<XmlValue>& p) {
  auto arg = [&](size_t i) -> std::string { return i < p.size() ? p[i].as_str() : std::string(); };
  std::string caller = arg(0);

  if (method == "getUri") return reply(1, "", XmlValue::Str(g_master_uri));
  if (method == "getPid") return reply(1, "", XmlValue::Int((int64_t)getpid_portable()));

  if (method == "registerPublisher") {
    std::string topic = arg(1), type = arg(2), api = arg(3);
    std::vector<std::string> subs;
    {
      std::lock_guard<std::mutex> lk(g_mu);
      g_pubs[topic][caller] = api;
      g_node_apis[caller] = api;
      if (!type.empty() && type != "*") g_types.emplace(topic, type);
      subs = map_values(g_subs[topic]);
    }
    logline("+pub  " + topic + "  [" + type + "]  by " + caller);
    notify_subscribers(topic);
    std::vector<XmlValue> a; for (auto& s : subs) a.push_back(XmlValue::Str(s));
    return reply(1, "registered", XmlValue::Array(a));
  }
  if (method == "unregisterPublisher") {
    std::string topic = arg(1); bool existed;
    { std::lock_guard<std::mutex> lk(g_mu); existed = g_pubs[topic].erase(caller) > 0; }
    if (existed) { logline("-pub  " + topic + "  by " + caller); notify_subscribers(topic); }
    return reply(1, "", XmlValue::Int(existed ? 1 : 0));
  }
  if (method == "registerSubscriber") {
    std::string topic = arg(1), type = arg(2), api = arg(3);
    std::vector<std::string> pubs;
    {
      std::lock_guard<std::mutex> lk(g_mu);
      g_subs[topic][caller] = api;
      g_node_apis[caller] = api;
      if (!type.empty() && type != "*") g_types.emplace(topic, type);
      pubs = map_values(g_pubs[topic]);
    }
    logline("+sub  " + topic + "  [" + type + "]  by " + caller);
    std::vector<XmlValue> a; for (auto& s : pubs) a.push_back(XmlValue::Str(s));
    return reply(1, "registered", XmlValue::Array(a));
  }
  if (method == "unregisterSubscriber") {
    std::string topic = arg(1); bool existed;
    { std::lock_guard<std::mutex> lk(g_mu); existed = g_subs[topic].erase(caller) > 0; }
    if (existed) logline("-sub  " + topic + "  by " + caller);
    return reply(1, "", XmlValue::Int(existed ? 1 : 0));
  }
  if (method == "registerService") {
    std::string service = arg(1), srv_api = arg(2), api = arg(3);
    { std::lock_guard<std::mutex> lk(g_mu); g_srvs[service] = {caller, srv_api}; g_node_apis[caller] = api; }
    logline("+srv  " + service + "  at " + srv_api + "  by " + caller);
    return reply(1, "registered", XmlValue::Int(1));
  }
  if (method == "unregisterService") {
    std::string service = arg(1); bool existed;
    { std::lock_guard<std::mutex> lk(g_mu); existed = g_srvs.erase(service) > 0; }
    if (existed) logline("-srv  " + service + "  by " + caller);
    return reply(1, "", XmlValue::Int(existed ? 1 : 0));
  }
  if (method == "lookupService") {
    std::string service = arg(1), url;
    { std::lock_guard<std::mutex> lk(g_mu); auto it = g_srvs.find(service); if (it != g_srvs.end()) url = it->second.second; }
    if (url.empty()) return reply(-1, "no provider", XmlValue::Str(""));
    return reply(1, "", XmlValue::Str(url));
  }
  if (method == "lookupNode") {
    std::string node = arg(1), api;
    { std::lock_guard<std::mutex> lk(g_mu); auto it = g_node_apis.find(node); if (it != g_node_apis.end()) api = it->second; }
    if (api.empty()) return reply(-1, "unknown node", XmlValue::Str(""));
    return reply(1, "", XmlValue::Str(api));
  }
  if (method == "getPublishedTopics") {
    std::vector<XmlValue> out;
    std::lock_guard<std::mutex> lk(g_mu);
    for (auto& kv : g_pubs) {
      if (kv.second.empty()) continue;
      std::string ty = g_types.count(kv.first) ? g_types[kv.first] : "*";
      out.push_back(XmlValue::Array({XmlValue::Str(kv.first), XmlValue::Str(ty)}));
    }
    return reply(1, "", XmlValue::Array(out));
  }
  if (method == "getTopicTypes") {
    std::vector<XmlValue> out;
    std::lock_guard<std::mutex> lk(g_mu);
    for (auto& kv : g_types) out.push_back(XmlValue::Array({XmlValue::Str(kv.first), XmlValue::Str(kv.second)}));
    return reply(1, "", XmlValue::Array(out));
  }
  if (method == "getSystemState") {
    auto state = [](const std::map<std::string, std::map<std::string, std::string>>& m) {
      std::vector<XmlValue> out;
      for (auto& kv : m) {
        if (kv.second.empty()) continue;
        std::vector<XmlValue> who;
        for (auto& n : kv.second) who.push_back(XmlValue::Str(n.first));
        out.push_back(XmlValue::Array({XmlValue::Str(kv.first), XmlValue::Array(who)}));
      }
      return XmlValue::Array(out);
    };
    std::lock_guard<std::mutex> lk(g_mu);
    std::vector<XmlValue> srv;
    for (auto& kv : g_srvs)
      srv.push_back(XmlValue::Array({XmlValue::Str(kv.first),
                                     XmlValue::Array({XmlValue::Str(kv.second.first)})}));
    return reply(1, "", XmlValue::Array({state(g_pubs), state(g_subs), XmlValue::Array(srv)}));
  }

  // -- parameter server (full dict tree via XmlValue::Struct) --
  if (method == "setParam") {
    std::string key = norm_key(arg(1));
    XmlValue val = p.size() > 2 ? p[2] : XmlValue::Str("");
    { std::lock_guard<std::mutex> lk(g_mu); param_set(key, val); }
    logline("param set " + key);
    return reply(1, "", XmlValue::Int(0));
  }
  if (method == "getParam") {
    std::string key = norm_key(arg(1));
    std::lock_guard<std::mutex> lk(g_mu);
    XmlValue val;
    if (!param_get(key, &val)) return reply(0, "Parameter [" + key + "] is not set", XmlValue::Int(0));
    return reply(1, "", val);
  }
  if (method == "hasParam") {
    std::string key = norm_key(arg(1));
    std::lock_guard<std::mutex> lk(g_mu);
    return reply(1, key, XmlValue::Bool(param_has(key)));
  }
  if (method == "deleteParam") {
    std::string key = norm_key(arg(1)); bool existed;
    { std::lock_guard<std::mutex> lk(g_mu); existed = param_delete(key); }
    if (!existed) return reply(0, "Parameter [" + key + "] is not set", XmlValue::Int(0));
    return reply(1, "", XmlValue::Int(0));
  }
  if (method == "getParamNames") {
    std::vector<XmlValue> names;
    {
      std::lock_guard<std::mutex> lk(g_mu);
      std::vector<std::string> flat;
      param_names(g_param_root, "", &flat);
      for (auto& n : flat) names.push_back(XmlValue::Str(n));
    }
    return reply(1, "", XmlValue::Array(names));
  }
  if (method == "searchParam") {
    // Walk the caller's namespace upward looking for the key's head element.
    std::string key = arg(1);
    std::string ns = caller;
    auto slash = ns.rfind('/');
    ns = (slash == std::string::npos || slash == 0) ? "" : ns.substr(0, slash);
    std::lock_guard<std::mutex> lk(g_mu);
    while (true) {
      std::string cand = norm_key(ns + "/" + key);
      if (param_has(cand)) return reply(1, "", XmlValue::Str(cand));
      if (ns.empty()) break;
      auto s = ns.rfind('/');
      ns = (s == std::string::npos) ? "" : ns.substr(0, s);
    }
    return reply(-1, "Cannot find parameter [" + key + "]", XmlValue::Str(""));
  }

  // Unknown but harmless (e.g. subscribeParam/getMasterUri): reply politely.
  return reply(1, "unhandled: " + method, XmlValue::Int(0));
}

// ---- HTTP + accept loop ---------------------------------------------------
bool read_http_request(int fd, std::string* body) {
  std::string data; char buf[4096];
  size_t header_end = std::string::npos, content_length = 0; bool have_len = false;
  while (true) {
    if (header_end == std::string::npos) {
      header_end = data.find("\r\n\r\n");
      if (header_end != std::string::npos) {
        std::string headers = data.substr(0, header_end), lower = headers;
        for (auto& c : lower) c = (char)tolower(c);
        size_t q = lower.find("content-length:");
        if (q != std::string::npos) { content_length = (size_t)std::atoll(headers.c_str() + q + 15); have_len = true; }
      }
    }
    if (header_end != std::string::npos) {
      size_t have = data.size() - (header_end + 4);
      if (!have_len || have >= content_length) break;
    }
    ssize_t r = irap_noroslib::net_recv(fd, buf, sizeof(buf), 0);
    if (r <= 0) break;
    data.append(buf, (size_t)r);
  }
  if (header_end == std::string::npos) return false;
  *body = data.substr(header_end + 4, have_len ? content_length : std::string::npos);
  return true;
}

void send_http_response(int fd, const std::string& xml) {
  std::ostringstream resp;
  resp << "HTTP/1.0 200 OK\r\nContent-Type: text/xml\r\nContent-Length: " << xml.size() << "\r\n\r\n" << xml;
  std::string s = resp.str();
  irap_noroslib::write_n(fd, s.data(), s.size());
}

void handle(int fd) {
  std::string body;
  if (read_http_request(fd, &body)) {
    std::string method; std::vector<XmlValue> params;
    XmlValue result;
    if (irap_noroslib::parse_method_call(body, &method, &params)) result = dispatch(method, params);
    else result = reply(0, "parse error", XmlValue::Int(0));
    send_http_response(fd, irap_noroslib::build_method_response(result));
  }
  irap_noroslib::net_close(fd);
}

// ---- /rosout aggregator ---------------------------------------------------
// A raw-passthrough rosgraph_msgs/Log message: the aggregator forwards the exact
// bytes from /rosout to /rosout_agg, so we never need to parse the fields.
struct Log {
  static constexpr const char* TYPE = "rosgraph_msgs/Log";
  static constexpr const char* MD5 = "acffd30cd6b6de30f120938c17c593fb";
  static constexpr const char* DEFINITION =
      "byte DEBUG=1\nbyte INFO=2\nbyte WARN=4\nbyte ERROR=8\nbyte FATAL=16\n"
      "Header header\nbyte level\nstring name\nstring msg\nstring file\n"
      "string function\nuint32 line\nstring[] topics\n";
  std::vector<uint8_t> raw;
  std::vector<uint8_t> serialize() const { return raw; }
  static Log deserialize(const std::vector<uint8_t>& b) { Log m; m.raw = b; return m; }
};

// Runs the aggregator node and spins (blocks until shutdown / Ctrl-C).
void run_rosout(int port, const std::string& host) {
  irap_noroslib::init_node("/rosout", "http://127.0.0.1:" + std::to_string(port) + "/", host);
  static irap_noroslib::Publisher<Log> agg("/rosout_agg");
  static irap_noroslib::Subscriber<Log> sub("/rosout", [](const Log& m) { agg.publish(m); });
  irap_noroslib::loginfo("started /rosout aggregator (-> /rosout_agg)");
  irap_noroslib::spin();
}

}  // namespace

int getpid_portable() {
#if defined(_WIN32)
  return (int)GetCurrentProcessId();
#else
  return (int)::getpid();
#endif
}

int main(int argc, char** argv) {
  irap_noroslib::net_startup();
  std::string host, bind = "0.0.0.0";
  int port = 0;
  bool rosout = true;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if ((a == "-p" || a == "--port") && i + 1 < argc) port = std::atoi(argv[++i]);
    else if (a == "--host" && i + 1 < argc) host = argv[++i];
    else if (a == "--bind" && i + 1 < argc) bind = argv[++i];
    else if (a == "-q" || a == "--quiet") g_quiet = true;
    else if (a == "--no-rosout") rosout = false;
  }
  if (port == 0) {
    if (const char* mu = std::getenv("ROS_MASTER_URI")) {
      std::string h; uint16_t pp = 0;
      if (irap_noroslib::parse_http_uri(mu, &h, &pp)) port = pp;
    }
  }
  if (port == 0) port = 11311;
  if (host.empty()) {
    if (const char* h = std::getenv("ROS_HOSTNAME")) host = h;
    else if (const char* h2 = std::getenv("ROS_IP")) host = h2;
    else { char hn[256]; host = (gethostname(hn, sizeof(hn)) == 0) ? hn : "localhost"; }
  }
  g_master_uri = "http://" + host + ":" + std::to_string(port) + "/";

  uint16_t bound = 0;
  int listen_fd = irap_noroslib::tcp_listen(bind, (uint16_t)port, &bound);
  if (listen_fd < 0) {
    std::fprintf(stderr, "[nr_roscore] failed to bind %s:%d (in use?)\n", bind.c_str(), port);
    return 1;
  }
  // Seed the params a roscore normally provides.
  param_set("/run_id", XmlValue::Str("irap_noroslib-" + std::to_string(getpid_portable())));
  param_set("/rosdistro", XmlValue::Str("irap_noroslib\n"));
  param_set("/rosversion", XmlValue::Str("irap_noroslib 0.1.0\n"));

  std::printf("[nr_roscore] ROS master online\n");
  std::printf("[nr_roscore] ROS_MASTER_URI=%s  (bind %s:%d)\n", g_master_uri.c_str(), bind.c_str(), port);
  std::printf("[nr_roscore] point nodes here:  export ROS_MASTER_URI=%s\n", g_master_uri.c_str());
  std::fflush(stdout);

  // Master accept loop runs in the background so the main thread can host the
  // /rosout aggregator (whose spin() also gives us clean Ctrl-C shutdown).
  std::thread([listen_fd] {
    for (;;) {
      int fd = accept(listen_fd, nullptr, nullptr);
      if (fd < 0) continue;
      std::thread(handle, fd).detach();
    }
  }).detach();

  if (rosout) {
    run_rosout(port, host);           // blocks until Ctrl-C
  } else {
    for (;;) std::this_thread::sleep_for(std::chrono::hours(1));
  }
  return 0;
}
