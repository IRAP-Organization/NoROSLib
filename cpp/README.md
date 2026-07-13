# NoROSLib (C++) — `#include "irap_noroslib.hpp"`

A roscpp-flavoured ROS client library — **no ROS installed, no bridge, single
header, links zero ROS libraries.** C++17 + POSIX sockets + pthreads. It speaks the
real ROS wire protocols (XML-RPC master/slave + TCPROS/UDPROS) *directly*, so it
connects to a **native, unmodified `roscore`** and real ROS nodes treat it as a
legitimate node — **not** rosbridge/roslibpy, and nothing extra runs on the robot.

## Install: copy one file

The entire library is a single header: [`include/irap_noroslib.hpp`](include/irap_noroslib.hpp).
Copy it into your project's include path. That's the whole install.

Because it's header-only (stb-style), the implementation must be compiled in
**exactly one** translation unit. In one `.cpp` of your project:

```cpp
#define IRAP_NOROSLIB_IMPLEMENTATION
#include "irap_noroslib.hpp"
```

In every **other** file, just `#include "irap_noroslib.hpp"` (no define). Compile with
C++17 and link the platform's socket/thread libs:

```bash
# Linux / macOS / WSL
g++ -std=c++17 -pthread your_app.cpp irap_noroslib_impl.cpp -o your_app

# Windows, MinGW
g++ -std=c++17 your_app.cpp irap_noroslib_impl.cpp -o your_app.exe -lws2_32

# Windows, MSVC (ws2_32 is auto-linked via #pragma comment)
cl /std:c++17 /EHsc your_app.cpp irap_noroslib_impl.cpp
```

(where `irap_noroslib_impl.cpp` is the one file that defines `IRAP_NOROSLIB_IMPLEMENTATION`.)

### Platforms

irap_noroslib ships a Winsock/POSIX compatibility layer, so the **same header builds on
Linux, macOS, WSL, and native Windows**. On Windows it uses Winsock2 (`WSAStartup`
is handled for you); everywhere else it uses BSD sockets. The only platform
difference you touch is the link flag above.

## Dependencies

**None beyond a standard C++17 toolchain.** No ROS, no Boost, no third-party
packages — just the compiler and the OS's own sockets/threads (`-pthread` on
POSIX, `-lws2_32` on MinGW, auto-linked on MSVC). CMake ≥ 3.10 is optional, only
to build the bundled examples. Full details in [`DEPENDENCIES.md`](DEPENDENCIES.md).

## Hello, topics

**Publisher** (`talker.cpp`):

```cpp
#include "irap_noroslib.hpp"
#include "irap_noroslib/std_msgs/String.h"     // the roscpp path, prefixed
int main() {
  irap_noroslib::init_node("talker");
  irap_noroslib::Publisher<std_msgs::String> pub("/chatter");
  irap_noroslib::Rate rate(10);
  while (irap_noroslib::ok()) {
    std_msgs::String m; m.data = "hello world";
    pub.publish(m);
    rate.sleep();
  }
}
```

**Subscriber** (`listener.cpp`):

```cpp
#include "irap_noroslib.hpp"
#include "irap_noroslib/std_msgs/String.h"
int main() {
  irap_noroslib::init_node("listener");
  irap_noroslib::Subscriber<std_msgs::String> sub("/chatter",
      [](const std_msgs::String& m){ irap_noroslib::loginfo("I heard: " + m.data); });
  irap_noroslib::spin();
}
```

That's a roscpp node with `ros::` swapped for `irap_noroslib::` — the message type
(`std_msgs::String`) and its include path are the ones you already know, just
prefixed. `#include "irap_noroslib.hpp"` alone would also be enough; the per-type
header is there so ported code reads unchanged.

Point at a master (defaults to `http://127.0.0.1:11311`) via `$ROS_MASTER_URI` /
`$ROS_HOSTNAME`, or from code: `irap_noroslib::set_master_uri(...)` / `set_hostname(...)`
before `init_node`.

