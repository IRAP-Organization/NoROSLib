// nr_rostopic -- `rostopic`, without ROS installed.
//
//   nr_rostopic list [-v]
//   nr_rostopic type   /chatter
//   nr_rostopic info   /chatter
//   nr_rostopic echo [-n N] [--noarr] /chatter
//   nr_rostopic hz     /chatter
//   nr_rostopic bw     /chatter
//   nr_rostopic pub [-r HZ | -1] /chatter std_msgs/String "data: hi"
//   nr_rostopic find   std_msgs/String
//
// `echo` decodes ANY topic, including a custom type you have no .msg file for: a
// ROS publisher hands over the full message definition during the TCPROS
// handshake, so the type is rebuilt on the spot. (Real `rostopic echo` cannot do
// that -- it needs the message class built in a catkin package first.)
//
// Master: $ROS_MASTER_URI, or --master http://host:11311
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "irap_noroslib.hpp"

using namespace irap_noroslib;

static std::string g_master = "http://localhost:11311";
static std::string g_master_origin = "the default";   // where g_master came from
static const char* CALLER = "/nr_rostopic";

// ------------------------------------------------------------------ output ---
static const size_t kArrayElide = 12;

static std::string fmt_value(const Value& v, int indent, bool noarr);

static std::string fmt_msg(const DynamicMessage& m, int indent, bool noarr) {
  std::string pad(indent * 2, ' ');
  std::string out;
  size_t slot = 0;
  for (const Field& f : m.spec().fields()) {
    if (f.is_constant) continue;
    const Value& v = m.values()[slot++];
    if (v.kind() == Value::MSG) {
      out += pad + f.name + ": \n" + fmt_msg(v.msg(), indent + 1, noarr);
    } else if (v.kind() == Value::ARRAY && !v.array().empty() &&
               v.array()[0].kind() == Value::MSG) {
      out += pad + f.name + ": \n";
      if (noarr) { out += pad + "  <array of " + std::to_string(v.array().size()) + ">\n"; continue; }
      for (const Value& el : v.array())
        out += pad + "  - \n" + fmt_msg(el.msg(), indent + 2, noarr);
    } else {
      out += pad + f.name + ": " + fmt_value(v, indent, noarr) + "\n";
    }
  }
  return out;
}

static std::string fmt_value(const Value& v, int indent, bool noarr) {
  std::string pad(indent * 2, ' ');
  switch (v.kind()) {
    case Value::BYTES: {
      const auto& b = v.bytes();
      if (noarr || b.size() > kArrayElide)
        return "<" + std::to_string(b.size()) + " bytes>";
      std::string s = "[";
      for (size_t i = 0; i < b.size(); ++i)
        s += (i ? ", " : "") + std::to_string((int)b[i]);
      return s + "]";
    }
    case Value::ARRAY: {
      const auto& a = v.array();
      if (noarr || a.size() > kArrayElide)
        return "<array of " + std::to_string(a.size()) + ">";
      std::string s = "[";
      for (size_t i = 0; i < a.size(); ++i)
        s += (i ? ", " : "") + a[i].str();
      return s + "]";
    }
    case Value::TIME:
    case Value::DURATION:
      return "\n" + pad + "  secs: " + v.str().substr(0, v.str().find('.')) +
             "\n" + pad + "  nsecs: " + v.str().substr(v.str().find('.') + 1);
    default:
      return v.str();
  }
}

// ------------------------------------------------------------------ master ---
static bool topic_types(std::vector<std::pair<std::string, std::string>>* out) {
  std::string err;
  if (!get_topic_types(g_master, CALLER, out, &err)) {
    // Say which master, and where it came from -- a stale $ROS_MASTER_URI is the
    // usual culprit, and "cannot reach the master" alone hides that.
    fprintf(stderr,
            "nr_rostopic: cannot reach the ROS master at %s\n"
            "             (from %s)\n"
            "             %s\n\n"
            "Is a master running? Start one with:  nr_roscore\n"
            "Point at a different one, and remember it here:\n"
            "    nr_rostopic --set_ros_master_uri http://HOST:11311 "
            "--set_ros_hostname YOUR_IP\n",
            g_master.c_str(), g_master_origin.c_str(), err.c_str());
    return false;
  }
  return true;
}

