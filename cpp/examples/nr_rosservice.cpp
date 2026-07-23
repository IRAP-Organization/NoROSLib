// nr_rosservice -- `rosservice`, without ROS installed.
//
//   nr_rosservice list [-v]
//   nr_rosservice type  /add_two_ints
//   nr_rosservice uri   /add_two_ints
//   nr_rosservice info  /add_two_ints
//   nr_rosservice find  rospy_tutorials/AddTwoInts
//   nr_rosservice args  /add_two_ints
//   nr_rosservice call [-f Type.srv] /add_two_ints "a: 3, b: 4"
//
// `type`/`uri`/`info`/`find` work for ANY service: the master does not record a
// service's type, so nr_rosservice probes the running service directly (the same
// handshake `rosservice` uses). `args`/`call` need the request's field layout,
// which a service does NOT send over the wire -- built-ins (std_srvs) and types
// loaded with load_srv_file() just work; for anything else, point it at the .srv
// file with -f. It shares ./master.yaml with nr_rostopic.
//
// Master: $ROS_MASTER_URI, or --master http://host:11311
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "irap_noroslib.hpp"

using namespace irap_noroslib;

static std::string g_master = "http://localhost:11311";
static std::string g_master_origin = "the default";
static const char* CALLER = "/nr_rosservice";

// ------------------------------------------------------------------ output ---
static const size_t kArrayElide = 12;

static std::string fmt_value(const Value& v, int indent);

static std::string fmt_msg(const DynamicMessage& m, int indent) {
  std::string pad(indent * 2, ' ');
  std::string out;
  size_t slot = 0;
  for (const Field& f : m.spec().fields()) {
    if (f.is_constant) continue;
    const Value& v = m.values()[slot++];
    if (v.kind() == Value::MSG) {
      out += pad + f.name + ": \n" + fmt_msg(v.msg(), indent + 1);
    } else if (v.kind() == Value::ARRAY && !v.array().empty() &&
               v.array()[0].kind() == Value::MSG) {
      out += pad + f.name + ": \n";
      for (const Value& el : v.array())
        out += pad + "  - \n" + fmt_msg(el.msg(), indent + 2);
    } else {
      out += pad + f.name + ": " + fmt_value(v, indent) + "\n";
    }
  }
  return out;
}