## Build the examples

```bash
cd cpp
cmake -S . -B build && cmake --build build -j
./build/talker        # ./build/listener, add_two_ints_*, stamped_*, fibonacci_*, ...
```

The CMake build compiles the implementation once (`irap_noroslib_impl.cpp`) and links it
into each example — demonstrating exactly the integration described above.

### All examples (`cpp/examples/`)

| File | What it does |
|---|---|
| `talker.cpp` | publish `std_msgs/String` at 10 Hz |
| `listener.cpp` | subscribe `std_msgs/String` |
| `custom_msg.cpp` | define + publish your own message struct |
| `md5_discovery.cpp` | subscribe with a wrong md5, recover automatically |
| `stamped_pub.cpp` / `stamped_sub.cpp` (+ `sensor_reading.hpp`) | a message with a `std_msgs::Header` |
| `add_two_ints_server.cpp` / `add_two_ints_client.cpp` (+ `add_two_ints.hpp`) | a service server + client |
| `fibonacci_server.cpp` / `fibonacci_client.cpp` (+ `fibonacci_action.hpp`) | an action server + client |
| `params_example.cpp` | parameters get / set / has / delete |
| `udp_listener.cpp` | subscribe over UDPROS |
| `webcam_pub.cpp` † | publish `sensor_msgs/Image` + `CompressedImage` from `/dev/video0` |
| `image_viewer.cpp` † | subscribe those images and show them (`cv::imshow`) |
| `nr_roscore.cpp` | run your own ROS master (roscore) + `/rosout` aggregator |

† `webcam_pub` and `image_viewer` **require OpenCV** (`cv::VideoCapture` /
`cv::imencode` / `cv::imshow`). **OpenCV is not a dependency of irap_noroslib** — the core
library links zero third-party packages; only these two optional demos need it.
The CMake build compiles them **only if OpenCV is found** (otherwise it prints a
notice and skips them). By hand:

```bash
g++ -std=c++17 examples/webcam_pub.cpp irap_noroslib_impl.cpp -o webcam_pub \
    -pthread $(pkg-config --cflags --libs opencv4)
```

These mirror the Python examples one-for-one (same names, same behaviour), so the
two libraries stay in lock-step. Every example calls `irap_noroslib::set_master_uri(...)`
and `irap_noroslib::set_hostname(...)` **before** `init_node` (falling back to
`$ROS_MASTER_URI` / `$ROS_HOSTNAME`, else a local roscore).

Run each as `./build/<name>`.

## Messages

Built-in catalog (each provides `TYPE`, `MD5`, `DEFINITION` and
`serialize()`/`deserialize()`; every md5 matches `rosmsg md5`):

- `std_msgs::` String, Bool, Byte, Char, Int8, Int16, Int32, Int64, UInt8, UInt16, UInt32, UInt64, Float32, Float64, Empty, Time, Duration, Header, ColorRGBA
- `geometry_msgs::` Vector3, Point, Point32, Quaternion, Pose, PoseStamped, PoseArray, Twist, TwistStamped, Accel, Wrench, Transform, TransformStamped, Polygon, PoseWithCovariance, TwistWithCovariance
- `sensor_msgs::` Image, CompressedImage, PointField, PointCloud2, Imu, LaserScan, JointState, NavSatFix, NavSatStatus, Range, Temperature, MagneticField, RegionOfInterest, CameraInfo
- `nav_msgs::` Odometry, Path, OccupancyGrid, MapMetaData, GridCells
- `diagnostic_msgs::` KeyValue, DiagnosticStatus, DiagnosticArray
- `trajectory_msgs::` JointTrajectory, JointTrajectoryPoint, MultiDOFJointTrajectory, MultiDOFJointTrajectoryPoint
- `actionlib_msgs::` GoalID, GoalStatus, GoalStatusArray

This is the **same 64-type catalog as the Python `irap_noroslib.msg`** — the two
libraries are in lock-step. Every md5 matches `rosmsg md5`, and all 64 types plus
a custom message have been round-tripped through a real `roscore` and decoded by
genuine `rospy` subscribers.

