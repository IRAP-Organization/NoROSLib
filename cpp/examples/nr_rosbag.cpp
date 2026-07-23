// nr_rosbag -- `rosbag`, without ROS installed.
//
//   nr_rosbag record [-a] [-O NAME] [-o PREFIX] [--split --duration S | --size MB] [topic ...]
//   nr_rosbag play [-r RATE] [-l] [-d SEC] bag [bag ...]
//   nr_rosbag info bag [bag ...]
//
// record captures the exact wire bytes of ANY topic -- even a custom type you
// have no .msg file for -- saving the publisher's message_definition alongside,
// exactly as real rosbag does. Bags are the real v2.0 format, so they open in
// genuine rosbag info / play / rqt_bag, and a real-ROS bag plays back here.
//
// Shares ./master.yaml with nr_rostopic. Master: $ROS_MASTER_URI, or --master.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "irap_noroslib.hpp"

using namespace irap_noroslib;

static std::string g_master = "http://localhost:11311";
static const char* CALLER = "/nr_rosbag";
static std::atomic<bool> g_stop{false};

static void on_term(int) { g_stop.store(true); }

// ------------------------------------------------------------------ record ---
struct RecordOpts {
  bool all = false;
  std::string output_name, output_prefix;
  bool split = false;
  double duration = 0;      // seconds per bag
  double size_mb = 0;       // MB per bag
  std::vector<std::string> topics;
};

static std::string timestamp() {
  std::time_t t = std::time(nullptr);
  char buf[32];
  std::strftime(buf, sizeof(buf), "%Y-%m-%d-%H-%M-%S", std::localtime(&t));
  return buf;
}

static std::string record_name(const RecordOpts& o, int index) {
  std::string base;
  if (!o.output_name.empty()) {
    base = o.output_name;
    if (base.size() > 4 && base.substr(base.size() - 4) == ".bag")
      base = base.substr(0, base.size() - 4);
  } else {
    base = (o.output_prefix.empty() ? "" : o.output_prefix + "_") + timestamp();
  }
  if (index >= 0) base += "_" + std::to_string(index);
  return base + ".bag";
}

