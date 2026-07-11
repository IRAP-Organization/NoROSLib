# NoROSLib

<p align="center">
  <img
    src="https://github.com/user-attachments/assets/c398c07a-1c5f-4cdb-b0a2-8c4068b6b208"
    alt="NoROSLib Logo"
    width="260">
</p>

<h1 align="center">NoROSLib</h1>

<p align="center">
<b>A lightweight ROS client library that needs no ROS installed.</b>
</p>

<p align="center">
Python • C++ • Windows • Linux • macOS
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Python-3.6+-3776AB?logo=python&logoColor=white">
  <img src="https://img.shields.io/badge/C++-17-00599C?logo=cplusplus&logoColor=white">
  <img src="https://img.shields.io/badge/ROS-Noetic-22314E?logo=ros">
  <img src="https://img.shields.io/badge/License-MIT-green">
</p>

**A lightweight ROS client library that needs no ROS installed.** NoROSLib (the
library you import is called `noros`) impersonates a ROS node by speaking the real
ROS wire protocols directly — the XML-RPC master/slave API and TCPROS/UDPROS — so
a real `roscore` and real ROS nodes treat it as a legitimate node. It links
**zero ROS libraries**.

Available in **Python** (rospy-flavoured) and **C++** (roscpp-flavoured,
single-header). Both are verified end-to-end against real ROS Noetic.

## Why NoROSLib?

ROS Noetic officially supports **only Ubuntu 20.04**. That is a real problem the
moment your world isn't exactly Ubuntu 20.04:

- **Your OS is newer (or different).** Run your node on Ubuntu 22.04 — or any
  distro that isn't 20.04 — and the prebuilt ROS Noetic packages simply aren't
  there for you.
- **Building ROS from source is painful and slow.** Compiling ROS Noetic yourself
  on an unsupported OS takes a very long time, drags in a mountain of
  dependencies, and often still breaks.
- **Windows is effectively a no-go.** Getting ROS Noetic onto Windows is a huge,
  time-consuming effort — if you get it working at all.

NoROSLib sidesteps all of it. Instead of *installing* ROS, it just **talks to a
ROS Noetic master over the network** the same way a real node does. So a program
on Ubuntu 22.04, an older/newer distro, or any machine with a socket can join a
ROS Noetic graph — publish/subscribe topics, call services, run actions, read and
write parameters — **without building or installing ROS at all.**

It is designed to be **light-weight and fast**: no ROS libraries, minimal
footprint, quick to drop in (Python is `pip install`; C++ is one header to copy),
so you can talk to ROS Noetic from wherever your code actually runs.

### Platform support

| | Runs on |
|---|---|
| **Python** | Pure standard-library — anywhere Python 3.6+ runs (Linux, macOS, Windows). |
| **C++** | Linux, macOS, WSL, **and native Windows** — a built-in Winsock/POSIX compatibility layer picks the right socket API per platform. On Windows link `ws2_32` (MSVC auto-links it; with MinGW pass `-lws2_32`). |

The **master it talks to** is a normal ROS Noetic `roscore` (typically on an
Ubuntu 20.04 box, a container, or a robot) — NoROSLib is the *client* that reaches
it from elsewhere.

## Capabilities

| Feature | Transport / API | Python | C++ |
|---|---|:---:|:---:|
| **Topics** (pub/sub) | TCPROS **and** UDPROS | ✅ | ✅ |
| **Services** (srv) | rosrpc / TCPROS | ✅ | ✅ |
| **Actions** (actionlib) | 5 action topics | ✅ | ✅ |
| **Parameters** | master XML-RPC | ✅ | ✅ |
| **Messages** | std/geometry/sensor + custom | ✅ | ✅ |
| **md5** | auto-computed **and** auto-discovered from mismatch | ✅ | ✅ |
| **Config** | master URI + hostname from code | ✅ | ✅ |
| **Master (`nr_roscore`)** | *be* the roscore: Master + Param Server (dict trees) + `/rosout` | ✅ | ✅ |

## Install

### Python — `pip install`

```bash
pip install noros           # from PyPI (once published)
pip install ./python        # from this repo
```

Then:

```python
import noros
from noros import msg

noros.init_node("talker")
pub = noros.Publisher("/chatter", msg.String)
rate = noros.Rate(10)
while not noros.is_shutdown():
    pub.publish(msg.String(data="hello world"))
    rate.sleep()
```

See [`python/README.md`](python/README.md).

### C++ — copy one header

The whole library is a single header, [`cpp/include/noros.hpp`](cpp/include/noros.hpp).
Copy it into your include path. Put the implementation in **one** `.cpp`:

```cpp
#define NOROS_IMPLEMENTATION
#include "noros.hpp"
```

Everywhere else just `#include "noros.hpp"`. Compile with `-std=c++17 -pthread`.