static std::string type_of(const std::string& topic) {
  std::vector<std::pair<std::string, std::string>> ts;
  if (!topic_types(&ts)) return "";
  for (const auto& t : ts)
    if (t.first == topic) return t.second;
  return "";
}

// ---------------------------------------------------------------- commands ---
static int cmd_list(bool verbose) {
  std::vector<std::pair<std::string, std::string>> ts;
  if (!topic_types(&ts)) return 1;
  std::map<std::string, std::string> sorted(ts.begin(), ts.end());
  if (!verbose) {
    for (const auto& t : sorted) printf("%s\n", t.first.c_str());
    return 0;
  }
  GraphMap pubs, subs, srvs;
  std::string err;
  if (!get_system_state(g_master, CALLER, &pubs, &subs, &srvs, &err)) return 1;
  printf("\nPublished topics:\n");
  for (const auto& p : pubs)
    printf(" * %s [%s] %zu publisher(s)\n", p.first.c_str(),
           sorted.count(p.first) ? sorted[p.first].c_str() : "?", p.second.size());
  printf("\nSubscribed topics:\n");
  for (const auto& s : subs)
    printf(" * %s [%s] %zu subscriber(s)\n", s.first.c_str(),
           sorted.count(s.first) ? sorted[s.first].c_str() : "?", s.second.size());
  printf("\n");
  return 0;
}

static int cmd_type(const std::string& topic) {
  std::string t = type_of(topic);
  if (t.empty()) { fprintf(stderr, "Unknown topic %s\n", topic.c_str()); return 1; }
  printf("%s\n", t.c_str());
  return 0;
}

static int cmd_info(const std::string& topic) {
  std::string t = type_of(topic);
  if (t.empty()) { fprintf(stderr, "Unknown topic %s\n", topic.c_str()); return 1; }
  GraphMap pubs, subs, srvs;
  std::string err;
  if (!get_system_state(g_master, CALLER, &pubs, &subs, &srvs, &err)) return 1;
  printf("Type: %s\n\nPublishers: \n", t.c_str());
  bool any = false;
  for (const auto& p : pubs)
    if (p.first == topic)
      for (const auto& n : p.second) { printf(" * %s\n", n.c_str()); any = true; }
  if (!any) printf(" * None\n");
  printf("\nSubscribers: \n");
  any = false;
  for (const auto& s : subs)
    if (s.first == topic)
      for (const auto& n : s.second) { printf(" * %s\n", n.c_str()); any = true; }
  if (!any) printf(" * None\n");
  printf("\n");
  return 0;
}

static int cmd_find(const std::string& type) {
  std::vector<std::pair<std::string, std::string>> ts;
  if (!topic_types(&ts)) return 1;
  std::map<std::string, std::string> sorted(ts.begin(), ts.end());
  for (const auto& t : sorted)
    if (t.second == type) printf("%s\n", t.first.c_str());
  return 0;
}

static int cmd_echo(const std::string& topic, int count, bool noarr) {
  init_node("nr_rostopic");
  std::atomic<int> n{0};
  AnySubscriber sub(topic, [&](const DynamicMessage& m, const MsgType&) {
    printf("%s---\n", fmt_msg(m, 0, noarr).c_str());
    fflush(stdout);
    ++n;
  });
  while (ok() && (count <= 0 || n.load() < count))
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  return 0;
}