Headers: `irap_noroslib/{std_msgs,geometry_msgs,sensor_msgs,nav_msgs,diagnostic_msgs,trajectory_msgs,actionlib_msgs}.hpp`
(all pulled in by `irap_noroslib.hpp`).

### How to use them

**ROS-style includes.** Every type has a header on the exact path roscpp uses,
just prefixed with `irap_noroslib/` — so porting a roscpp node is a one-word edit:

```cpp
// roscpp                              // irap_noroslib
#include "std_msgs/String.h"           #include "irap_noroslib/std_msgs/String.h"
#include "geometry_msgs/Twist.h"       #include "irap_noroslib/geometry_msgs/Twist.h"
#include "sensor_msgs/Image.h"         #include "irap_noroslib/sensor_msgs/Image.h"
#include "std_srvs/Trigger.h"          #include "irap_noroslib/std_srvs/Trigger.h"
```

The type names are unchanged — `std_msgs::String`, `geometry_msgs::Twist` — because
messages live in their real ROS namespaces; only the library's own API is under
`irap_noroslib::`. All seven message packages and `std_srvs` are covered. Each of
these headers just pulls in the single header, so `#include "irap_noroslib.hpp"`
alone still gives you everything.

Each type is a plain struct — construct it, set fields, publish. Nested messages,
`std::vector<>` arrays, `std_msgs::Header`, and `time`/`duration` all work:

```cpp
#include "irap_noroslib.hpp"

// simple scalar wrappers
std_msgs::Int32   i;  i.data = 7;
std_msgs::Float64 f;  f.data = 1.5;
std_msgs::String  s;  s.data = "hello";

// nested messages
geometry_msgs::Twist t;
t.linear.x  = 1.0;          // Vector3 linear
t.angular.z = 0.5;          // Vector3 angular

// a Header-stamped message (time is stored as stamp_sec / stamp_nsec)
nav_msgs::Odometry o;
o.header.seq = 0;
o.header.stamp_now();       // fills stamp_sec / stamp_nsec with wall time
o.header.frame_id = "odom";
o.child_frame_id  = "base_link";
o.pose.pose.position.x = 1.5;   // nested-in-nested
o.twist.twist.linear.x = 0.3;

// variable-length arrays are std::vector; uint8[] is std::vector<uint8_t>
sensor_msgs::JointState js;
js.name     = {"j1", "j2"};
js.position = {0.1, 0.2};
sensor_msgs::Image img;
img.data = {0x00, 0x01, 0x02};

// publish / subscribe
irap_noroslib::Publisher<nav_msgs::Odometry> pub("/odom");
pub.publish(o);
irap_noroslib::Subscriber<nav_msgs::Odometry> sub("/odom",
    [](const nav_msgs::Odometry& m){ irap_noroslib::loginfo(m.child_frame_id); });
```

### Your own message types

Two ways. Both produce the same wire bytes and the same md5, so they interoperate:

| | You give it | md5sum |
|---|---|---|
| **Load the `.msg` file** *(below)* | the file's path | **derived for you** |
| **A hand-written struct** | fields + codec + **the md5** | you supply it |

**A struct** is the typed, zero-overhead option: the three static strings (`TYPE`,
`MD5`, `DEFINITION`) plus `serialize()`/`deserialize()` built on `irap_noroslib::Writer`
/ `irap_noroslib::Reader`. The catch is the **`MD5` — you must supply it yourself**
(`rosmsg md5 <type>`); nothing derives it for you on this path, and a publisher with
a wrong md5 is rejected by real ROS subscribers. See `examples/custom_msg.cpp`, and
`examples/sensor_reading.hpp` for one with a `std_msgs::Header`.

If you'd rather not deal with md5s at all, load the file instead:

### Loading a `.msg` / `.srv` / `.action` **file** at runtime