```cpp
#include "noros.hpp"
int main() {
  noros::init_node("talker");
  noros::Publisher<std_msgs::String> pub("/chatter");
  noros::Rate rate(10);
  while (noros::ok()) {
    std_msgs::String m; m.data = "hi";
    pub.publish(m); rate.sleep();
  }
}
```

See [`cpp/README.md`](cpp/README.md).

## The headline feature: automatic md5 discovery

If you subscribe with a wrong or unknown md5sum, a real ROS publisher rejects the
connection with

```
[ERROR] Client [...] wants topic X to have datatype/md5sum [.../c3c0...],
but our version has [.../4193...]. Dropping connection.
```

noros **reads the publisher's real md5 out of that error, adopts it, and
reconnects** — so you never get stuck on an md5 you got wrong. Discovery is
symmetric: noros emits the same error to peers that present a wrong md5 to it.

## Publish / subscribe

The full pub + sub pair in both languages (examples: `talker`/`listener`).

### Python

```python
# talker.py — publisher
import noros
from noros import msg

noros.init_node("talker")
pub = noros.Publisher("/chatter", msg.String)
rate = noros.Rate(10)                       # 10 Hz
i = 0
while not noros.is_shutdown():
    pub.publish(msg.String(data="hello world %d" % i))
    i += 1
    rate.sleep()
```

```python
# listener.py — subscriber
import noros
from noros import msg

noros.init_node("listener")
noros.Subscriber("/chatter", msg.String,
                 lambda m: noros.loginfo("I heard: " + m.data))
noros.spin()
```

### C++

```cpp
// talker.cpp — publisher
#include "noros.hpp"
int main() {
  noros::init_node("talker");
  noros::Publisher<std_msgs::String> pub("/chatter");
  noros::Rate rate(10);                       // 10 Hz
  int i = 0;
  while (noros::ok()) {
    std_msgs::String m;
    m.data = "hello world " + std::to_string(i++);
    pub.publish(m);
    rate.sleep();
  }
}
```

```cpp
// listener.cpp — subscriber
#include "noros.hpp"
int main() {
  noros::init_node("listener");
  noros::Subscriber<std_msgs::String> sub("/chatter",
      [](const std_msgs::String& m){ noros::loginfo("I heard: " + m.data); });
  noros::spin();
}
```

Both interoperate with real ROS: `rostopic echo /chatter` sees the talker, and
`rostopic pub -r 10 /chatter std_msgs/String "data: hi"` feeds the listener.

## Custom messages

Ship your own message type. Its md5sum is derived by the exact ROS algorithm (so
it matches `rosmsg md5 <type>` and interoperates), or — if you don't know it —
noros discovers the publisher's real md5 from the handshake. Example: `custom_msg`.

### Python — one call from `.msg` text

```python
import noros
from noros import define_message

# md5 + wire codec derived automatically from the .msg text
Pose2D = define_message("my_pkg/Pose2D", """
    float64 x
    float64 y
    float64 theta
""")

noros.init_node("pub")
pub = noros.Publisher("/pose", Pose2D)
pub.publish(Pose2D(x=1.0, y=2.0, theta=3.14))
# subscribe: noros.Subscriber("/pose", Pose2D, lambda m: print(m.x, m.y, m.theta))
```

Nest other registered types by full name (`std_msgs/Header header`, `my_pkg/Pose2D[] poses`),
use constants (`uint8 FOO=1`), fixed/variable arrays, and all ROS builtins.

### C++ — a small struct

A noros message is any struct with the three static strings + the two codec
functions (use `noros::Writer` / `noros::Reader`, little-endian, length-prefixed —
the ROS recipe):

```cpp
#include "noros.hpp"

struct Pose2D {
  static constexpr const char* TYPE = "my_pkg/Pose2D";
  static constexpr const char* MD5  = "938fa65709584ad8e77d238529be13b8"; // rosmsg md5
  static constexpr const char* DEFINITION = "float64 x\nfloat64 y\nfloat64 theta\n";
  double x = 0, y = 0, theta = 0;
  std::vector<uint8_t> serialize() const {
    noros::Writer w; w.f64(x); w.f64(y); w.f64(theta); return w.b;
  }
  static Pose2D deserialize(const std::vector<uint8_t>& b) {
    noros::Reader r(b); Pose2D m; m.x = r.f64(); m.y = r.f64(); m.theta = r.f64(); return m;
  }
};

// noros::Publisher<Pose2D> pub("/pose");  /  noros::Subscriber<Pose2D> sub("/pose", cb);
```

Don't know the md5? Put anything and let noros discover the publisher's real one
from the mismatch error (see the md5-discovery feature above). For a stamped
message, make the first field a `Header` (Python: `std_msgs/Header header`; C++:
compose `std_msgs::Header` in your codec) — see the `stamped_pub`/`stamped_sub`
examples.

## Examples

