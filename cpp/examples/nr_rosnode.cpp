// nr_rosnode -- `rosnode`, without ROS installed.
//
//   nr_rosnode list [-a] [-u]
//   nr_rosnode info   /talker
//   nr_rosnode ping [-c N] [--all] /talker
//   nr_rosnode machine [HOST]
//   nr_rosnode kill   /talker [/talker2 ...]
//   nr_rosnode cleanup
//
// Node names and their topics/services come from the master (getSystemState).
// pid / connections / liveness / shutdown come from calling each node's own
// slave API directly (getPid / getBusInfo / shutdown), exactly as rosnode does.
//
// Shares ./master.yaml with nr_rostopic. Master: $ROS_MASTER_URI, or --master.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "irap_noroslib.hpp"

using namespace irap_noroslib;

static std::string g_master = "http://localhost:11311";
static std::string g_master_origin = "the default";
static const char* CALLER = "/nr_rosnode";
static const double kPingTimeout = 3.0;

// ------------------------------------------------------------------ master ---
static void unreachable(const std::string& err) {
  fprintf(stderr,
          "nr_rosnode: cannot reach the ROS master at %s\n"
          "            (from %s)\n            %s\n\n"
          "Is a master running? Start one with:  nr_roscore\n",
          g_master.c_str(), g_master_origin.c_str(), err.c_str());
}

static bool system_state(GraphMap* pubs, GraphMap* subs, GraphMap* srvs) {
  std::string err;
  if (!get_system_state(g_master, CALLER, pubs, subs, srvs, &err)) {
    unreachable(err);
    return false;
  }
  return true;
}

static std::vector<std::string> all_nodes() {
  GraphMap pubs, subs, srvs;
  std::set<std::string> names;
  if (system_state(&pubs, &subs, &srvs))
    for (const GraphMap* g : {&pubs, &subs, &srvs})
      for (const auto& e : *g)
        for (const auto& n : e.second) names.insert(n);
  return {names.begin(), names.end()};
}

static std::string node_api(const std::string& name) {
  std::string uri, err;
  if (!lookup_node(g_master, CALLER, name, &uri, &err)) return "";
  return uri;
}

// A slave-API call (getPid / getBusInfo / shutdown). Returns the value element
// of the [code, status, value] reply, or false on any transport/fault error.
static bool slave_call(const std::string& uri, const std::string& method,
                       const std::vector<XmlValue>& extra, XmlValue* value) {
  std::vector<XmlValue> params = {XmlValue::Str(CALLER)};
  for (const auto& e : extra) params.push_back(e);
  XmlValue result;
  std::string err;
  if (!xmlrpc_call(uri, method, params, &result, &err)) return false;
  if (result.type != XmlValue::Type::Array || result.arr.size() < 3) return false;
  if (result.arr[0].as_int() != 1) return false;
  *value = result.arr[2];
  return true;
}

static bool node_alive(const std::string& uri) {
  XmlValue v;
  return !uri.empty() && slave_call(uri, "getPid", {}, &v);
}

// topics/services this node provides, split by section
static void node_regs(const std::string& name, std::vector<std::string>* pubs,
                      std::vector<std::string>* subs, std::vector<std::string>* srvs) {
  GraphMap gp, gs, gk;
  if (!system_state(&gp, &gs, &gk)) return;
  auto owned = [&](const GraphMap& g, std::vector<std::string>* out) {
    for (const auto& e : g)
      if (std::find(e.second.begin(), e.second.end(), name) != e.second.end())
        out->push_back(e.first);
    std::sort(out->begin(), out->end());
  };
  owned(gp, pubs); owned(gs, subs); owned(gk, srvs);
}

static std::map<std::string, std::string> topic_types() {
  std::vector<std::pair<std::string, std::string>> ts;
  std::string err;
  get_topic_types(g_master, CALLER, &ts, &err);
  return {ts.begin(), ts.end()};
}

// ---------------------------------------------------------------- commands ---
static int cmd_list(bool all, bool url) {
  std::vector<std::string> nodes = all_nodes();
  for (const auto& n : nodes) {
    if (all || url) {
      std::string uri = node_api(n);
      if (uri.empty()) uri = "(unknown)";
      if (url && !all) printf("%s\n", uri.c_str());
      else printf("%-30s %s\n", n.c_str(), uri.c_str());
    } else {
      printf("%s\n", n.c_str());
    }
  }
  return 0;
}