static int cmd_hz(const std::string& topic) {
  init_node("nr_rostopic");
  std::mutex mu;
  std::vector<double> stamps;
  AnySubscriber sub(topic, [&](const DynamicMessage&, const MsgType&) {
    int64_t s, ns;
    wall_time(&s, &ns);
    std::lock_guard<std::mutex> lk(mu);
    stamps.push_back(s + ns * 1e-9);
  });
  printf("subscribed to [%s]\n", topic.c_str());
  fflush(stdout);
  while (ok()) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::vector<double> t;
    { std::lock_guard<std::mutex> lk(mu); t.swap(stamps); }
    if (t.size() < 2) { printf("no new messages\n"); fflush(stdout); continue; }
    double mn = 1e9, mx = 0, sum = 0;
    for (size_t i = 1; i < t.size(); ++i) {
      double d = t[i] - t[i - 1];
      mn = std::min(mn, d); mx = std::max(mx, d); sum += d;
    }
    double mean = sum / (t.size() - 1);
    double var = 0;
    for (size_t i = 1; i < t.size(); ++i) {
      double d = t[i] - t[i - 1] - mean;
      var += d * d;
    }
    var /= (t.size() - 1);
    printf("average rate: %.3f\n\tmin: %.3fs max: %.3fs std dev: %.5fs window: %zu\n",
           mean > 0 ? 1.0 / mean : 0.0, mn, mx, std::sqrt(var), t.size());
    fflush(stdout);
  }
  return 0;
}

static std::string hsize(double n) {
  const char* u[] = {"B", "KB", "MB", "GB"};
  int i = 0;
  while (n >= 1024 && i < 3) { n /= 1024; ++i; }
  char buf[64];
  snprintf(buf, sizeof(buf), "%.2f%s", n, u[i]);
  return buf;
}

static int cmd_bw(const std::string& topic) {
  init_node("nr_rostopic");
  std::mutex mu;
  std::vector<size_t> sizes;
  auto sub = detail::subscribe_any(
      topic, [&](const std::string&, const std::string&, const std::string&,
                 const std::vector<uint8_t>& body) {
        std::lock_guard<std::mutex> lk(mu);
        sizes.push_back(body.size());
      });
  printf("subscribed to [%s]\n", topic.c_str());
  fflush(stdout);
  while (ok()) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::vector<size_t> s;
    { std::lock_guard<std::mutex> lk(mu); s.swap(sizes); }
    if (s.empty()) { printf("no new messages\n"); fflush(stdout); continue; }
    size_t total = 0, mn = s[0], mx = s[0];
    for (size_t v : s) { total += v; mn = std::min(mn, v); mx = std::max(mx, v); }
    printf("average: %s/s\n\tmean: %s min: %s max: %s window: %zu\n",
           hsize((double)total).c_str(), hsize((double)total / s.size()).c_str(),
           hsize((double)mn).c_str(), hsize((double)mx).c_str(), s.size());
    fflush(stdout);
  }
  return 0;
}

// -- a tiny YAML-flow parser: {a: 1, b: {c: 2}, d: [1,2]} -> dotted set() calls --
static void skip_ws(const std::string& s, size_t* i) {
  while (*i < s.size() && (s[*i] == ' ' || s[*i] == '\t' || s[*i] == '\n' || s[*i] == ','))
    ++*i;
}

static void assign(DynamicMessage& m, const std::string& path, const std::string& tok) {
  const Field* f = m.spec().field_of(path.substr(0, path.find('.')));
  (void)f;
  // let the message's own field type decide how to read the token
  try {
    if (tok == "true" || tok == "false") { m.set(path, tok == "true"); return; }
    size_t used = 0;
    double d = std::stod(tok, &used);
    if (used == tok.size()) { m.set(path, d); return; }
  } catch (...) {}
  m.set(path, tok);
}

static void parse_into(const std::string& s, size_t* i, DynamicMessage& m,
                       const std::string& prefix);