Every example lives in `python/examples/` and/or `cpp/examples/`. Run them with a
master up (a real `roscore`, or noros's own `nr_roscore`) and `ROS_MASTER_URI`
set. Python: `python3 python/examples/<name>.py`. C++: build with CMake, then
`./cpp/build/<name>`.

| Example | Shows | Python | C++ |
|---|---|:---:|:---:|
| **talker** | publish `std_msgs/String` at 10 Hz | ✅ | ✅ |
| **listener** | subscribe `std_msgs/String` | ✅ | ✅ |
| **custom_msg** | define + publish your own message type | ✅ | ✅ |
| **md5_discovery** | subscribe with a wrong md5, auto-recover from the error | ✅ | ✅ |
| **stamped_pub** / **stamped_sub** | a custom message with a `std_msgs/Header` | ✅ | ✅ |
| **add_two_ints_server** / **add_two_ints_client** | a service (srv) server + client | ✅ | ✅ |
| **fibonacci_server** / **fibonacci_client** | an action (actionlib) server + client | ✅ | ✅ |
| **params_example** | parameters: get/set/has/delete/search/list | ✅ | — |
| **udp_listener** | subscribe over **UDPROS** | ✅ | — (use `Subscriber(..., "udpros")`) |
| **nr_roscore** | run your own ROS master (roscore) | ✅ | ✅ |
| **webcam_pub** | publish `sensor_msgs/Image` + `CompressedImage` from `/dev/video0` | ✅ | — |
| **ros_image_viewer** | a *real* rospy node that `cv2.imshow`s the webcam feed | ✅ (rospy) | — |

Cross-check any of them with real ROS tools — `rostopic echo`/`pub`,
`rosservice call`, `rosparam get`/`set`, and the `roscpp_tutorials` /
`rospy_tutorials` / `actionlib_tutorials` packages.

### End-to-end demo with noros only (no ROS installed)

```bash
# 1) your own master
python3 python/examples/nr_roscore.py --port 11311 &
export ROS_MASTER_URI=http://localhost:11311
# 2) talk to it
python3 python/examples/listener.py &
python3 python/examples/talker.py
```

## Run your own ROS master — `nr_roscore`

noros can also **be the roscore**, not just talk to one. `nr_roscore` is a
standalone ROS master + parameter server (no ROS installed) that real ROS nodes
(rospy/roscpp), `rostopic`/`rosservice`/`rosparam`, and noros nodes all register
with. It implements the Master API (register/unregister publisher/subscriber/
service, `lookupService`/`lookupNode`, `getSystemState`, `getPublishedTopics`, …),
sends `publisherUpdate` to subscribers, and serves parameters.

```bash
# Python
python3 python/examples/nr_roscore.py --port 11311

# C++ (built with the examples)
./cpp/build/nr_roscore --port 11311
```

Configuration (same knobs as a real roscore):

| Knob | Source (in precedence order) |
|---|---|
| **port** | `--port` › port in `$ROS_MASTER_URI` › `11311` |
| **hostname** | `--host` › `$ROS_HOSTNAME` › `$ROS_IP` › system hostname |

Then point any node at it: `export ROS_MASTER_URI=http://<host>:<port>`. Both
masters handle **full (nested dict) parameter trees** and auto-start a `/rosout`
→ `/rosout_agg` aggregator (disable with `--no-rosout`). Verified with real ROS:
two `rostopic pub`/`echo` nodes talk **through** nr_roscore,
`rosservice`/`rosparam` (incl. nested dicts)/`rostopic list`/`info` work against
it, and noros nodes use it as their master. Stress-tested: the 13-type topic
matrix (TCPROS+UDPROS), a 3,600-call service flood, concurrent parameter churn,
and a 5,400-op registration storm all pass against both masters.

## How it works

1. **Register with the master** (`registerPublisher` / `registerSubscriber`) and
   stand up a **slave XML-RPC server** so the master and peers can call
   `requestTopic` / `publisherUpdate`.
2. **Publisher:** open a TCPROS (and UDPROS) socket; on each subscriber, read its
   connection header, check the md5, send ours, then stream `[len][body]`.
3. **Subscriber:** for each publisher the master reports, `requestTopic` →
   connect → exchange headers. On an md5 error, **learn the real md5 and
   reconnect**; otherwise adopt the publisher's type/md5 and deliver decoded
   messages to your callback.
4. **Services / actions / parameters** ride the same master + TCPROS machinery.

Message bodies are the exact bytes ROS puts on the wire, so the codecs are
byte-for-byte compatible with real ROS (every md5sum matches `rosmsg`/`rossrv`).

## Layout

```
python/    pip-installable package (noros/), examples/, tests/
cpp/       single header (include/noros.hpp), examples/, dev/ (source of truth)
LICENSE    MIT
```

## Credits

Created by **iRAP Robot**, King Mongkut's University of Technology North Bangkok
(KMUTNB).

## License

MIT — see [LICENSE](LICENSE).
# NoROSLib