static std::string fmt_value(const Value& v, int indent) {
  std::string pad(indent * 2, ' ');
  switch (v.kind()) {
    case Value::BYTES: {
      const auto& b = v.bytes();
      if (b.size() > kArrayElide) return "<" + std::to_string(b.size()) + " bytes>";
      std::string s = "[";
      for (size_t i = 0; i < b.size(); ++i)
        s += (i ? ", " : "") + std::to_string((int)b[i]);
      return s + "]";
    }
    case Value::ARRAY: {
      const auto& a = v.array();
      if (a.size() > kArrayElide) return "<array of " + std::to_string(a.size()) + ">";
      std::string s = "[";
      for (size_t i = 0; i < a.size(); ++i) s += (i ? ", " : "") + a[i].str();
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
static void unreachable(const std::string& err) {
  fprintf(stderr,
          "nr_rosservice: cannot reach the ROS master at %s\n"
          "               (from %s)\n"
          "               %s\n\n"
          "Is a master running? Start one with:  nr_roscore\n"
          "Point at a different one, and remember it here:\n"
          "    nr_rosservice --set_ros_master_uri http://HOST:11311 "
          "--set_ros_hostname YOUR_IP\n",
          g_master.c_str(), g_master_origin.c_str(), err.c_str());
}

static bool services(GraphMap* out) {
  GraphMap pubs, subs;
  std::string err;
  if (!get_system_state(g_master, CALLER, &pubs, &subs, out, &err)) {
    unreachable(err);
    return false;
  }
  return true;
}

// Probe a running service for its reply header (type / md5 / request_type ...).
static bool probe(const std::string& service, std::map<std::string, std::string>* hdr) {
  init_node("nr_rosservice");
  std::string err;
  if (!detail::probe_service(service, hdr, &err)) {
    fprintf(stderr, "nr_rosservice: cannot probe %s: %s\n", service.c_str(), err.c_str());
    return false;
  }
  return true;
}

// Turn a service type into request/response codecs: a .srv file (-f) wins, else
// whatever is registered (std_srvs, or anything already load_srv_file()'d).
static bool service_type(const std::string& type, const std::string& srv_file, SrvType* out) {
  if (!srv_file.empty()) { *out = load_srv_file(srv_file); return true; }
  if (has_srv_type(type)) { *out = get_srv_type(type); return true; }
  return false;
}

static std::vector<std::string> request_args(const std::string& type,
                                             const std::string& srv_file) {
  SrvType srv;
  std::vector<std::string> names;
  if (!service_type(type, srv_file, &srv)) return names;
  DynamicMessage req = srv.request().create();
  for (const Field& f : req.spec().fields())
    if (!f.is_constant) names.push_back(f.name);
  return names;
}

// ---------------------------------------------------------------- commands ---
static int cmd_list(bool verbose) {
  GraphMap srvs;
  if (!services(&srvs)) return 1;
  std::map<std::string, std::vector<std::string>> sorted(srvs.begin(), srvs.end());
  for (const auto& s : sorted) {
    if (!verbose) { printf("%s\n", s.first.c_str()); continue; }
    std::string nodes;
    for (size_t i = 0; i < s.second.size(); ++i) nodes += (i ? ", " : "") + s.second[i];
    printf(" * %s [%s]\n", s.first.c_str(), nodes.empty() ? "?" : nodes.c_str());
  }
  return 0;
}

static int cmd_type(const std::string& service) {
  std::map<std::string, std::string> hdr;
  if (!probe(service, &hdr)) return 1;
  printf("%s\n", hdr["type"].c_str());
  return 0;
}

static int cmd_uri(const std::string& service) {
  std::string url, err;
  if (!lookup_service(g_master, CALLER, service, &url, &err)) {
    fprintf(stderr, "nr_rosservice: %s\n", err.c_str());
    return 1;
  }
  printf("%s\n", url.c_str());
  return 0;
}

static int cmd_info(const std::string& service, const std::string& srv_file) {
  GraphMap srvs;
  if (!services(&srvs)) return 1;
  std::vector<std::string> nodes;
  bool found = false;
  for (const auto& s : srvs) if (s.first == service) { nodes = s.second; found = true; }
  if (!found) { fprintf(stderr, "Unknown service %s\n", service.c_str()); return 1; }
  std::map<std::string, std::string> hdr;
  if (!probe(service, &hdr)) return 1;
  std::string url, err;
  lookup_service(g_master, CALLER, service, &url, &err);
  std::string nl;
  for (size_t i = 0; i < nodes.size(); ++i) nl += (i ? ", " : "") + nodes[i];
  printf("Node: %s\n", nl.empty() ? "?" : nl.c_str());
  printf("URI: %s\n", url.c_str());
  printf("Type: %s\n", hdr["type"].c_str());
  std::string args;
  for (const auto& a : request_args(hdr["type"], srv_file)) args += (args.empty() ? "" : " ") + a;
  printf("Args: %s\n", args.c_str());
  return 0;
}

static int cmd_find(const std::string& type) {
  GraphMap srvs;
  if (!services(&srvs)) return 1;
  std::map<std::string, std::vector<std::string>> sorted(srvs.begin(), srvs.end());
  init_node("nr_rosservice");
  for (const auto& s : sorted) {
    std::map<std::string, std::string> hdr;
    std::string err;
    if (detail::probe_service(s.first, &hdr, &err) && hdr["type"] == type)
      printf("%s\n", s.first.c_str());
  }
  return 0;
}

static int cmd_args(const std::string& service, const std::string& srv_file) {
  std::map<std::string, std::string> hdr;
  if (!probe(service, &hdr)) return 1;
  std::vector<std::string> names = request_args(hdr["type"], srv_file);
  if (names.empty() && srv_file.empty())
    fprintf(stderr, "nr_rosservice: unknown request layout for [%s]; give -f Type.srv\n",
            hdr["type"].c_str());
  std::string out;
  for (const auto& a : names) out += (out.empty() ? "" : " ") + a;
  printf("%s\n", out.c_str());
  return 0;
}

// -- a tiny YAML-flow parser: {a: 1, b: {c: 2}, d: [1,2]} -> dotted set() calls --
static void skip_ws(const std::string& s, size_t* i) {
  while (*i < s.size() && (s[*i] == ' ' || s[*i] == '\t' || s[*i] == '\n' || s[*i] == ','))
    ++*i;
}

static void assign(DynamicMessage& m, const std::string& path, const std::string& tok) {
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
  if (s[*i] == '[') {
    ++*i;
    std::vector<std::string> toks;
    skip_ws(s, i);
    while (*i < s.size() && s[*i] != ']') { toks.push_back(read_token(s, i)); skip_ws(s, i); }
    ++*i;
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
  ++*i;
  skip_ws(s, i);
  while (*i < s.size() && s[*i] != '}') {
    size_t start = *i;
    while (*i < s.size() && s[*i] != ':') ++*i;
    std::string key = s.substr(start, *i - start);
    while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.pop_back();
    size_t a = key.find_first_not_of(" \t\"'");
    size_t b = key.find_last_not_of(" \t\"'");
    key = key.substr(a, b - a + 1);
    ++*i;
    parse_value(s, i, m, prefix + key);
    skip_ws(s, i);
  }
  ++*i;
}

static int cmd_call(const std::string& service, const std::string& data,
                    const std::string& srv_file) {
  std::map<std::string, std::string> hdr;
  if (!probe(service, &hdr)) return 1;
  std::string type = hdr["type"];
  SrvType srv;
  if (!service_type(type, srv_file, &srv)) {
    fprintf(stderr,
            "nr_rosservice: don't know the request layout for [%s].\n"
            "               A service doesn't send its .srv over the wire, so\n"
            "               point me at the file:  -f path/to/Type.srv\n"
            "               (std_srvs and already-loaded types need no file.)\n",
            type.c_str());
    return 1;
  }
  DynamicServiceClient client(service, srv);
  DynamicMessage req = client.request();
  if (!data.empty()) {
    std::string d = data;
    if (d[0] != '{') d = "{" + d + "}";
    size_t i = 0;
    try {
      parse_into(d, &i, req, "");
    } catch (const std::exception& e) {
      fprintf(stderr, "could not parse the request data: %s\n", e.what());
      return 1;
    }
  }
  DynamicMessage resp;
  if (!client.call(req, resp)) {
    fprintf(stderr, "nr_rosservice: call failed\n");
    return 1;
  }
  printf("%s", fmt_msg(resp, 0).c_str());
  return 0;
}

// -------------------------------------------------------------------- main ---
static void usage() {
  printf(
      "nr_rosservice -- rosservice, with no ROS installed\n\n"
      "  nr_rosservice list [-v]\n"
      "  nr_rosservice type  SERVICE\n"
      "  nr_rosservice uri   SERVICE\n"
      "  nr_rosservice info  [-f Type.srv] SERVICE\n"
      "  nr_rosservice find  TYPE\n"
      "  nr_rosservice args  [-f Type.srv] SERVICE\n"
      "  nr_rosservice call  [-f Type.srv] SERVICE \"a: 3, b: 4\"\n\n"
      "  --master HOST  master host, IP, host:port or full URI\n"
      "  --port N       master port (default: 11311)\n"
      "  --host HOST    our hostname -- how other nodes reach us\n\n"
      "Remember the master once, in ./master.yaml (shared with nr_rostopic):\n"
      "  nr_rosservice --set_ros_master_uri http://HOST:11311 --set_ros_hostname YOUR_IP\n\n"
      "type/uri/info/find probe any running service. args/call need the .srv (-f),\n"
      "unless it's a built-in (std_srvs) or already loaded with load_srv_file().\n");
}

// ------------------------------------------------- remembered settings ----
// Shares ./master.yaml with nr_rostopic: set the master once, both tools use it.
// The file WINS over $ROS_MASTER_URI / $ROS_IP / $ROS_HOSTNAME; an explicit
// --master/--port/--host still beats the file.
static const char* MASTER_YAML = "master.yaml";

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
  load_master_yaml(&cur_uri, &cur_host);
  if (!uri.empty()) cur_uri = uri;
  if (!host.empty()) cur_host = host;
  std::ofstream f(MASTER_YAML);
  if (!f) { fprintf(stderr, "nr_rosservice: cannot write %s here\n", MASTER_YAML); return false; }
  f << "# nr_rostopic remembered settings. Delete this file to forget them.\n"
    << "# These win over $ROS_MASTER_URI / $ROS_IP / $ROS_HOSTNAME.\n";
  if (!cur_uri.empty()) f << "ros_master_uri: " << cur_uri << "\n";
  if (!cur_host.empty()) f << "ros_hostname: " << cur_host << "\n";
  f.close();
  printf("saved to ./%s\n", MASTER_YAML);
  if (!cur_uri.empty()) printf("  ros_master_uri: %s\n", cur_uri.c_str());
  if (!cur_host.empty()) printf("  ros_hostname:   %s\n", cur_host.c_str());
  printf("\nnr_rosservice in this directory now uses these -- no flags needed.\n");
  return true;
}

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

  if (!set_uri.empty() || !set_host.empty()) {
    if (!set_uri.empty()) set_uri = resolve_master(set_uri, port);
    return save_master_yaml(set_uri, set_host) ? 0 : 1;
  }
  if (a.empty()) { usage(); return 0; }

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
  set_log_level("warn");

  const std::string& cmd = a[0];
  auto has = [&](const std::string& f) {
    for (size_t i = 1; i < a.size(); ++i) if (a[i] == f) return true;
    return false;
  };
  auto positional = [&](size_t n) -> std::string {
    size_t seen = 0;
    for (size_t i = 1; i < a.size(); ++i) {
      if (a[i].size() && a[i][0] == '-') {
        if (a[i] == "-f") ++i;                    // skip its value
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
  if (cmd == "uri") return cmd_uri(positional(0));
  if (cmd == "info") return cmd_info(positional(0), opt_val("-f", ""));
  if (cmd == "find") return cmd_find(positional(0));
  if (cmd == "args") return cmd_args(positional(0), opt_val("-f", ""));
  if (cmd == "call") return cmd_call(positional(0), positional(1), opt_val("-f", ""));

  usage();
  return 1;
}