static std::string read_token(const std::string& s, size_t* i) {
  skip_ws(s, i);
  if (s[*i] == '"' || s[*i] == '\'') {
    char q = s[*i];
    ++*i;
    size_t start = *i;
    while (*i < s.size() && s[*i] != q) ++*i;
    std::string v = s.substr(start, *i - start);
    ++*i;
    return v;
  }
  size_t start = *i;
  while (*i < s.size() && s[*i] != ',' && s[*i] != '}' && s[*i] != ']') ++*i;
  std::string v = s.substr(start, *i - start);
  while (!v.empty() && (v.back() == ' ' || v.back() == '\t')) v.pop_back();
  return v;
}

static void parse_value(const std::string& s, size_t* i, DynamicMessage& m,
                        const std::string& path) {
  skip_ws(s, i);
  if (s[*i] == '{') { parse_into(s, i, m, path + "."); return; }
  if (s[*i] == '[') {                       // an array
    ++*i;
    std::vector<std::string> toks;
    skip_ws(s, i);
    while (*i < s.size() && s[*i] != ']') {
      toks.push_back(read_token(s, i));
      skip_ws(s, i);
    }
    ++*i;                                    // ]
    Value& slot = m.at(path);
    if (slot.kind() == Value::BYTES) {
      std::vector<uint8_t> b;
      for (const auto& t : toks) b.push_back((uint8_t)std::stoi(t));
      m.set_bytes(path, b);
    } else {
      std::vector<double> d;
      for (const auto& t : toks) d.push_back(std::stod(t));
      m.set_array(path, d);
    }
    return;
  }
  assign(m, path, read_token(s, i));
}

static void parse_into(const std::string& s, size_t* i, DynamicMessage& m,
                       const std::string& prefix) {
  ++*i;                                      // {
  skip_ws(s, i);
  while (*i < s.size() && s[*i] != '}') {
    size_t start = *i;
    while (*i < s.size() && s[*i] != ':') ++*i;
    std::string key = s.substr(start, *i - start);
    while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.pop_back();
    size_t a = key.find_first_not_of(" \t\"'");
    size_t b = key.find_last_not_of(" \t\"'");
    key = key.substr(a, b - a + 1);
    ++*i;                                    // :
    parse_value(s, i, m, prefix + key);
    skip_ws(s, i);
  }
  ++*i;                                      // }
}

static int cmd_pub(const std::string& topic, const std::string& type,
                   const std::string& data, double rate, bool once) {
  if (!has_msg_type(type)) {
    fprintf(stderr,
            "Unknown message type [%s]. It is not a built-in; load its .msg first\n"
            "with load_msg_file(), or publish a built-in type.\n", type.c_str());
    return 1;
  }
  MsgType t = get_msg_type(type);
  DynamicMessage m = t.create();
  if (!data.empty()) {
    std::string d = data;
    if (d[0] != '{') d = "{" + d + "}";
    size_t i = 0;
    try {
      parse_into(d, &i, m, "");
    } catch (const std::exception& e) {
      fprintf(stderr, "could not parse the message data: %s\n", e.what());
      return 1;
    }
  }

  init_node("nr_rostopic");
  DynamicPublisher pub(topic, t, once);
  if (once) {
    for (int i = 0; i < 60 && pub.get_num_connections() == 0; ++i)
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    pub.publish(m);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    return 0;
  }
  Rate r(rate > 0 ? rate : 10);
  while (ok()) { pub.publish(m); r.sleep(); }
  return 0;
}

// -------------------------------------------------------------------- main ---
static void usage() {
  printf(
      "nr_rostopic -- rostopic, with no ROS installed\n\n"
      "  nr_rostopic list [-v]\n"
      "  nr_rostopic type   TOPIC\n"
      "  nr_rostopic info   TOPIC\n"
      "  nr_rostopic find   TYPE\n"
      "  nr_rostopic echo [-n N] [--noarr] TOPIC\n"
      "  nr_rostopic hz     TOPIC\n"
      "  nr_rostopic bw     TOPIC\n"
      "  nr_rostopic pub [-r HZ | -1] TOPIC TYPE \"data\"\n\n"
      "  --master HOST  master host, IP, host:port or full URI\n"
      "  --port N       master port (default: 11311)\n"
      "  --host HOST    our hostname -- how other nodes reach us\n\n"
      "Remember the master once, in ./master.yaml:\n"
      "  nr_rostopic --set_ros_master_uri http://HOST:11311 --set_ros_hostname YOUR_IP\n"
      "  nr_rostopic list          # no flags needed any more\n\n"
      "`echo` decodes ANY topic -- even a custom type you have no .msg file for.\n");
}