static int cmd_record(const RecordOpts& o) {
  if (!o.all && o.topics.empty()) {
    fprintf(stderr, "nr_rosbag record: give topics, or -a/--all\n");
    return 1;
  }
  init_node("nr_rosbag");
  std::signal(SIGTERM, on_term);

  std::mutex mu;
  std::unique_ptr<BagWriter> writer;
  std::map<std::string, int> conns;       // topic -> conn id in current bag
  std::set<std::string> subscribed;
  std::vector<std::shared_ptr<detail::Subscription>> subs;
  size_t bag_bytes = 0, total = 0;
  int index = 0;
  auto opened = std::chrono::steady_clock::now();
  size_t split_bytes = (size_t)(o.size_mb * 1024 * 1024);

  auto open_bag = [&]() {
    std::string name = record_name(o, o.split ? index : -1);
    writer.reset(new BagWriter(name));
    conns.clear();
    bag_bytes = 0;
    opened = std::chrono::steady_clock::now();
    printf("Recording to '%s'.\n", name.c_str());
  };

  auto maybe_roll = [&]() {
    if (!o.split) return;
    bool big = split_bytes && bag_bytes >= split_bytes;
    double age = std::chrono::duration<double>(
                     std::chrono::steady_clock::now() - opened).count();
    bool old = o.duration && age >= o.duration;
    if (big || old) { writer->close(); ++index; open_bag(); }
  };

  auto subscribe_topic = [&](const std::string& topic) {
    if (subscribed.count(topic)) return;
    subscribed.insert(topic);
    subs.push_back(detail::subscribe_any(
        topic, [&, topic](const std::string& type, const std::string& md5,
                          const std::string& def, const std::vector<uint8_t>& body) {
          int64_t s, n;
          wall_time(&s, &n);
          std::lock_guard<std::mutex> lk(mu);
          auto it = conns.find(topic);
          if (it == conns.end()) {
            int cid = writer->connection(topic, type, md5, def, CALLER);
            conns[topic] = cid;
            if (index == 0) printf("Subscribed to [%s]\n", topic.c_str());
            it = conns.find(topic);
          }
          writer->write(it->second, (uint32_t)s, (uint32_t)n, body);
          bag_bytes += body.size();
          ++total;
          maybe_roll();
        }));
  };

  open_bag();
  if (!o.all)
    for (const auto& t : o.topics) subscribe_topic(t[0] == '/' ? t : "/" + t);

  while (!g_stop.load() && ok()) {
    if (o.all) {
      std::vector<std::pair<std::string, std::string>> ts;
      std::string err;
      if (get_topic_types(g_master, CALLER, &ts, &err))
        for (const auto& t : ts) subscribe_topic(t.first);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }
  {
    std::lock_guard<std::mutex> lk(mu);
    writer->close();
  }
  printf("\nDone. Wrote %zu messages%s.\n", total,
         o.split ? (" across " + std::to_string(index + 1) + " bags").c_str() : "");
  return 0;
}

// -------------------------------------------------------------------- play ---
struct PlayItem {
  uint32_t secs, nsecs;
  std::string topic, type, md5, def;
  std::vector<uint8_t> body;
};

static int cmd_play(const std::vector<std::string>& bags, double rate, bool loop,
                    double delay) {
  std::vector<PlayItem> items;
  for (const auto& path : bags) {
    std::ifstream test(path);
    if (!test) { fprintf(stderr, "nr_rosbag: no such bag '%s'\n", path.c_str()); return 1; }
    BagReader r(path);
    r.read_messages([&](const BagMessage& m, const BagConnection& c) {
      items.push_back({m.secs, m.nsecs, m.topic, c.type, c.md5, c.definition, m.data});
    });
  }
  if (items.empty()) { printf("nr_rosbag: nothing to play\n"); return 0; }
  std::sort(items.begin(), items.end(), [](const PlayItem& a, const PlayItem& b) {
    return a.secs != b.secs ? a.secs < b.secs : a.nsecs < b.nsecs;
  });

  init_node("nr_rosbag");
  std::signal(SIGTERM, on_term);
  std::map<std::string, std::shared_ptr<detail::Publication>> pubs;
  for (const auto& it : items) {
    if (pubs.count(it.topic)) continue;
    pubs[it.topic] = detail::advertise(it.topic, it.type, it.md5, it.def, false);
  }
  if (rate <= 0) rate = 1.0;
  printf("Waiting %.1fs for subscribers...\n", delay);
  std::this_thread::sleep_for(std::chrono::duration<double>(delay));

  int loops = 0;
  do {
    printf("Playing %zu messages from %zu bag(s)%s\n", items.size(), bags.size(),
           loop ? (" (loop " + std::to_string(loops + 1) + ")").c_str() : "");
    double t0 = items[0].secs + items[0].nsecs * 1e-9;
    auto wall0 = std::chrono::steady_clock::now();
    for (const auto& it : items) {
      if (g_stop.load() || !ok()) break;
      double target = ((it.secs + it.nsecs * 1e-9) - t0) / rate;
      double now = std::chrono::duration<double>(
                       std::chrono::steady_clock::now() - wall0).count();
      if (target > now)
        std::this_thread::sleep_for(std::chrono::duration<double>(target - now));
      detail::publish_raw(*pubs[it.topic], it.body);
    }
    ++loops;
  } while (loop && !g_stop.load() && ok());
  printf("Done.\n");
  return 0;
}

// -------------------------------------------------------------------- info ---
static std::string fmt_dur(double s) {
  char b[64];
  if (s < 60) snprintf(b, sizeof(b), "%.1fs", s);
  else snprintf(b, sizeof(b), "%d:%04.1fs (%.1fs)", (int)(s / 60), std::fmod(s, 60.0), s);
  return b;
}
static std::string fmt_time(double t) {
  std::time_t tt = (std::time_t)t;
  char b[64];
  std::strftime(b, sizeof(b), "%b %d %Y %H:%M:%S", std::localtime(&tt));
  char out[96];
  snprintf(out, sizeof(out), "%s.%02d", b, (int)((t - (double)tt) * 100));
  return out;
}
static std::string fmt_size(double n) {
  const char* u[] = {"B", "KB", "MB", "GB"};
  int i = 0;
  while (n >= 1024 && i < 3) { n /= 1024; ++i; }
  char b[48];
  snprintf(b, sizeof(b), "%.1f %s", n, u[i]);
  return b;
}

static int cmd_info(const std::vector<std::string>& bags) {
  int rc = 0;
  for (const auto& path : bags) {
    std::ifstream test(path);
    if (!test) { fprintf(stderr, "nr_rosbag: no such bag '%s'\n", path.c_str()); rc = 1; continue; }
    BagReader r(path);
    BagReader::Info in = r.info();
    printf("path:        %s\n", in.path.c_str());
    printf("version:     2.0\n");
    if (in.messages) {
      printf("duration:    %s\n", fmt_dur(in.end - in.start).c_str());
      printf("start:       %s (%.2f)\n", fmt_time(in.start).c_str(), in.start);
      printf("end:         %s (%.2f)\n", fmt_time(in.end).c_str(), in.end);
    }
    printf("size:        %s\n", fmt_size((double)in.size).c_str());
    printf("messages:    %llu\n", (unsigned long long)in.messages);
    printf("compression: none [%d chunk(s)]\n", in.chunks);
    std::map<std::string, std::string> types;
    for (const auto& kv : in.conns) types[kv.second.type] = kv.second.md5;
    bool first = true;
    for (const auto& t : types) {
      printf("%s%s [%s]\n", first ? "types:       " : "             ",
             t.first.c_str(), t.second.c_str());
      first = false;
    }
    first = true;
    // sort topics by name
    std::map<std::string, std::pair<uint64_t, std::string>> topics;
    for (const auto& kv : in.conns)
      topics[kv.second.topic] = {in.counts.count(kv.first) ? in.counts.at(kv.first) : 0,
                                 kv.second.type};
    for (const auto& t : topics) {
      char line[256];
      snprintf(line, sizeof(line), "%-28s %6llu msgs : %s", t.first.c_str(),
               (unsigned long long)t.second.first, t.second.second.c_str());
      printf("%s%s\n", first ? "topics:      " : "             ", line);
      first = false;
    }
  }
  return rc;
}

// -------------------------------------------------------------------- main ---
static void usage() {
  printf(
      "nr_rosbag -- rosbag, with no ROS installed\n\n"
      "  nr_rosbag record [-a] [-O NAME] [-o PREFIX]\n"
      "                   [--split --duration SEC | --size MB] [topic ...]\n"
      "  nr_rosbag play [-r RATE] [-l] [-d SEC] bag [bag ...]\n"
      "  nr_rosbag info bag [bag ...]\n\n"
      "  --master HOST   master host, IP, host:port or full URI\n"
      "  --port N        master port (default: 11311)\n\n"
      "record captures ANY topic (custom types included) to real v2.0 bags.\n");
}

static void load_master_yaml(std::string* uri, std::string* host) {
  std::ifstream f("master.yaml");
  if (!f) return;
  std::string line;
  while (std::getline(f, line)) {
    size_t b = line.find_first_not_of(" \t");
    if (b == std::string::npos || line[b] == '#') continue;
    size_t colon = line.find(':', b);
    if (colon == std::string::npos) continue;
    std::string key = line.substr(b, colon - b), val = line.substr(colon + 1);
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

static std::string resolve_master(std::string uri, int port) {
  if (uri.find("://") == std::string::npos) uri = "http://" + uri;
  size_t sep = uri.find("://");
  std::string scheme = uri.substr(0, sep), rest = uri.substr(sep + 3);
  while (!rest.empty() && rest.back() == '/') rest.pop_back();
  size_t colon = rest.find(':');
  std::string host = colon == std::string::npos ? rest : rest.substr(0, colon);
  std::string p = colon == std::string::npos ? "11311" : rest.substr(colon + 1);
  if (port > 0) p = std::to_string(port);
  return scheme + "://" + host + ":" + p;
}

int main(int argc, char** argv) {
  int port = 0;
  std::string cli_master, cli_host;
  std::vector<std::string> a;
  for (int i = 1; i < argc; ++i) {
    std::string s = argv[i];
    if (s == "--master" && i + 1 < argc) { cli_master = argv[++i]; continue; }
    if (s == "--port" && i + 1 < argc) { port = std::atoi(argv[++i]); continue; }
    if (s == "--host" && i + 1 < argc) { cli_host = argv[++i]; continue; }
    a.push_back(s);
  }
  if (a.empty()) { usage(); return 0; }

  std::string yaml_uri, yaml_host;
  load_master_yaml(&yaml_uri, &yaml_host);
  const char* env_master = std::getenv("ROS_MASTER_URI");
  if (!cli_master.empty()) g_master = cli_master;
  else if (!yaml_uri.empty()) g_master = yaml_uri;
  else if (env_master && *env_master) g_master = env_master;
  g_master = resolve_master(g_master, port);

  std::string host = "localhost";
  if (!cli_host.empty()) host = cli_host;
  else if (!yaml_host.empty()) host = yaml_host;
  else if (const char* h = std::getenv("ROS_HOSTNAME")) host = h;
  else if (const char* ip = std::getenv("ROS_IP")) host = ip;

  set_master_uri(g_master);
  set_hostname(host);
  set_log_level("warn");

  const std::string& cmd = a[0];
  auto has = [&](const std::string& f) {
    for (size_t i = 1; i < a.size(); ++i) if (a[i] == f) return true;
    return false;
  };
  auto opt_val = [&](const std::string& f, const std::string& d) {
    for (size_t i = 1; i + 1 < a.size(); ++i) if (a[i] == f) return a[i + 1];
    return d;
  };
  auto positionals = [&]() {
    std::vector<std::string> out;
    for (size_t i = 1; i < a.size(); ++i) {
      const std::string& s = a[i];
      if (!s.empty() && s[0] == '-') {
        if (s == "-O" || s == "-o" || s == "-r" || s == "-d" ||
            s == "--duration" || s == "--size")
          ++i;  // skip its value
        continue;
      }
      out.push_back(s);
    }
    return out;
  };

  if (cmd == "record") {
    RecordOpts o;
    o.all = has("-a") || has("--all");
    o.output_name = opt_val("-O", opt_val("--output-name", ""));
    o.output_prefix = opt_val("-o", opt_val("--output-prefix", ""));
    o.split = has("--split");
    o.duration = std::atof(opt_val("--duration", "0").c_str());
    o.size_mb = std::atof(opt_val("--size", "0").c_str());
    o.topics = positionals();
    return cmd_record(o);
  }
  if (cmd == "play") {
    std::vector<std::string> bags = positionals();
    if (bags.empty()) { fprintf(stderr, "nr_rosbag play: give one or more bags\n"); return 1; }
    return cmd_play(bags, std::atof(opt_val("-r", "1").c_str()),
                    has("-l") || has("--loop"),
                    std::atof(opt_val("-d", "0.2").c_str()));
  }
  if (cmd == "info") {
    std::vector<std::string> bags = positionals();
    if (bags.empty()) { fprintf(stderr, "nr_rosbag info: give one or more bags\n"); return 1; }
    return cmd_info(bags);
  }
  usage();
  return 1;
}