Copy the files off the robot and give irap_noroslib the **full path of each one** —
no catkin package, no ROS install, no struct to write, and **no md5 to look up**.
It parses the file and derives the md5 exactly the way ROS does.

Say you scp'd these off a robot running the package `my_robot_msgs`, and dropped
them anywhere. There is no `package.xml`, no `msg/` directory — just files:

```
/home/me/robot_msgs/Reading.msg
/home/me/robot_msgs/CustomData.msg
/home/me/robot_msgs/GetStatus.srv
```

```
# /home/me/robot_msgs/CustomData.msg
std_msgs/Header header
int32 id
string label
float64[] samples
Reading[] readings          <-- a custom type from the same package
geometry_msgs/Point where   <-- a built-in; nothing to do, it just resolves
```

**One file, one call**, each with its own full path:

```cpp
#include "irap_noroslib.hpp"
using namespace irap_noroslib;

const std::string MSGS = "/home/me/robot_msgs";   // wherever you put them
const std::string PKG  = "my_robot_msgs";         // the package they came from

MsgType Reading    = load_msg_file(MSGS + "/Reading.msg",    PKG);
MsgType CustomData = load_msg_file(MSGS + "/CustomData.msg", PKG);
SrvType GetStatus  = load_srv_file(MSGS + "/GetStatus.srv",  PKG);

init_node("my_node");

// publish -- fields by name, nest with a dot, index with brackets
DynamicPublisher pub("/data", CustomData);
DynamicMessage m = CustomData.create();
m.set("id", 7).set("label", "hi");
m.set("header.frame_id", "base_link");
m.set_array("samples", std::vector<double>{1.0, 2.0});
m.set("where.x", 3.0);
m.append("readings").msg().set("value", 1.5).set("unit", "C");
pub.publish(m);

// subscribe
DynamicSubscriber sub("/data", CustomData, [](const DynamicMessage& m) {
  loginfo(m.get<std::string>("label"));
  double x     = m.get<double>("where.x");
  auto samples = m.get_array<double>("samples");            // arrays
  auto unit    = m.get<std::string>("readings[0].unit");    // index into an array
  // auto blob = m.bytes("blob");                           // a uint8[] field
});
```

`Reading.msg` is loaded too, because `CustomData` nests it — **every custom type
you use needs its own call**. Order doesn't matter, and if you forget one the error
names the file to add:

```
irap_noroslib: unknown message type "my_robot_msgs/Reading". It is nested by a type
you loaded, so load its file too:
    load_msg_file("/path/to/Reading.msg", "my_robot_msgs");
```

Built-in types (`std_msgs/Header`, `geometry_msgs/Point`, …) need no call at all —
all 64 are already there.

**One file, one call.** Several custom messages? Load each by its own path — order
doesn't matter, and if you forget one the error names the file to load. The second
argument is the ROS package the message came from (ROS names types `pkg/Type`); it
is inferred only when the file still sits in a `<pkg>/msg/<Type>.msg` layout.

| Function | Loads |
|---|---|
| `load_msg_file(path, pkg)` | one `.msg` → `MsgType` |
| `load_msg_files({paths}, pkg)` | several `.msg` files at once |
| `load_srv_file(path, pkg)` | one `.srv` (split on `---`) → `SrvType` |
| `load_action_file(path, pkg)` | one `.action` → `ActionType` + all 7 action types |
| `selftest_builtin_md5(&fails)` | recompute every built-in's md5 from its own text |

Services and actions have runtime counterparts too — `DynamicServiceServer`,
`DynamicServiceClient`, `DynamicActionClient`, `DynamicActionServer` — the usual
classes with the compile-time type replaced by a loaded one.

A runtime-loaded type and a hand-written struct produce **identical wire bytes and
identical md5s**, so they interoperate freely: a `DynamicPublisher` feeds a
`Subscriber<geometry_msgs::Twist>`, and both talk to real roscpp/rospy.

