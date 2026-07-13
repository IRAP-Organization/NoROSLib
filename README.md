# NoROSLib

<p align="center">
  <img
    src="https://github.com/user-attachments/assets/c398c07a-1c5f-4cdb-b0a2-8c4068b6b208"
    alt="NoROSLib Logo"
    width="260">
</p>

<h1 align="center">NoROSLib</h1>

<p align="center">
<b>Talk to a native ROS&nbsp;1 <code>roscore</code> directly — no ROS install, no bridge.</b>
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
**A lightweight ROS client library that needs no ROS installed — and no bridge.**
NoROSLib (you import it as `irap_noroslib`) speaks ROS 1's own wire protocol *directly*: it
registers with the **ROS Master over XML-RPC** and exchanges messages over
**TCPROS / UDPROS** — the very protocols `rospy` and `roscpp` use. So it connects
**straight to a native, unmodified `roscore`**, and real ROS nodes see it as a
legitimate ROS node. There is **no rosbridge, no roslibpy/WebSocket gateway, and
nothing extra installed on the robot** — and it links **zero ROS libraries**.

This is a real, **wire-compatible** ROS client — it publishes/subscribes real
topics, calls real services, runs real actions, and reads/writes real parameters
on a genuine roscore. It is **not** a rospy look-alike that only talks to other
NoROSLib programs. Available in **Python** (rospy-flavoured) and **C++**
(roscpp-flavoured, single-header); both are verified end-to-end against real ROS
Noetic — a real `rostopic echo` / `rosservice call` / `rosparam` sees it, and it
sees them.

## Native ROS — not a bridge

NoROSLib is **not** rosbridge/roslibpy, and it does **not** need one. It runs no
`rosbridge_server`, no WebSocket gateway, no translator process — on either side.
It implements the ROS 1 protocols themselves (XML-RPC master/slave discovery +
TCPROS/UDPROS transport), so from the robot's point of view a NoROSLib process is
indistinguishable from a native `roscpp` / `rospy` node. Point it at the robot's
`ROS_MASTER_URI` and go.

| Approach | Bridge/server needed on the ROS side? | Speaks native ROS wire protocol? | Extra install on the robot |
|---|:---:|:---:|:---:|
| rosbridge + roslibpy | **Yes** — `rosbridge_server` | No — JSON over WebSocket | `rosbridge_suite` |
| **NoROSLib** | **No** | **Yes** — XML-RPC + TCPROS/UDPROS | **None** |

```
   your machine (Windows / macOS / any Linux)          the robot (Ubuntu + ROS)
   ┌───────────────────────────┐                       ┌────────────────────────┐
   │  your app  +  irap_noroslib        │   XML-RPC + TCPROS    │  roscore               │
   │  (no ROS, no bridge)       │◄─────────────────────►│  native rospy/roscpp   │
   └───────────────────────────┘   (the real ROS wire)  │  nodes (unmodified)    │
                                                         └────────────────────────┘
```

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
| **Messages** | std/geometry/sensor/nav/diagnostic/trajectory + custom | ✅ | ✅ |
| **Load `.msg`/`.srv`/`.action` files** | point it at a file copied off a robot | ✅ | ✅ |
| **md5** | auto-computed **and** auto-discovered from mismatch | ✅ | ✅ |
| **Config** | master URI + hostname from code | ✅ | ✅ |
| **Master (`nr_roscore`)** | *be* the roscore: Master + Param Server (dict trees) + `/rosout` | ✅ | ✅ |

## Install

### Python — `pip install`

```bash
pip install irap_noroslib           # from PyPI (once published)
pip install ./python        # from this repo
```

**Dependencies: none.** irap_noroslib is pure **Python 3.6+ standard library** — it talks
to ROS using only `xmlrpc`, `socket`, `socketserver`, `threading`, `struct` and
`hashlib`, all built into CPython. [`python/requirements.txt`](python/requirements.txt)
is intentionally empty and documents exactly that. (OpenCV/numpy are needed *only*
by the optional webcam demos, never by the library.)

Then:

```python
import irap_noroslib
from irap_noroslib.std_msgs.msg import String     # the rospy path, prefixed

irap_noroslib.init_node("talker")
pub = irap_noroslib.Publisher("/chatter", String)
rate = irap_noroslib.Rate(10)
while not irap_noroslib.is_shutdown():
    pub.publish(String(data="hello world"))
    rate.sleep()
```

Messages import on the **same path rospy uses**, just prefixed — porting a node is
a one-word edit:

```python
from std_msgs.msg import String              # rospy
from irap_noroslib.std_msgs.msg import String  # irap_noroslib
```

See [`python/README.md`](python/README.md).

### C++ — copy one header

The whole library is a single header, [`cpp/include/irap_noroslib.hpp`](cpp/include/irap_noroslib.hpp).
Copy it into your include path. Put the implementation in **one** `.cpp`:

```cpp
#define IRAP_NOROSLIB_IMPLEMENTATION
#include "irap_noroslib.hpp"
```

Everywhere else just `#include "irap_noroslib.hpp"`. Compile with `-std=c++17 -pthread`.

**Dependencies: none beyond a C++17 toolchain.** No ROS, no Boost, no third-party
libraries — just the compiler and the OS's own sockets/threads: link `-pthread`
on Linux/macOS/WSL, `-lws2_32` on MinGW (MSVC auto-links Winsock). CMake ≥ 3.10 is
optional, only to build the bundled examples. Full details in
[`cpp/DEPENDENCIES.md`](cpp/DEPENDENCIES.md). (OpenCV is needed *only* by the two
optional webcam examples, never by the library.)

Messages also have the **same include path roscpp uses**, just prefixed — and the
type names are untouched, because messages keep their real ROS namespaces:

```cpp
#include "std_msgs/String.h"                 // roscpp
#include "irap_noroslib/std_msgs/String.h"   // irap_noroslib  -> still std_msgs::String
```

```cpp
#include "irap_noroslib.hpp"
#include "irap_noroslib/std_msgs/String.h"
int main() {
  irap_noroslib::init_node("talker");
  irap_noroslib::Publisher<std_msgs::String> pub("/chatter");
  irap_noroslib::Rate rate(10);
  while (irap_noroslib::ok()) {
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

irap_noroslib **reads the publisher's real md5 out of that error, adopts it, and
reconnects** — so you never get stuck on an md5 you got wrong when *subscribing*.

This rescues the **subscriber** side. It cannot fix a publisher: if you *advertise*
with a wrong md5, real ROS subscribers reject you and there is no error to learn
from. (irap_noroslib plays the other role too, emitting the same rejection to peers
that present a wrong md5 to it.) If you don't want to think about md5s at all,
load the `.msg` file and let irap_noroslib derive it — see [Custom messages](#custom-messages).

## Publish / subscribe

The full pub + sub pair in both languages (examples: `talker`/`listener`).

### Python

```python
# talker.py — publisher
import irap_noroslib
from irap_noroslib import msg

irap_noroslib.init_node("talker")
pub = irap_noroslib.Publisher("/chatter", msg.String)
rate = irap_noroslib.Rate(10)                       # 10 Hz
i = 0
while not irap_noroslib.is_shutdown():
    pub.publish(msg.String(data="hello world %d" % i))
    i += 1
    rate.sleep()
```

```python
# listener.py — subscriber
import irap_noroslib
from irap_noroslib import msg

irap_noroslib.init_node("listener")
irap_noroslib.Subscriber("/chatter", msg.String,
                 lambda m: irap_noroslib.loginfo("I heard: " + m.data))
irap_noroslib.spin()
```

### C++

```cpp
// talker.cpp — publisher
#include "irap_noroslib.hpp"
int main() {
  irap_noroslib::init_node("talker");
  irap_noroslib::Publisher<std_msgs::String> pub("/chatter");
  irap_noroslib::Rate rate(10);                       // 10 Hz
  int i = 0;
  while (irap_noroslib::ok()) {
    std_msgs::String m;
    m.data = "hello world " + std::to_string(i++);
    pub.publish(m);
    rate.sleep();
  }
}
```

```cpp
// listener.cpp — subscriber
#include "irap_noroslib.hpp"
int main() {
  irap_noroslib::init_node("listener");
  irap_noroslib::Subscriber<std_msgs::String> sub("/chatter",
      [](const std_msgs::String& m){ irap_noroslib::loginfo("I heard: " + m.data); });
  irap_noroslib::spin();
}
```

Both interoperate with real ROS: `rostopic echo /chatter` sees the talker, and
`rostopic pub -r 10 /chatter std_msgs/String "data: hi"` feeds the listener.

## Built-in messages

A ready-to-use catalog ships with irap_noroslib — every md5sum matches `rosmsg md5 <type>`
exactly, so they interoperate with real ROS nodes. Python: `from irap_noroslib import msg`
then `msg.Odometry` (or `msg.get("nav_msgs/Odometry")`). C++: `#include "irap_noroslib.hpp"`
then `nav_msgs::Odometry`.

The full catalog is **64 types**, identical in Python and C++:

| Package | Types |
|---|---|
| **std_msgs** (19) | Bool, Byte, Char, ColorRGBA, Duration, Empty, Float32, Float64, Header, Int8, Int16, Int32, Int64, String, Time, UInt8, UInt16, UInt32, UInt64 |
| **geometry_msgs** (16) | Accel, Point, Point32, Polygon, Pose, PoseArray, PoseStamped, PoseWithCovariance, Quaternion, Transform, TransformStamped, Twist, TwistStamped, TwistWithCovariance, Vector3, Wrench |
| **sensor_msgs** (14) | CameraInfo, CompressedImage, Image, Imu, JointState, LaserScan, MagneticField, NavSatFix, NavSatStatus, PointCloud2, PointField, Range, RegionOfInterest, Temperature |
| **nav_msgs** (5) | GridCells, MapMetaData, OccupancyGrid, Odometry, Path |
| **diagnostic_msgs** (3) | DiagnosticArray, DiagnosticStatus, KeyValue |
| **trajectory_msgs** (4) | JointTrajectory, JointTrajectoryPoint, MultiDOFJointTrajectory, MultiDOFJointTrajectoryPoint |
| **actionlib_msgs** (3) | GoalID, GoalStatus, GoalStatusArray |

Usage (constructing, nested fields, arrays, `Header`, publish/subscribe) is shown
in **[python/README.md](python/README.md#messages)** and
**[cpp/README.md](cpp/README.md#messages)**.

**C++ and Python expose the identical 64-type catalog** — the same types under
`msg.*` (Python) and the matching `pkg_msgs::*` structs (C++), every md5 matching
`rosmsg md5`. The two libraries are in lock-step.

Need a type that isn't here? Define it in one line (below) — irap_noroslib derives the
md5 and codec from the `.msg` text.

## Custom messages

Your own message type, three ways. All three produce the **same wire bytes and the
same md5sum**, so they interoperate freely with each other and with real ROS.
Example: `custom_msg` (shows two of them side by side).

| | You give it | md5sum | Both languages? |
|---|---|---|:---:|
| **1. Load the `.msg` file** | the file's path | **derived for you** | ✅ |
| **2. From `.msg` text, in code** | the text | **derived for you** | Python |
| **3. A hand-written struct** | fields + codec + **the md5** | you supply it | C++ |

### 1. Load the `.msg` file — easiest, and works in both languages

Copy a `.msg` off the robot and hand irap_noroslib its **full path**. No catkin
package, no ROS install, nothing to write:

```python
from irap_noroslib import load_msg_file, load_srv_file, load_action_file

CustomData = load_msg_file("/home/me/msgs/CustomData.msg", "my_robot_msgs")
Srv        = load_srv_file("/home/me/msgs/MySrv.srv",      "my_robot_msgs")
Act        = load_action_file("/home/me/msgs/MyAct.action", "my_robot_msgs")

pub = irap_noroslib.Publisher("/data", CustomData)
pub.publish(CustomData(id=7, label="hi"))
```

```cpp
using namespace irap_noroslib;
MsgType CustomData = load_msg_file("/home/me/msgs/CustomData.msg", "my_robot_msgs");

DynamicPublisher pub("/data", CustomData);
DynamicMessage m = CustomData.create();
m.set("id", 7).set("label", "hi");            // fields by name
m.set("header.frame_id", "base_link");        // nest with a dot
pub.publish(m);
```

The md5sum and the wire codec are **derived from the file** by the exact ROS
algorithm, so the type is precisely what `rosmsg md5` computes and real ROS nodes
accept it. **One file, one call** — several custom messages means several calls,
each with its own path (order doesn't matter). `load_action_file` registers all 7
ROS action types for you.

The second argument is the ROS package the message came from — the `my_robot_msgs`
in `my_robot_msgs/CustomData` — because ROS names types `pkg/Type`.

C++ also gets `DynamicSubscriber`, `DynamicServiceServer`, `DynamicServiceClient`,
`DynamicActionClient` and `DynamicActionServer`: the usual classes with the
compile-time type replaced by a loaded one.

### 2. From `.msg` text, in code *(Python)*

```python
from irap_noroslib import define_message

# md5 + wire codec derived automatically from the text
Pose2D = define_message("my_pkg/Pose2D", """
    float64 x
    float64 y
    float64 theta
""")

pub = irap_noroslib.Publisher("/pose", Pose2D)
pub.publish(Pose2D(x=1.0, y=2.0, theta=3.14))
```

Nest other registered types by full name (`std_msgs/Header header`,
`my_pkg/Pose2D[] poses`), use constants (`uint8 FOO=1`), fixed/variable arrays, and
every ROS builtin.

### 3. A hand-written struct *(C++)* — fully typed, zero overhead

The typed, zero-cost option. Note the trade-off: **you must supply the md5
yourself** (`rosmsg md5 <type>`) — nothing derives it for you here. If you'd rather
not, use option 1.

```cpp
#include "irap_noroslib.hpp"

struct Pose2D {
  static constexpr const char* TYPE = "my_pkg/Pose2D";
  static constexpr const char* MD5  = "938fa65709584ad8e77d238529be13b8"; // rosmsg md5
  static constexpr const char* DEFINITION = "float64 x\nfloat64 y\nfloat64 theta\n";
  double x = 0, y = 0, theta = 0;
  std::vector<uint8_t> serialize() const {
    irap_noroslib::Writer w; w.f64(x); w.f64(y); w.f64(theta); return w.b;
  }
  static Pose2D deserialize(const std::vector<uint8_t>& b) {
    irap_noroslib::Reader r(b); Pose2D m; m.x = r.f64(); m.y = r.f64(); m.theta = r.f64(); return m;
  }
};

// irap_noroslib::Publisher<Pose2D> pub("/pose");  /  irap_noroslib::Subscriber<Pose2D> sub("/pose", cb);
```

Getting the md5 wrong here matters: a **publisher** with a bad md5 is rejected by
real ROS subscribers. (md5 *discovery* — described above — rescues the **subscriber**
side only: it learns the publisher's real md5 from the rejection and reconnects. It
cannot fix a publisher.)

For a stamped message, make the first field a `Header` (Python: `std_msgs/Header
header`; C++: compose `std_msgs::Header` in your codec) — see `stamped_pub` /
`stamped_sub`.

---

Verified end-to-end: a custom package built on real ROS with `catkin_make`, then
loaded from its bare files — in **both** languages all 11 md5s (3 messages, a
service, 7 action types) match `rosmsg md5` / `rossrv md5`, and the types flow
**both ways** against genuine rospy nodes over topics, a service and an action.
Details in [`python/README.md`](python/README.md) / [`cpp/README.md`](cpp/README.md).

## Examples

Every example lives in `python/examples/` and/or `cpp/examples/`. Run them with a
master up (a real `roscore`, or irap_noroslib's own `nr_roscore`) and `ROS_MASTER_URI`
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
| **params_example** | parameters: get/set/has/delete (Python also search/list) | ✅ | ✅ |
| **udp_listener** | subscribe over **UDPROS** | ✅ | ✅ |
| **nr_roscore** | run your own ROS master (roscore) | ✅ | ✅ |
| **webcam_pub** | publish `sensor_msgs/Image` + `CompressedImage` from `/dev/video0` | ✅  | ✅  |
| **image viewer** | subscribe those images and display them | ✅  `ros_image_viewer.py` (real rospy) | ✅  `image_viewer.cpp` (irap_noroslib) |

 The webcam publisher and image viewer **require OpenCV** (`cv2` in Python,
`<opencv2/opencv.hpp>` in C++). **OpenCV is not a dependency of irap_noroslib** — the core
library needs nothing beyond the standard library / a C++17 toolchain; only these
optional demos use it. The C++ CMake build compiles them only if OpenCV is found.

Cross-check any of them with real ROS tools — `rostopic echo`/`pub`,
`rosservice call`, `rosparam get`/`set`, and the `roscpp_tutorials` /
`rospy_tutorials` / `actionlib_tutorials` packages.

### End-to-end demo with irap_noroslib only (no ROS installed)

```bash
# 1) your own master
python3 python/examples/nr_roscore.py --port 11311 &
export ROS_MASTER_URI=http://localhost:11311
# 2) talk to it
python3 python/examples/listener.py &
python3 python/examples/talker.py
```

## Run your own ROS master — `nr_roscore`

irap_noroslib can also **be the roscore**, not just talk to one. `nr_roscore` is a
standalone ROS master + parameter server (no ROS installed) that real ROS nodes
(rospy/roscpp), `rostopic`/`rosservice`/`rosparam`, and irap_noroslib nodes all register
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
it, and irap_noroslib nodes use it as their master. Stress-tested: the 13-type topic
matrix (TCPROS+UDPROS), a 3,600-call service flood, concurrent parameter churn,
and a 5,400-op registration storm all pass against both masters.

### Message-type coverage (verified against real ROS)

Every built-in message type — and a **custom message created on real ROS** (a
catkin `noros_stress_msgs/CustomData` package) — round-trips through a real
`roscore` in **both languages**, decoded by genuine `rospy` subscribers (so the
md5 handshake + wire bytes are checked by ROS itself):

| | types published | verified by real ROS |
|---|:---:|:---:|
| **Python** | 64 built-ins + custom | 65 / 65 ✅ |
| **C++** | 64 built-ins + custom | 65 / 65 ✅ |

The custom message's md5 that irap_noroslib computes (`90f5077…`) is **identical** to the
one real ROS's `rosmsg md5` generates for the same `.msg`, and it flows both ways
(irap_noroslib → `rostopic echo`, and a real `rospy` publisher → irap_noroslib subscriber) in
Python and C++.

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
python/    pip-installable package (irap_noroslib/), examples/, tests/
cpp/       single header (include/irap_noroslib.hpp), examples/, dev/ (source of truth)
LICENSE    MIT
```

## Credits

Created by **iRAP Robot**, King Mongkut's University of Technology North Bangkok
(KMUTNB).

## License

MIT — see [LICENSE](LICENSE).