// ------------------------------------------------- remembered settings ----
// nr_rostopic remembers the master URI and hostname in a master.yaml in the
// CURRENT DIRECTORY, so you set them once and every later command just works.
// The file WINS over $ROS_MASTER_URI / $ROS_IP / $ROS_HOSTNAME: a stale env var
// from some other ROS setup must not silently override what you saved. An
// explicit --master/--port/--host still beats the file.
static const char* MASTER_YAML = "master.yaml";

// A 10-line parser, not a YAML library: this file only holds two key: value lines.
static void load_master_yaml(std::string* uri, std::string* host) {
  std::ifstream f(MASTER_YAML);
  if (!f) return;
  std::string line;
  while (std::getline(f, line)) {
    size_t b = line.find_first_not_of(" \t");
    if (b == std::string::npos || line[b] == '#') continue;
    size_t colon = line.find(':', b);
    if (colon == std::string::npos) continue;
    std::string key = line.substr(b, colon - b);
    std::string val = line.substr(colon + 1);
    size_t vb = val.find_first_not_of(" \t");
    if (vb == std::string::npos) continue;
    size_t ve = val.find_last_not_of(" \t\r\n");
    val = val.substr(vb, ve - vb + 1);
    if (val.size() >= 2 && (val.front() == '"' || val.front() == '\'') && val.back() == val.front())
      val = val.substr(1, val.size() - 2);
    if (key == "ros_master_uri") *uri = val;
    else if (key == "ros_hostname") *host = val;
  }
}

static bool save_master_yaml(const std::string& uri, const std::string& host) {
  std::string cur_uri, cur_host;
  load_master_yaml(&cur_uri, &cur_host);          // keep what wasn't given now
  if (!uri.empty()) cur_uri = uri;
  if (!host.empty()) cur_host = host;
  std::ofstream f(MASTER_YAML);
  if (!f) {
    fprintf(stderr, "nr_rostopic: cannot write %s here\n", MASTER_YAML);
    return false;
  }
  f << "# nr_rostopic remembered settings. Delete this file to forget them.\n"
    << "# These win over $ROS_MASTER_URI / $ROS_IP / $ROS_HOSTNAME.\n";
  if (!cur_uri.empty()) f << "ros_master_uri: " << cur_uri << "\n";
  if (!cur_host.empty()) f << "ros_hostname: " << cur_host << "\n";
  f.close();
  printf("saved to ./%s\n", MASTER_YAML);
  if (!cur_uri.empty()) printf("  ros_master_uri: %s\n", cur_uri.c_str());
  if (!cur_host.empty()) printf("  ros_hostname:   %s\n", cur_host.c_str());
  printf("\nnr_rostopic in this directory now uses these -- no flags needed.\n");
  return true;
}

// Accepts a full URI, a host:port, or a bare host/IP -- so all of these agree:
//   --master http://127.0.0.1:11311   --master 127.0.0.1:11311
//   --master 127.0.0.1 --port 11311   --master 127.0.0.1   (port defaults to 11311)
static std::string resolve_master(std::string uri, int port) {
  if (uri.find("://") == std::string::npos) uri = "http://" + uri;
  size_t sep = uri.find("://");
  std::string scheme = uri.substr(0, sep);
  std::string rest = uri.substr(sep + 3);
  while (!rest.empty() && rest.back() == '/') rest.pop_back();
  size_t colon = rest.find(':');
  std::string host = colon == std::string::npos ? rest : rest.substr(0, colon);
  std::string p = colon == std::string::npos ? "11311" : rest.substr(colon + 1);
  if (port > 0) p = std::to_string(port);
  return scheme + "://" + host + ":" + p;
}