static int cmd_info(const std::string& name) {
  std::vector<std::string> nodes = all_nodes();
  if (std::find(nodes.begin(), nodes.end(), name) == nodes.end()) {
    fprintf(stderr, "ERROR: Unknown node %s\n", name.c_str());
    return 1;
  }
  std::map<std::string, std::string> types = topic_types();
  std::vector<std::string> pubs, subs, srvs;
  node_regs(name, &pubs, &subs, &srvs);
  printf("%s\nNode [%s]\n", std::string(80, '-').c_str(), name.c_str());
  printf("\nPublications: \n");
  for (const auto& t : pubs)
    printf(" * %s [%s]\n", t.c_str(), types.count(t) ? types[t].c_str() : "unknown type");
  if (pubs.empty()) printf(" * None\n");
  printf("\nSubscriptions: \n");
  for (const auto& t : subs)
    printf(" * %s [%s]\n", t.c_str(), types.count(t) ? types[t].c_str() : "unknown type");
  if (subs.empty()) printf(" * None\n");
  printf("\nServices: \n");
  for (const auto& t : srvs) printf(" * %s\n", t.c_str());
  if (srvs.empty()) printf(" * None\n");
  printf("\n");

  std::string uri = node_api(name);
  if (uri.empty()) { printf("cannot contact [%s]: no API uri\n", name.c_str()); return 0; }
  printf("contacting node %s ...\n", uri.c_str());
  XmlValue pid;
  if (!slave_call(uri, "getPid", {}, &pid)) {
    printf("cannot contact [%s]\n\n", name.c_str());
    return 0;
  }
  printf("Pid: %lld\n", (long long)pid.as_int());
  XmlValue businfo;
  printf("Connections:\n");
  if (slave_call(uri, "getBusInfo", {}, &businfo) &&
      businfo.type == XmlValue::Type::Array && !businfo.arr.empty()) {
    for (const auto& c : businfo.arr) {
      const auto& a = c.arr;
      std::string dest = a.size() > 1 ? a[1].as_str() : "?";
      std::string dir = a.size() > 2 ? a[2].as_str() : "?";
      std::string topic = a.size() > 4 ? a[4].as_str() : "?";
      printf(" * topic: %s\n    * to: %s\n    * direction: %s\n",
             topic.c_str(), dest.c_str(), dir.c_str());
    }
  } else {
    printf(" * None\n");
  }
  printf("\n");
  return 0;
}

