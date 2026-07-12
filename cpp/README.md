# noros (C++)

A roscpp-flavoured ROS client library — **no ROS installed, single header, links
zero ROS libraries.** C++17 + POSIX sockets + pthreads. It speaks the real ROS
wire protocols (XML-RPC master/slave + TCPROS/UDPROS), so a real `roscore` and
real ROS nodes treat it as a legitimate node.

## Install: copy one file

The entire library is a single header: [`include/noros.hpp`](include/noros.hpp).
Copy it into your project's include path. That's the whole install.

Because it's header-only (stb-style), the implementation must be compiled in
**exactly one** translation unit. In one `.cpp` of your project:

```cpp
#define NOROS_IMPLEMENTATION
#include "noros.hpp"
```

In every **other** file, just `#include "noros.hpp"` (no define). Compile with
C++17 and link the platform's socket/thread libs:

```bash
# Linux / macOS / WSL
g++ -std=c++17 -pthread your_app.cpp noros_impl.cpp -o your_app

# Windows, MinGW
g++ -std=c++17 your_app.cpp noros_impl.cpp -o your_app.exe -lws2_32

# Windows, MSVC (ws2_32 is auto-linked via #pragma comment)
cl /std:c++17 /EHsc your_app.cpp noros_impl.cpp
```

(where `noros_impl.cpp` is the one file that defines `NOROS_IMPLEMENTATION`.)

### Platforms

noros ships a Winsock/POSIX compatibility layer, so the **same header builds on
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
#include "noros.hpp"
int main() {
  noros::init_node("talker");
  noros::Publisher<std_msgs::String> pub("/chatter");
  noros::Rate rate(10);
  while (noros::ok()) {
    std_msgs::String m; m.data = "hello world";
    pub.publish(m);
    rate.sleep();
  }
}
```

**Subscriber** (`listener.cpp`):

```cpp
#include "noros.hpp"
int main() {
  noros::init_node("listener");
  noros::Subscriber<std_msgs::String> sub("/chatter",
      [](const std_msgs::String& m){ noros::loginfo("I heard: " + m.data); });
  noros::spin();
}
```

Point at a master (defaults to `http://127.0.0.1:11311`) via `$ROS_MASTER_URI` /
`$ROS_HOSTNAME`, or from code: `noros::set_master_uri(...)` / `set_hostname(...)`
before `init_node`.

## Build the examples

```bash
cd cpp
cmake -S . -B build && cmake --build build -j
./build/talker        # ./build/listener, add_two_ints_*, stamped_*, fibonacci_*, ...
```

The CMake build compiles the implementation once (`noros_impl.cpp`) and links it
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
| `nr_roscore.cpp` | run your own ROS master (roscore) + `/rosout` aggregator |

These mirror the Python examples one-for-one (same names, same behaviour), so the
two libraries stay in lock-step. Every example calls `noros::set_master_uri(...)`
and `noros::set_hostname(...)` **before** `init_node` (falling back to
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

This is the **same 64-type catalog as the Python `noros.msg`** — the two
libraries are in lock-step. Every md5 matches `rosmsg md5`, and all 64 types plus
a custom message have been round-tripped through a real `roscore` and decoded by
genuine `rospy` subscribers.

Headers: `noros/{std_msgs,geometry_msgs,sensor_msgs,nav_msgs,diagnostic_msgs,trajectory_msgs,actionlib_msgs}.hpp`
(all pulled in by `noros.hpp`).

### How to use them

Each type is a plain struct — construct it, set fields, publish. Nested messages,
`std::vector<>` arrays, `std_msgs::Header`, and `time`/`duration` all work:

```cpp
#include "noros.hpp"

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
noros::Publisher<nav_msgs::Odometry> pub("/odom");
pub.publish(o);
noros::Subscriber<nav_msgs::Odometry> sub("/odom",
    [](const nav_msgs::Odometry& m){ noros::loginfo(m.child_frame_id); });
```

### Your own message types

Not in the catalog? Write a small struct with the three static strings
(`TYPE`, `MD5`, `DEFINITION`) plus `serialize()`/`deserialize()` using
`noros::Writer` / `noros::Reader`. See `examples/custom_msg.cpp`, and
`examples/sensor_reading.hpp` for one with a `std_msgs::Header`.

## Services

A service is a struct with `TYPE` + `MD5` and nested `Request` / `Response`. See
`examples/add_two_ints.hpp`, or use built-in `std_srvs::{Empty,Trigger,SetBool}`.

```cpp
noros::ServiceServer<AddTwoInts> srv("/add_two_ints",
    [](const AddTwoInts::Request& q, AddTwoInts::Response& r){ r.sum = q.a + q.b; return true; });

noros::ServiceClient<AddTwoInts> c("/add_two_ints");
AddTwoInts::Request q; q.a = 3; q.b = 4; AddTwoInts::Response r;
c.call(q, r);   // r.sum == 7
```

## Actions (actionlib)

An action is a traits struct exposing Goal/Result/Feedback + the wrapper types —
see `examples/fibonacci_action.hpp`.

```cpp
noros::SimpleActionClient<fib::Fibonacci> c("/fibonacci");
c.waitForServer(8.0);
fib::Goal g; g.order = 10;
c.sendGoal(g, [](const fib::Feedback& f){ /* ... */ });
c.waitForResult(15.0); c.getResult().sequence;   // + getState(), cancelGoal()
```

## Parameters

```cpp
noros::set_param("/demo/rate", 30);                 // int/double/bool/string
int rate = noros::get_param_or<int>("/demo/rate", 10);
noros::has_param("/demo/rate"); noros::delete_param("/demo/rate");
```

## UDPROS (unreliable transport)

```cpp
noros::Subscriber<std_msgs::String> sub("/chatter", cb, "udpros");  // 3rd arg
```

Publishers offer UDPROS automatically.

## Run your own ROS master — `nr_roscore`

noros can also *be* the roscore. `examples/nr_roscore.cpp` is a standalone ROS
master + parameter server that real ROS nodes and noros nodes register with.

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

Subscribe with a wrong/unknown md5 and noros parses the publisher's real md5 from
the rejection and reconnects (`>>> DISCOVERED real md5 ...`). See
`examples/md5_discovery.cpp`.

## Repo layout

```
cpp/
  include/noros.hpp      <- THE single header (copy this)
  examples/*.cpp         <- pubsub, services, actions, ... (each #include "noros.hpp")
  noros_impl.cpp         <- the one TU that #define NOROS_IMPLEMENTATION
  CMakeLists.txt         <- builds the examples
  dev/                   <- source of truth (contributors only)
    include/noros/*.hpp  <- split declaration headers
    src/*.cpp            <- split implementation
    tools/amalgamate.py  <- regenerates include/noros.hpp from dev/
```

The single header is **generated** from `dev/`. To change the library, edit
`dev/` and run `python3 dev/tools/amalgamate.py`.