int main(int argc, char** argv) {
  int port = 0;
  std::string cli_master, cli_host, set_uri, set_host;
  std::vector<std::string> a;
  for (int i = 1; i < argc; ++i) {
    std::string s = argv[i];
    if (s == "--master" && i + 1 < argc) { cli_master = argv[++i]; continue; }
    if (s == "--port" && i + 1 < argc) { port = std::atoi(argv[++i]); continue; }
    if (s == "--host" && i + 1 < argc) { cli_host = argv[++i]; continue; }
    if (s == "--set_ros_master_uri" && i + 1 < argc) { set_uri = argv[++i]; continue; }
    if (s == "--set_ros_hostname" && i + 1 < argc) { set_host = argv[++i]; continue; }
    a.push_back(s);
  }

  // --set_ros_master_uri / --set_ros_hostname: remember, then exit.
  if (!set_uri.empty() || !set_host.empty()) {
    if (!set_uri.empty()) set_uri = resolve_master(set_uri, port);  // normalise
    return save_master_yaml(set_uri, set_host) ? 0 : 1;
  }
  if (a.empty()) { usage(); return 0; }

  // precedence: --master/--host > ./master.yaml > $ROS_MASTER_URI/$ROS_IP > default
  std::string yaml_uri, yaml_host;
  load_master_yaml(&yaml_uri, &yaml_host);
  const char* env_master = std::getenv("ROS_MASTER_URI");
  const char* env_host = std::getenv("ROS_HOSTNAME");
  const char* env_ip = std::getenv("ROS_IP");

  std::string origin = "the default";
  if (!cli_master.empty())      { g_master = cli_master;  origin = "--master"; }
  else if (!yaml_uri.empty())   { g_master = yaml_uri;    origin = "./master.yaml"; }
  else if (env_master && *env_master) { g_master = env_master; origin = "$ROS_MASTER_URI"; }
  g_master = resolve_master(g_master, port);
  g_master_origin = origin;

  std::string host = "localhost";
  if (!cli_host.empty()) host = cli_host;
  else if (!yaml_host.empty()) host = yaml_host;
  else if (env_host && *env_host) host = env_host;
  else if (env_ip && *env_ip) host = env_ip;

  set_master_uri(g_master);
  set_hostname(host);
  set_log_level("warn");     // a CLI prints its data, not the library's chatter

  const std::string& cmd = a[0];
  auto has = [&](const std::string& f) {
    for (size_t i = 1; i < a.size(); ++i) if (a[i] == f) return true;
    return false;
  };
  auto positional = [&](size_t n) -> std::string {
    size_t seen = 0;
    for (size_t i = 1; i < a.size(); ++i) {
      if (a[i].size() && a[i][0] == '-') {
        if (a[i] == "-n" || a[i] == "-r") ++i;      // skip its value
        continue;
      }
      if (seen++ == n) return a[i];
    }
    return "";
  };
  auto opt_val = [&](const std::string& f, const std::string& dflt) {
    for (size_t i = 1; i + 1 < a.size(); ++i) if (a[i] == f) return a[i + 1];
    return dflt;
  };

  if (cmd == "list") return cmd_list(has("-v") || has("--verbose"));
  if (cmd == "type") return cmd_type(positional(0));
  if (cmd == "info") return cmd_info(positional(0));
  if (cmd == "find") return cmd_find(positional(0));
  if (cmd == "echo")
    return cmd_echo(positional(0), std::atoi(opt_val("-n", "0").c_str()), has("--noarr"));
  if (cmd == "hz") return cmd_hz(positional(0));
  if (cmd == "bw") return cmd_bw(positional(0));
  if (cmd == "pub")
    return cmd_pub(positional(0), positional(1), positional(2),
                   std::atof(opt_val("-r", "0").c_str()), has("-1"));

  usage();
  return 1;
}