static int cmd_ping(const std::string& node, int count, bool all) {
  std::vector<std::string> targets = all ? all_nodes() : std::vector<std::string>{node};
  int rc = 0;
  for (const auto& name : targets) {
    std::string uri = node_api(name);
    if (uri.empty()) {
      fprintf(stderr, "ERROR: node [%s] is unknown or unreachable\n", name.c_str());
      rc = 1;
      continue;
    }
    printf("rosnode: node is [%s]\n", name.c_str());
    printf("pinging %s with a timeout of %.1fs\n", name.c_str(), kPingTimeout);
    int n = 0;
    double acc = 0;
    while (count == 0 || n < count) {
      auto t0 = std::chrono::steady_clock::now();
      XmlValue v;
      if (!slave_call(uri, "getPid", {}, &v)) {
        printf("connection to [%s] timed out\n", name.c_str());
        rc = 1;
        break;
      }
      double ms = std::chrono::duration<double, std::milli>(
                      std::chrono::steady_clock::now() - t0).count();
      acc += ms;
      ++n;
      printf("xmlrpc reply from %s\ttime=%.6fms\n", uri.c_str(), ms);
      if (count == 0 || n < count) std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    if (n) printf("\nping average: %.6fms\n", acc / n);
  }
  return rc;
}

static int cmd_machine(const std::string& machine) {
  std::map<std::string, std::vector<std::string>> by_host;
  for (const auto& n : all_nodes()) {
    std::string uri = node_api(n), host = "(unknown)";
    uint16_t port = 0;
    if (!uri.empty()) parse_http_uri(uri, &host, &port);
    by_host[host].push_back(n);
  }
  if (!machine.empty()) {
    auto it = by_host.find(machine);
    if (it != by_host.end()) {
      std::sort(it->second.begin(), it->second.end());
      for (const auto& n : it->second) printf("%s\n", n.c_str());
    }
  } else {
    for (const auto& kv : by_host) printf("%s\n", kv.first.c_str());
  }
  return 0;
}

static int cmd_kill(const std::vector<std::string>& names, bool all) {
  std::vector<std::string> targets = all ? all_nodes() : names;
  int killed = 0;
  for (const auto& name : targets) {
    std::string uri = node_api(name);
    if (uri.empty()) {
      fprintf(stderr, "ERROR: Unknown node(s) specified: %s\n", name.c_str());
      continue;
    }
    // A node often drops the connection as it shuts down, before it can reply,
    // so we ignore the call result and check whether it is actually gone.
    XmlValue v;
    slave_call(uri, "shutdown", {XmlValue::Str("kill requested via nr_rosnode")}, &v);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    if (node_alive(uri)) {
      fprintf(stderr, "ERROR: could not kill %s (still alive)\n", name.c_str());
    } else {
      printf("killed %s\n", name.c_str());
      ++killed;
    }
  }
  return killed ? 0 : 1;
}

static int cmd_cleanup() {
  GraphMap pubs, subs, srvs;
  if (!system_state(&pubs, &subs, &srvs)) return 1;
  std::vector<std::pair<std::string, std::string>> dead;   // (node, uri)
  for (const auto& name : all_nodes()) {
    std::string uri = node_api(name);
    if (uri.empty() || !node_alive(uri)) dead.emplace_back(name, uri);
  }
  if (dead.empty()) { printf("No unreachable nodes to purge.\n"); return 0; }
  printf("Unable to contact the following nodes:\n");
  for (const auto& d : dead) printf(" * %s\n", d.first.c_str());
  printf("\nUnregistering their topics and services from the master ...\n");
  std::string err;
  for (const auto& d : dead) {
    const std::string& name = d.first;
    const std::string& api = d.second;
    for (const auto& e : pubs)
      if (std::find(e.second.begin(), e.second.end(), name) != e.second.end())
        unregister_publisher(g_master, name, e.first, api, &err);
    for (const auto& e : subs)
      if (std::find(e.second.begin(), e.second.end(), name) != e.second.end())
        unregister_subscriber(g_master, name, e.first, api, &err);
    for (const auto& e : srvs)
      if (std::find(e.second.begin(), e.second.end(), name) != e.second.end()) {
        std::string svc_api;
        if (!lookup_service(g_master, CALLER, e.first, &svc_api, &err)) svc_api = api;
        unregister_service(g_master, name, e.first, svc_api, &err);
      }
  }
  printf("done.\n");
  return 0;
}

// -------------------------------------------------------------------- main ---
static void usage() {
  printf(
      "nr_rosnode -- rosnode, with no ROS installed\n\n"
      "  nr_rosnode list [-a] [-u]\n"
      "  nr_rosnode info    NODE\n"
      "  nr_rosnode ping [-c N] [--all] NODE\n"
      "  nr_rosnode machine [HOST]\n"
      "  nr_rosnode kill    NODE [NODE ...] | --all\n"
      "  nr_rosnode cleanup\n\n"
      "  --master HOST  master host, IP, host:port or full URI\n"
      "  --port N       master port (default: 11311)\n"
      "  --host HOST    our hostname\n\n"
      "Remember the master once, in ./master.yaml (shared with nr_rostopic):\n"
      "  nr_rosnode --set_ros_master_uri http://HOST:11311 --set_ros_hostname YOUR_IP\n");
}

// --------------------------------------------------- remembered settings ----
// Shares ./master.yaml with nr_rostopic; see nr_rostopic.cpp for the rationale.
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
  if (!f) { fprintf(stderr, "nr_rosnode: cannot write %s here\n", MASTER_YAML); return false; }
  f << "# nr_rostopic remembered settings. Delete this file to forget them.\n"
    << "# These win over $ROS_MASTER_URI / $ROS_IP / $ROS_HOSTNAME.\n";
  if (!cur_uri.empty()) f << "ros_master_uri: " << cur_uri << "\n";
  if (!cur_host.empty()) f << "ros_hostname: " << cur_host << "\n";
  f.close();
  printf("saved to ./%s\n", MASTER_YAML);
  if (!cur_uri.empty()) printf("  ros_master_uri: %s\n", cur_uri.c_str());
  if (!cur_host.empty()) printf("  ros_hostname:   %s\n", cur_host.c_str());
  printf("\nnr_rosnode in this directory now uses these -- no flags needed.\n");
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
      if (a[i].size() && a[i][0] == '-') { if (a[i] == "-c") ++i; continue; }
      if (seen++ == n) return a[i];
    }
    return "";
  };
  auto positionals = [&]() {
    std::vector<std::string> out;
    for (size_t i = 1; i < a.size(); ++i)
      if (a[i].empty() || a[i][0] != '-') out.push_back(a[i]);
    return out;
  };
  auto opt_val = [&](const std::string& f, const std::string& dflt) {
    for (size_t i = 1; i + 1 < a.size(); ++i) if (a[i] == f) return a[i + 1];
    return dflt;
  };

  if (cmd == "list") return cmd_list(has("-a") || has("--all"), has("-u") || has("--url"));
  if (cmd == "info") return cmd_info(positional(0));
  if (cmd == "ping") {
    bool all = has("-a") || has("--all");
    std::string node = positional(0);
    if (node.empty() && !all) {
      fprintf(stderr, "nr_rosnode ping: give a NODE, or --all\n");
      return 1;
    }
    return cmd_ping(node, std::atoi(opt_val("-c", "0").c_str()), all);
  }
  if (cmd == "machine") return cmd_machine(positional(0));
  if (cmd == "kill") {
    bool all = has("-a") || has("--all");
    std::vector<std::string> nodes = positionals();
    if (nodes.empty() && !all) {
      fprintf(stderr, "nr_rosnode kill: give one or more NODEs, or --all\n");
      return 1;
    }
    return cmd_kill(nodes, all);
  }
  if (cmd == "cleanup") return cmd_cleanup();

  usage();
  return 1;
}