`examples/custom_msg.cpp` shows both ways side by side. Verified against real ROS:
every md5 derived from a bare file matches `rosmsg md5` / `rossrv md5`, and all 64
built-ins pass `selftest_builtin_md5()` — their *computed* md5 equals the hardcoded
one, with no ROS installed.

## Services

A service is a struct with `TYPE` + `MD5` and nested `Request` / `Response`. See
`examples/add_two_ints.hpp`, or use built-in `std_srvs::{Empty,Trigger,SetBool}`.

```cpp
irap_noroslib::ServiceServer<AddTwoInts> srv("/add_two_ints",
    [](const AddTwoInts::Request& q, AddTwoInts::Response& r){ r.sum = q.a + q.b; return true; });

irap_noroslib::ServiceClient<AddTwoInts> c("/add_two_ints");
AddTwoInts::Request q; q.a = 3; q.b = 4; AddTwoInts::Response r;
c.call(q, r);   // r.sum == 7
```

## Actions (actionlib)

An action is a traits struct exposing Goal/Result/Feedback + the wrapper types —
see `examples/fibonacci_action.hpp`.

```cpp
irap_noroslib::SimpleActionClient<fib::Fibonacci> c("/fibonacci");
c.waitForServer(8.0);
fib::Goal g; g.order = 10;
c.sendGoal(g, [](const fib::Feedback& f){ /* ... */ });
c.waitForResult(15.0); c.getResult().sequence;   // + getState(), cancelGoal()
```

## Parameters

```cpp
irap_noroslib::set_param("/demo/rate", 30);                 // int/double/bool/string
int rate = irap_noroslib::get_param_or<int>("/demo/rate", 10);
irap_noroslib::has_param("/demo/rate"); irap_noroslib::delete_param("/demo/rate");
```

## UDPROS (unreliable transport)

```cpp
irap_noroslib::Subscriber<std_msgs::String> sub("/chatter", cb, "udpros");  // 3rd arg
```

Publishers offer UDPROS automatically.

## Run your own ROS master — `nr_roscore`

irap_noroslib can also *be* the roscore. `examples/nr_roscore.cpp` is a standalone ROS
master + parameter server that real ROS nodes and irap_noroslib nodes register with.

```bash
./build/nr_roscore                 # binds :11311, advertises this host
./build/nr_roscore --port 11322
ROS_MASTER_URI=http://host:11311 ROS_HOSTNAME=host ./build/nr_roscore
```

Config precedence — **port:** `--port` › `$ROS_MASTER_URI` › `11311`;
**hostname:** `--host` › `$ROS_HOSTNAME` › `$ROS_IP` › system hostname. Implements
the Master API (register/lookup/getSystemState/…) + a full **nested-dict**
Parameter Server, and auto-starts a `/rosout` → `/rosout_agg` aggregator (disable
with `--no-rosout`). Verified: two real `rostopic pub`/`echo` nodes talk through
it; `rosservice` / `rosparam` (incl. nested dicts) / `rostopic list`/`info` work;
stress-tested with the topic matrix, a service flood, and a registration storm.

## Automatic md5 discovery

Subscribe with a wrong/unknown md5 and irap_noroslib parses the publisher's real md5 from
the rejection and reconnects (`>>> DISCOVERED real md5 ...`). See
`examples/md5_discovery.cpp`.

## Repo layout

```
cpp/
  include/irap_noroslib.hpp      <- THE single header (copy this)
  examples/*.cpp         <- pubsub, services, actions, ... (each #include "irap_noroslib.hpp")
  irap_noroslib_impl.cpp         <- the one TU that #define IRAP_NOROSLIB_IMPLEMENTATION
  CMakeLists.txt         <- builds the examples
  dev/                   <- source of truth (contributors only)
    include/irap_noroslib/*.hpp  <- split declaration headers
    src/*.cpp            <- split implementation
    tools/amalgamate.py  <- regenerates include/irap_noroslib.hpp from dev/
```

The single header is **generated** from `dev/`. To change the library, edit
`dev/` and run `python3 dev/tools/amalgamate.py`.
