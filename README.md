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

> **Never used ROS before?** Start at
> **[Step 1 — Install](#step-1--install-noroslib)** and follow the steps in order.
> The guide assumes you know *nothing* about ROS, and every step is a command you
> can copy and run.

---

## Contents

**Getting started (follow in order)**

| | Step | |
|---|---|---|
| 0 | [What ROS is, in 60 seconds](#step-0--what-ros-is-in-60-seconds) | read this first if ROS is new to you |
| 1 | [Install NoROSLib](#step-1--install-noroslib) | Python: `pip install`. C++: copy one header. |
| 2 | [Get a ROS master running](#step-2--get-a-ros-master-running) | the "phone book" everything registers with |
| 3 | [Point your program at the master](#step-3--point-your-program-at-the-master) | one environment variable |
| 4 | [Check that it works](#step-4--check-that-it-works) | `nr_rostopic list` |
| 5 | [Send data — your first publisher](#step-5--send-data--your-first-publisher) | Python + C++ |
| 6 | [Receive data — your first subscriber](#step-6--receive-data--your-first-subscriber) | Python + C++ |
| 7 | [Inspect the traffic](#step-7--inspect-the-traffic) | echo, hz, info, pub |
| 8 | [Choose a message type](#step-8--choose-a-message-type) | 64 built-ins |
| 9 | [Use your robot's own message type](#step-9--use-your-robots-own-message-type) | load a `.msg` file |
| 10 | [Call a service](#step-10--call-a-service) | request → reply |
| 11 | [Run an action](#step-11--run-an-action) | long job + feedback + cancel |
| 12 | [Read and write parameters](#step-12--read-and-write-parameters) | shared config |
| 13 | [Connect to a real robot on the network](#step-13--connect-to-a-real-robot-on-the-network) | the one gotcha that trips everyone |
| 14 | [When something goes wrong](#step-14--when-something-goes-wrong) | symptom → cause → fix |

**Reference** — [Why NoROSLib](#why-noroslib) · [Capabilities](#capabilities) ·
[Custom messages in depth](#custom-messages-in-depth) ·
[`nr_rostopic`](#nr_rostopic--rostopic-with-no-ros-installed) ·
[`nr_roscore`](#nr_roscore--run-your-own-ros-master) ·
[Automatic md5 discovery](#automatic-md5-discovery) ·
[All examples](#all-examples) · [How it works](#how-it-works)

---

# Getting started

## Step 0 — What ROS is, in 60 seconds

Skip this if you already know ROS. If you don't, these seven words are all you
need to follow the rest of the guide.

| Word | What it actually means |
|---|---|
| **Node** | One running program. Your program will be a node. |
| **Master** (`roscore`) | A **phone book**. Nodes register with it and ask it "who has `/chatter`?" It stores addresses, **not** your data. Exactly one runs per system. |
| **Topic** | A named channel, like `/chatter` or `/camera/image_raw`. Data flows one way, and any number of nodes can send or receive on it. |
| **Message** | The data itself. Every message has a **type**, like `std_msgs/String` (a piece of text) or `sensor_msgs/Image` (a camera frame). |
| **Publish / subscribe** | *Publish* = send on a topic. *Subscribe* = receive from a topic. Publishers and subscribers don't know each other; they only know the topic name. |
| **Service** | A request → reply call, like calling a function on another node. Returns immediately. |
| **Action** | A long-running job (drive somewhere, pick something up) that reports **progress** while it runs and can be **cancelled**. |

One more thing, and it's the thing beginners get stuck on:

> **The master only introduces nodes; it does not carry their data.** When your
> node subscribes to `/chatter`, the master tells it *"the publisher is at
> 192.168.1.50:41001"* — and then your node opens a **direct connection to that
> publisher**. Data never flows through the master. This is why, in
> [Step 13](#step-13--connect-to-a-real-robot-on-the-network), the robot has to be
> able to reach *your* machine back, not just the other way round.

---

## Step 1 — Install NoROSLib

**You do not need ROS installed. Not now, not later.** Pick your language.

### Python

```bash
pip install irap_noroslib          # from PyPI (once published)
pip install ./python               # from a clone of this repo
```

**Dependencies: none.** It is pure **Python 3.6+ standard library** — it talks to
ROS using only `xmlrpc`, `socket`, `socketserver`, `threading`, `struct` and
`hashlib`, all built into Python itself.
[`python/requirements.txt`](python/requirements.txt) is intentionally empty and
says exactly that. (OpenCV/numpy are needed *only* by the optional webcam demos,
never by the library.)

Check it worked:

```bash
python3 -c "import irap_noroslib; print('ok')"
```

Installing also puts two commands on your **PATH** — you will use them in
[Step 2](#step-2--get-a-ros-master-running) and
[Step 4](#step-4--check-that-it-works):

```bash
nr_roscore --help          # your own ROS master
nr_rostopic --help         # rostopic, without ROS
```

### C++

The whole library is **one file**:
[`cpp/include/irap_noroslib.hpp`](cpp/include/irap_noroslib.hpp). Copy it next to
your code. That's the install.

```bash
cp cpp/include/irap_noroslib.hpp   ~/my_project/
```

In a single-file program, ask for the implementation **once**, at the top:

```cpp
#define IRAP_NOROSLIB_IMPLEMENTATION     // exactly one .cpp in your project does this
#include "irap_noroslib.hpp"
```

Compile with a C++17 compiler and threads:

```bash
g++ -std=c++17 my_node.cpp -o my_node -pthread
```

**Dependencies: none beyond a C++17 toolchain.** No ROS, no Boost, no third-party
libraries — just the compiler and the OS's own sockets/threads. On Linux/macOS/WSL
pass `-pthread`; on MinGW add `-lws2_32` (MSVC links Winsock automatically). Full
details in [`cpp/DEPENDENCIES.md`](cpp/DEPENDENCIES.md).

> **Bigger project (more than one `.cpp`)?** Put those two lines in **exactly one**
> `.cpp` file; every other file just does `#include "irap_noroslib.hpp"`. That is the
> whole rule.

> **Optional:** if you'd rather write ROS-style includes
> (`#include "irap_noroslib/std_msgs/String.h"`), copy the small
> [`cpp/include/irap_noroslib/`](cpp/include/irap_noroslib) folder too. Those are
> one-line convenience headers; `irap_noroslib.hpp` alone already contains every type.

---

## Step 2 — Get a ROS master running

Every ROS system needs **exactly one master** running somewhere. Two situations:

### A) You have a robot / PC that already runs ROS

Then the master is already up over there — someone ran `roscore` on it. **Do
nothing here**, find its IP address, and go to
[Step 3](#step-3--point-your-program-at-the-master).

### B) You have nothing — no ROS anywhere

Run **NoROSLib's own master**. It is a real ROS master: real ROS nodes,
`rostopic`/`rosservice`/`rosparam`, and NoROSLib nodes all register with it.

```bash
nr_roscore                                  # port 11311, all interfaces
nr_roscore --bind 127.0.0.1 --port 11311    # this machine only
```

C++ users who built the examples can run `./cpp/build/nr_roscore --port 11311`
instead — same thing.

**Leave it running in its own terminal.** You should see — and it tells you the
exact line to paste into the next step:

```
[nr_roscore] ROS master online
[nr_roscore] ROS_MASTER_URI=http://127.0.0.1:11311/  (bind 127.0.0.1:11311)
[nr_roscore] point nodes here:  export ROS_MASTER_URI=http://127.0.0.1:11311/
[nr_roscore] started /rosout aggregator (-> /rosout_agg)
```

Every other step in this guide assumes a master is up. If nothing works later,
come back and check this terminal is still alive.

---

## Step 3 — Point your program at the master

A node finds the master through **one environment variable**, `ROS_MASTER_URI`.
This is standard ROS, and NoROSLib reads it automatically.

```bash
# Linux / macOS
export ROS_MASTER_URI=http://localhost:11311        # master on this machine
export ROS_MASTER_URI=http://192.168.1.50:11311     # master on a robot at .50
```

```powershell
# Windows PowerShell
$env:ROS_MASTER_URI = "http://192.168.1.50:11311"
```

Set it **in every terminal** you run a node from. Forgetting it is the #1
beginner mistake — the node then quietly tries `http://localhost:11311` and finds
nothing.

**Prefer to do it in code?** Both languages let you, and it overrides the variable:

```python
irap_noroslib.set_master_uri("http://192.168.1.50:11311")   # before init_node()
```

```cpp
irap_noroslib::set_master_uri("http://192.168.1.50:11311"); // before init_node()
```

### Check for a leftover `ROS_IP` first

There is a second variable, `ROS_IP` (and its sibling `ROS_HOSTNAME`), that says
**how other nodes reach you**. You don't need it when everything runs on one
machine — *unless your shell already sets it*, which is common on any machine that
has touched ROS before:

```bash
echo "$ROS_IP $ROS_HOSTNAME"      # both empty? good, skip this.
```

If it prints an address, that address is what every node you start will advertise —
and if it's stale, wrong, or firewalled, **nothing will connect and nothing will
say why**. For a tutorial on one machine, force it to loopback:

```bash
export ROS_IP=localhost           # $ROS_IP wins over $ROS_HOSTNAME, as in real ROS
```

(This is not hypothetical: it is the single most common reason the steps below
"do nothing".) When you move to a real robot,
[Step 13](#step-13--connect-to-a-real-robot-on-the-network) sets it to your real IP.

---

## Step 4 — Check that it works

Before writing any code, confirm your terminal can see the master:

```bash
nr_rostopic list
```

Expected output (a bare master with nothing else running):

```
/rosout
/rosout_agg
```

**That's success** — those two topics always exist. If instead you get
`connection refused`, the master isn't running or `ROS_MASTER_URI` is wrong: go
back to [Step 2](#step-2--get-a-ros-master-running) /
[Step 3](#step-3--point-your-program-at-the-master).

Pointing at the master explicitly works too, if you'd rather not use the variable:

```bash
nr_rostopic --master 192.168.1.50 --port 11311 list
```

---

## Step 5 — Send data — your first publisher

You will publish text on the topic `/chatter`, ten times a second. This is the
"hello world" of ROS.

### Python

Save as `my_talker.py`:

```python
import irap_noroslib
from irap_noroslib.std_msgs.msg import String    # a message type: plain text

irap_noroslib.init_node("my_talker")             # join the graph, pick a node name
pub = irap_noroslib.Publisher("/chatter", String)  # I will send String on /chatter
rate = irap_noroslib.Rate(10)                    # 10 times per second

i = 0
while not irap_noroslib.is_shutdown():           # until Ctrl-C
    pub.publish(String(data="hello world %d" % i))
    i += 1
    rate.sleep()                                 # keeps the 10 Hz pace
```

```bash
python3 my_talker.py
```

### C++

Save as `my_talker.cpp`:

```cpp
#define IRAP_NOROSLIB_IMPLEMENTATION
#include "irap_noroslib.hpp"

int main() {
  irap_noroslib::init_node("my_talker");              // join the graph
  irap_noroslib::Publisher<std_msgs::String> pub("/chatter");
  irap_noroslib::Rate rate(10);                       // 10 times per second

  int i = 0;
  while (irap_noroslib::ok()) {                       // until Ctrl-C
    std_msgs::String m;
    m.data = "hello world " + std::to_string(i++);
    pub.publish(m);
    rate.sleep();
  }
}
```

```bash
g++ -std=c++17 my_talker.cpp -o my_talker -pthread
./my_talker
```

It looks like it's doing nothing. It isn't — **nobody is listening yet.** Leave it
running and open a new terminal.

> The same program ships ready-made as `python/examples/talker.py` and
> `cpp/examples/talker.cpp`.

---

## Step 6 — Receive data — your first subscriber

In a **new terminal** (remember `export ROS_MASTER_URI=...` again — every terminal
needs it), listen to what the talker is sending.

### Python

Save as `my_listener.py`:

```python
import irap_noroslib
from irap_noroslib.std_msgs.msg import String

def on_message(m):                               # called for every message
    irap_noroslib.loginfo("I heard: " + m.data)

irap_noroslib.init_node("my_listener")
irap_noroslib.Subscriber("/chatter", String, on_message)
irap_noroslib.spin()                             # wait for messages forever
```

```bash
python3 my_listener.py
```

### C++

Save as `my_listener.cpp`:

```cpp
#define IRAP_NOROSLIB_IMPLEMENTATION
#include "irap_noroslib.hpp"

int main() {
  irap_noroslib::init_node("my_listener");
  irap_noroslib::Subscriber<std_msgs::String> sub("/chatter",
      [](const std_msgs::String& m) {                     // called per message
        irap_noroslib::loginfo("I heard: " + m.data);
      });
  irap_noroslib::spin();                                  // wait forever
}
```

```bash
g++ -std=c++17 my_listener.cpp -o my_listener -pthread
./my_listener
```

You should now see, ten times a second:

```
[INFO] [1784011311.460142]: I heard: hello world 245
[INFO] [1784011311.560102]: I heard: hello world 246
```

**That is a working ROS system.** The two mix freely: a **Python** talker feeds a
**C++** listener, and either one feeds a real `rospy`/`roscpp` node on a robot —
they are all just ROS nodes on the wire.

---

## Step 7 — Inspect the traffic

`nr_rostopic` is the real `rostopic` tool, with no ROS installed — same
subcommands, same flags. With the talker from Step 5 still running:

```bash
nr_rostopic list                 # what topics exist?          -> /chatter ...
nr_rostopic echo /chatter        # print the messages          (Ctrl-C to stop)
nr_rostopic echo -n 5 /chatter   # print 5, then exit
nr_rostopic type /chatter        # what type is it?            -> std_msgs/String
nr_rostopic info /chatter        # who publishes / subscribes?
nr_rostopic hz /chatter          # how fast?                   -> ~10 Hz
nr_rostopic bw /chatter          # how many bytes/s?
```

`echo` prints:

```
data: "hello world 25"
---
data: "hello world 26"
---
```

You can also **send** a message by hand — handy for testing a subscriber with no
publisher written yet:

```bash
nr_rostopic pub -r 10 /chatter std_msgs/String "data: hi"   # 10 Hz
nr_rostopic pub -1  /chatter std_msgs/String "data: hi"     # once
```

Full reference: [`nr_rostopic`](#nr_rostopic--rostopic-with-no-ros-installed) —
including the one thing it does that **real `rostopic` cannot**.

---

## Step 8 — Choose a message type

`std_msgs/String` was just text. Real robots send poses, scans, images. **64 types
are built in** — the standard ROS catalog, every md5sum matching `rosmsg md5`
exactly, so real ROS nodes accept them.

| Package | Types |
|---|---|
| **std_msgs** (19) | Bool, Byte, Char, ColorRGBA, Duration, Empty, Float32, Float64, Header, Int8, Int16, Int32, Int64, String, Time, UInt8, UInt16, UInt32, UInt64 |
| **geometry_msgs** (16) | Accel, Point, Point32, Polygon, Pose, PoseArray, PoseStamped, PoseWithCovariance, Quaternion, Transform, TransformStamped, Twist, TwistStamped, TwistWithCovariance, Vector3, Wrench |
| **sensor_msgs** (14) | CameraInfo, CompressedImage, Image, Imu, JointState, LaserScan, MagneticField, NavSatFix, NavSatStatus, PointCloud2, PointField, Range, RegionOfInterest, Temperature |
| **nav_msgs** (5) | GridCells, MapMetaData, OccupancyGrid, Odometry, Path |
| **diagnostic_msgs** (3) | DiagnosticArray, DiagnosticStatus, KeyValue |
| **trajectory_msgs** (4) | JointTrajectory, JointTrajectoryPoint, MultiDOFJointTrajectory, MultiDOFJointTrajectoryPoint |
| **actionlib_msgs** (3) | GoalID, GoalStatus, GoalStatusArray |

Use one exactly like `String`. Driving a robot, for instance, is a
`geometry_msgs/Twist` on `/cmd_vel`:

```python
import irap_noroslib
from irap_noroslib.geometry_msgs.msg import Twist

irap_noroslib.init_node("driver")
pub = irap_noroslib.Publisher("/cmd_vel", Twist)

cmd = Twist()
cmd.linear.x = 0.2        # 0.2 m/s forward     (nested fields: linear.x)
cmd.angular.z = 0.5       # 0.5 rad/s turning
pub.publish(cmd)
```

```cpp
#define IRAP_NOROSLIB_IMPLEMENTATION
#include "irap_noroslib.hpp"

int main() {
  irap_noroslib::init_node("driver");
  irap_noroslib::Publisher<geometry_msgs::Twist> pub("/cmd_vel");

  geometry_msgs::Twist cmd;
  cmd.linear.x  = 0.2;    // 0.2 m/s forward
  cmd.angular.z = 0.5;    // 0.5 rad/s turning
  pub.publish(cmd);
}
```

The Python and C++ catalogs are **identical, all 64 types** — same names, same
md5s. Details and more usage: [python/README.md](python/README.md#messages),
[cpp/README.md](cpp/README.md#messages).

**Type you need isn't in the list?** That's the next step.

---

## Step 9 — Use your robot's own message type

Robots define their own message types (`my_robot_msgs/CustomData`). You **do not**
need their catkin package, and you don't need ROS. You need **the `.msg` file**.

**1. Copy the `.msg` files off the robot** — `scp`, a USB stick, anything. Put them
anywhere:

```
/home/me/robot_msgs/Reading.msg
/home/me/robot_msgs/CustomData.msg
```

**2. Load each file by its full path.** One file, one call:

```python
import irap_noroslib
from irap_noroslib import load_msg_file

MSGS = "/home/me/robot_msgs"      # wherever you put them
PKG  = "my_robot_msgs"            # the ROS package they came from

Reading    = load_msg_file(f"{MSGS}/Reading.msg",    PKG)
CustomData = load_msg_file(f"{MSGS}/CustomData.msg", PKG)

irap_noroslib.init_node("my_node")
irap_noroslib.Subscriber("/data", CustomData, lambda m: print(m.id, m.label))
irap_noroslib.spin()
```

```cpp
#define IRAP_NOROSLIB_IMPLEMENTATION
#include "irap_noroslib.hpp"
using namespace irap_noroslib;

const std::string MSGS = "/home/me/robot_msgs";
const std::string PKG  = "my_robot_msgs";

int main() {
  MsgType Reading    = load_msg_file(MSGS + "/Reading.msg",    PKG);
  MsgType CustomData = load_msg_file(MSGS + "/CustomData.msg", PKG);

  init_node("my_node");
  DynamicSubscriber sub("/data", CustomData, [](const DynamicMessage& m) {
    loginfo(m.get<std::string>("label"));       // fields by name
  });
  spin();
}
```

That's it. NoROSLib **derives the md5sum and the wire format from the file** using
the exact algorithm real ROS uses, so the robot accepts your node as native.

Three rules, and they cover everything that can go wrong here:

- **One call per file.** `CustomData` nests `Reading`, so `Reading.msg` gets loaded
  too. Order doesn't matter. Forget one and the error names the file you missed.
- **Built-in types need no call.** `std_msgs/Header`, `geometry_msgs/Point`, … all
  64 are already there.
- **Get the package name right.** It isn't in the md5, so a typo won't break the
  connection — it will quietly make `rostopic echo` fail and `rosbag` record the
  wrong type. Details: [Does the package name matter?](#does-the-package-name-matter--yes-get-it-right)

Services (`.srv`) and actions (`.action`) load the same way, with
`load_srv_file()` and `load_action_file()`. Full detail — including defining a
type in code with no file at all — in
[Custom messages in depth](#custom-messages-in-depth). Working example:
`custom_msg` (both languages).

> **No `.msg` file either?** You can still *read* the topic:
> `nr_rostopic echo` decodes a type it has never seen, because ROS publishers hand
> over their full message definition when they connect. See
> [`echo` does something real `rostopic` cannot](#echo-does-something-real-rostopic-cannot).

---

## Step 10 — Call a service

A topic is a stream. A **service** is a request → reply call, like a function on
another node. Example: `add_two_ints` (a + b → sum).

A service type is just **two message bodies**: the request, then the response.
Describe it once and NoROSLib derives the md5 (matching `rossrv md5`). If you have
the robot's `.srv` **file**, use `load_srv_file(path, pkg)` instead — same result,
see [Step 9](#step-9--use-your-robots-own-message-type).

### Python

**The server** — waits for requests:

```python
import irap_noroslib
from irap_noroslib import define_service

AddTwoInts = define_service("rospy_tutorials/AddTwoInts",
                            "int64 a\nint64 b",     # request fields
                            "int64 sum")            # response fields

def handle(req):                                    # called per request
    return AddTwoInts.Response(sum=req.a + req.b)

irap_noroslib.init_node("add_server")
irap_noroslib.Service("/add_two_ints", AddTwoInts, handle)
irap_noroslib.spin()
```

**The client** — asks, and waits for the answer:

```python
irap_noroslib.init_node("add_client")
irap_noroslib.wait_for_service("/add_two_ints", timeout=5.0)   # let the server come up

add = irap_noroslib.ServiceProxy("/add_two_ints", AddTwoInts)
resp = add(AddTwoInts.Request(a=3, b=4))            # blocks until the server replies
print("sum =", resp.sum)                            # sum = 7
```

### C++

`AddTwoInts` here is a small typed struct — see `cpp/examples/add_two_ints.hpp`
(or load the `.srv` file and use `DynamicServiceServer` / `DynamicServiceClient`).

```cpp
// server
irap_noroslib::ServiceServer<AddTwoInts> srv("/add_two_ints",
    [](const AddTwoInts::Request& req, AddTwoInts::Response& resp) {
      resp.sum = req.a + req.b;
      return true;                          // false = the call failed
    });
irap_noroslib::spin();

// client
irap_noroslib::ServiceClient<AddTwoInts> client("/add_two_ints");
AddTwoInts::Request req;  req.a = 3;  req.b = 4;
AddTwoInts::Response resp;
if (client.call(req, resp))                 // blocks until the server replies
  irap_noroslib::loginfo("sum = " + std::to_string(resp.sum));
```

Run the ready-made pair, server first:

```bash
python3 python/examples/add_two_ints_server.py     # terminal 1
python3 python/examples/add_two_ints_client.py     # terminal 2

./cpp/build/add_two_ints_server                    # or the C++ pair
./cpp/build/add_two_ints_client
```

Both work against real ROS: a real `rosservice call /add_two_ints "a: 3 b: 4"`
reaches your server, and your client calls a real `rospy` service.

---

## Step 11 — Run an action

An **action** is for a job that takes time — *drive to the kitchen*, *pick that
up*. Unlike a service it sends **feedback while it runs** and can be **cancelled**.
The classic example computes a Fibonacci sequence one step at a time.

```bash
python3 python/examples/fibonacci_server.py      # terminal 1: does the work
python3 python/examples/fibonacci_client.py      # terminal 2: sends the goal

./cpp/build/fibonacci_server                     # the same pair in C++
./cpp/build/fibonacci_client
```

The client sends a **goal**, gets **feedback** on every step, and finally a
**result** plus a state (`3` = SUCCEEDED):

```
[INFO] feedback: [0, 1, 1, 2, 3]
[INFO] feedback: [0, 1, 1, 2, 3, 5]
[INFO] feedback: [0, 1, 1, 2, 3, 5, 8]
[INFO] state=3 (3=SUCCEEDED) result=[0, 1, 1, 2, 3, 5, 8, 13, 21, 34]
```

These interoperate with real `actionlib`: your client drives a real ROS action
server, and a real ROS client (or `axclient.py`) drives your server. Read
`fibonacci_client.py` / `.cpp` — they are short — and copy the shape.

---

## Step 12 — Read and write parameters

Parameters are **shared settings** kept on the master — a robot's name, a speed
limit, a camera resolution. Any node can read or write them.

```python
irap_noroslib.set_param("/robot_name", "turtle")
irap_noroslib.set_param("/max_speed", 1.5)

name = irap_noroslib.get_param("/robot_name")        # "turtle"
speed = irap_noroslib.get_param("/max_speed", 1.0)   # 1.0 if unset (a default)

if irap_noroslib.has_param("/max_speed"):
    irap_noroslib.delete_param("/max_speed")
```

```cpp
irap_noroslib::set_param("/robot_name", "turtle");
irap_noroslib::set_param("/max_speed", 1.5);

// get_param_or<T>(key, fallback) -- returns the fallback if unset or wrong type
std::string name = irap_noroslib::get_param_or<std::string>("/robot_name", "unknown");
double speed     = irap_noroslib::get_param_or<double>("/max_speed", 1.0);

if (irap_noroslib::has_param("/max_speed")) irap_noroslib::delete_param("/max_speed");
```

From the shell (real ROS tools work against NoROSLib and vice-versa):

```bash
rosparam set /max_speed 1.5
rosparam get /robot_name
```

Full example: `params_example` in both languages.

---

## Step 13 — Connect to a real robot on the network

Everything so far ran on one machine. Now the master is **on the robot** and your
code is **on your laptop**. Two variables, and one of them is the thing that trips
everybody.

```bash
export ROS_MASTER_URI=http://192.168.1.50:11311   # where the ROBOT's master is
export ROS_IP=192.168.1.77                        # YOUR laptop's IP on that network
```

**Why `ROS_IP` matters.** Remember from [Step 0](#step-0--what-ros-is-in-60-seconds):
the master only hands out addresses — the actual data goes **node to node,
directly**. So when you subscribe, the robot's publisher has to **open a connection
back to your laptop**, and it uses the address you registered with. If you don't
set `ROS_IP`, NoROSLib registers your machine's hostname — and the robot usually
**cannot resolve** `my-laptop.local`. Symptom: the topic appears in
`nr_rostopic list`, but **no messages ever arrive.** Set `ROS_IP` to the IP the
robot can actually reach you on, and it works.

Find your IP with `ip addr` (Linux/macOS) or `ipconfig` (Windows) — it's the one
on the same subnet as the robot, here `192.168.1.x`.

Checklist, in order — do it once and it stays done:

1. **Ping both ways.** `ping 192.168.1.50` from the laptop, and `ping 192.168.1.77`
   from the robot. **Both must work.** One-way is exactly the failure `ROS_IP`
   causes, so prove it now.
2. **`export ROS_MASTER_URI=http://<robot-ip>:11311`** on the laptop.
3. **`export ROS_IP=<your-laptop-ip>`** on the laptop.
4. **`nr_rostopic list`** — you should now see the robot's real topics.
5. **`nr_rostopic echo /some_topic`** — data should flow. If step 4 works but this
   one hangs, it is `ROS_IP` or a **firewall** blocking the robot from connecting
   back to you.

Firewalls: ROS uses port **11311** for the master and **random high ports** for the
node-to-node connections. On a trusted lab network, allow the robot's IP through
your firewall entirely; don't try to pin the ports.

Nothing needs to be installed on the robot. It sees a normal ROS node.

---

## Step 14 — When something goes wrong

| What you see | What it means | Fix |
|---|---|---|
| `connection refused` on any command | No master at that address. | Is `nr_roscore`/`roscore` running? Is `ROS_MASTER_URI` right? ([Step 2](#step-2--get-a-ros-master-running)) |
| `nr_rostopic list` shows only `/rosout`, `/rosout_agg` | You're talking to the master fine, but no nodes are publishing. | Start the talker. If it *is* running, it's pointed at a **different** master — check `ROS_MASTER_URI` in **that** terminal. |
| Topic is listed, but `echo` prints **nothing** | The publisher can't connect **back** to you. | Set **`ROS_IP`** to your machine's IP; open the firewall. ([Step 13](#step-13--connect-to-a-real-robot-on-the-network)) |
| Everything hangs / a service call **times out**, even with everything on one machine | Your shell has a **leftover `ROS_IP`** (or `ROS_HOSTNAME`) from an old ROS setup. Your nodes advertise *that* address, and if it's stale or firewalled — even though it's your own LAN IP — peers can't reach them. | `echo $ROS_IP`. For local work: `export ROS_IP=localhost`. ([Step 3](#step-3--point-your-program-at-the-master)) |
| `Cannot load message class for [pkg/Type]` from **real** `rostopic` | Real ROS needs that type *built* in a catkin package. Not your bug. | Use `nr_rostopic echo` — it decodes types it has no file for. ([here](#echo-does-something-real-rostopic-cannot)) |
| `unknown message type "pkg/Thing"` from NoROSLib | A nested custom type whose `.msg` you didn't load. | `load_msg_file()` that file too — the error names it. ([Step 9](#step-9--use-your-robots-own-message-type)) |
| md5sum mismatch, publisher **drops** you | Your subscriber's type doesn't match the publisher's. | Nothing to do — NoROSLib reads the real md5 out of the error and reconnects. ([md5 discovery](#automatic-md5-discovery)) |
| Real ROS subscribers ignore what you **publish** | You advertised a wrong md5 (hand-written C++ struct). | Load the `.msg` file instead and let it derive the md5. ([Step 9](#step-9--use-your-robots-own-message-type)) |
| C++: `undefined reference to irap_noroslib::...` | You never compiled the implementation. | `#define IRAP_NOROSLIB_IMPLEMENTATION` in **exactly one** `.cpp`. ([Step 1](#step-1--install-noroslib)) |
| C++: `undefined reference to pthread_*` | Missing the threads flag. | Add `-pthread` (and `-lws2_32` on MinGW). |
| Node names collide / one node dies on startup | Two nodes registered with the same name. | Give each `init_node()` a unique name. |

While debugging, keep the default log level (`"info"`) — it prints the master URI,
every connection, and every md5 rejection, which is usually the whole answer. All
of it goes to **stderr**, so it never pollutes a piped `nr_rostopic echo`.

Once things work, you can quieten the library — `"info"` (default), `"warn"`,
`"error"`, `"none"`:

```python
irap_noroslib.set_log_level("warn")
```

```cpp
irap_noroslib::set_log_level("warn");
```

---

# Reference

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

### Native ROS — not a bridge

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
  your machine (Windows / macOS / any Linux)      the robot (Ubuntu + ROS)
  ┌──────────────────────────────┐               ┌────────────────────────┐
  │  your app + irap_noroslib    │  XML-RPC +    │  roscore               │
  │  (no ROS, no bridge)         │◄── TCPROS ───►│  native rospy/roscpp   │
  └──────────────────────────────┘  (real ROS)   │  nodes (unmodified)    │
                                                 └────────────────────────┘
```

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
| **`nr_rostopic`** | list / echo / pub / info / hz / bw / find — `echo` works on types with no `.msg` | ✅ | ✅ |
| **Master (`nr_roscore`)** | *be* the roscore: Master + Param Server (dict trees) + `/rosout` | ✅ | ✅ |

Message imports follow the **same paths rospy and roscpp use**, just prefixed —
porting a node is a one-word edit:

```python
from std_msgs.msg import String                 # rospy
from irap_noroslib.std_msgs.msg import String   # irap_noroslib
```

```cpp
#include "std_msgs/String.h"                    // roscpp
#include "irap_noroslib/std_msgs/String.h"      // irap_noroslib -> still std_msgs::String
```

## Custom messages in depth

[Step 9](#step-9--use-your-robots-own-message-type) covers the common case. Here is
the whole picture — your own message type, **three ways**. All three produce the
**same wire bytes and the same md5sum**, so they interoperate freely with each
other and with real ROS. Example: `custom_msg` (shows two of them side by side).

| | You give it | md5sum | Both languages? |
|---|---|---|:---:|
| **1. Load the `.msg` file** | the file's path | **derived for you** | ✅ |
| **2. From `.msg` text, in code** | the text | **derived for you** | Python |
| **3. A hand-written struct** | fields + codec + **the md5** | you supply it | C++ |

### 1. Load the `.msg` file — easiest, and works in both languages

Copy the definition files off the robot and hand NoROSLib the **full path of each
one**. No catkin package, no ROS install, nothing to write.

**Say you scp'd three files off a robot** running the package `my_robot_msgs`, and
dropped them anywhere — there is no `package.xml`, no `msg/` directory, they are
just files:

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

**Load each file by its own full path.** One file, one call:

```python
import irap_noroslib
from irap_noroslib import load_msg_file, load_srv_file

MSGS = "/home/me/robot_msgs"                       # wherever you put them
PKG  = "my_robot_msgs"                             # the package they came from

Reading    = load_msg_file(f"{MSGS}/Reading.msg",    PKG)
CustomData = load_msg_file(f"{MSGS}/CustomData.msg", PKG)
GetStatus  = load_srv_file(f"{MSGS}/GetStatus.srv",  PKG)

irap_noroslib.init_node("my_node")

# publish it
pub = irap_noroslib.Publisher("/data", CustomData)
m = CustomData(id=7, label="hi", samples=[1.0, 2.0])
m.header.frame_id = "base_link"
m.readings = [Reading(value=1.5, unit="C")]
m.where.x = 3.0
pub.publish(m)

# subscribe to it
irap_noroslib.Subscriber("/data", CustomData,
                         lambda m: print(m.id, m.label, m.where.x))
```

```cpp
#include "irap_noroslib.hpp"
using namespace irap_noroslib;

const std::string MSGS = "/home/me/robot_msgs";    // wherever you put them
const std::string PKG  = "my_robot_msgs";          // the package they came from

MsgType Reading    = load_msg_file(MSGS + "/Reading.msg",    PKG);
MsgType CustomData = load_msg_file(MSGS + "/CustomData.msg", PKG);
SrvType GetStatus  = load_srv_file(MSGS + "/GetStatus.srv",  PKG);

init_node("my_node");

// publish it -- fields by name, nest with a dot, index with brackets
DynamicPublisher pub("/data", CustomData);
DynamicMessage m = CustomData.create();
m.set("id", 7).set("label", "hi");
m.set("header.frame_id", "base_link");
m.set_array("samples", std::vector<double>{1.0, 2.0});
m.set("where.x", 3.0);
m.append("readings").msg().set("value", 1.5).set("unit", "C");
pub.publish(m);

// subscribe to it
DynamicSubscriber sub("/data", CustomData, [](const DynamicMessage& m) {
  loginfo(m.get<std::string>("label"));
  double x = m.get<double>("where.x");
  auto unit = m.get<std::string>("readings[0].unit");   // index into an array
});
```

Note `Reading.msg` is loaded too, because `CustomData` nests it — **every custom
type you use needs its own call**, one per file. Order doesn't matter, and if you
forget one the error tells you exactly which file to add:

```
unknown message type "my_robot_msgs/Reading". It is nested by a type you loaded,
so load its file too:
    load_msg_file("/path/to/Reading.msg", "my_robot_msgs")
```

Built-in types (`std_msgs/Header`, `geometry_msgs/Point`, …) need no call at all —
all 64 are already there.

#### Does the package name matter? — yes, get it right

ROS names a type `pkg/Type`, so a loose file can't tell you the `pkg` and you pass
it. (Only if the file still sits in a real catkin layout — `<pkg>/msg/<Type>.msg` —
is it inferred, and then you can omit it.)

It is **not** part of the md5sum, so a wrong package name does *not* by itself break
the connection: ROS validates the md5, and a real subscriber will happily accept
your data. That makes a typo here quietly harmful rather than loudly fatal. It
matters for two things:

- **Nested types by bare name.** `Reading[] readings` inside `my_robot_msgs/CustomData`
  means `my_robot_msgs/Reading`. Give the wrong package and it looks for
  `wrong_pkg/Reading`, which doesn't exist — the load fails outright.
- **Everything type-aware in ROS.** The name you pass is the type real ROS sees.
  Get it wrong and `rostopic echo` gives
  `ERROR: Cannot load message class for [wrong_pkg/CustomData]`, `rosbag` records
  the wrong type, and anything checking the type string is misled — even though the
  bytes on the wire are perfectly correct.

The md5sum and the wire codec are **derived from the file** by the exact ROS
algorithm, so the type is precisely what `rosmsg md5` computes and real ROS nodes
accept it. `load_action_file` works the same way and registers all 7 ROS action
types for you.

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
real ROS subscribers. (md5 *discovery* — below — rescues the **subscriber** side
only: it learns the publisher's real md5 from the rejection and reconnects. It
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

## Automatic md5 discovery

Every ROS message type has an **md5sum** — a fingerprint of its fields. Publisher
and subscriber must agree on it, or ROS refuses the connection:

```
[ERROR] Client [...] wants topic X to have datatype/md5sum [.../c3c0...],
but our version has [.../4193...]. Dropping connection.
```

NoROSLib **reads the publisher's real md5 out of that error, adopts it, and
reconnects** — so you never get stuck on an md5 you got wrong when *subscribing*.

This rescues the **subscriber** side. It cannot fix a publisher: if you *advertise*
with a wrong md5, real ROS subscribers reject you and there is no error to learn
from. (NoROSLib plays the other role too, emitting the same rejection to peers that
present a wrong md5 to it.) If you don't want to think about md5s at all, load the
`.msg` file and let NoROSLib derive it — see
[Step 9](#step-9--use-your-robots-own-message-type).

## `nr_rostopic` — `rostopic`, with no ROS installed

The same subcommands and the same arguments as the real tool, so your muscle memory
works. Available in **both languages** (`nr_rostopic` once pip-installed, or
`python3 -m irap_noroslib.rostopic`; `./cpp/build/nr_rostopic` in C++).

```bash
nr_rostopic list [-v]                                # all topics (+ types, counts)
nr_rostopic type   /chatter                          # std_msgs/String
nr_rostopic info   /chatter                          # type, publishers, subscribers
nr_rostopic find   std_msgs/String                   # topics of a type
nr_rostopic echo [-n N] [--noarr] /chatter           # decode and print
nr_rostopic hz     /chatter                          # publish rate
nr_rostopic bw     /chatter                          # bandwidth
nr_rostopic pub [-r HZ | -1] /chatter std_msgs/String "data: hi"
```

**Pointing it at a master.** `--master` takes a host, an IP, a `host:port`, or a full
URI, and `--port` sets the port — so these all mean the same thing:

```bash
nr_rostopic --master 127.0.0.1 --port 11311 echo /chatter
nr_rostopic --master 127.0.0.1:11311        echo /chatter
nr_rostopic --master http://127.0.0.1:11311 echo /chatter
nr_rostopic --master 127.0.0.1              echo /chatter   # port defaults to 11311
```

With nothing given it uses `$ROS_MASTER_URI`, else `http://localhost:11311`. The C++
`nr_rostopic` takes the same flags.

### `echo` does something real `rostopic` cannot

Point it at a **custom message type you have no `.msg` file for** — one built on the
robot, in a catkin package you don't have — and it still decodes it:

```bash
$ rostopic echo /custom            # the real tool, without the package built
ERROR: Cannot load message class for [noros_stress_msgs/CustomData]. Are your messages built?

$ nr_rostopic echo -n 1 /custom    # same shell, no .msg file, no catkin package
header:
  seq: 1
  frame_id: "robot"
id: 42
samples: [1.5, 2.5]
label: "hi"
blob: [1, 2, 3]
location:
  x: 9.5
valid: True
---
```

Because a ROS publisher hands over the **full message definition** in the TCPROS
handshake — its own text plus every nested dependency — NoROSLib rebuilds the type
on the spot and decodes the bytes. Real `rostopic echo` refuses, because it needs
the message class *built* in a catkin package first.

The same trick is available in your own code: `Subscriber(topic, None, cb)` in
Python, `AnySubscriber` in C++ — subscribe to a topic whose type you have never
seen and get a decoded message.

## `nr_roscore` — run your own ROS master

NoROSLib can also **be the roscore**, not just talk to one. `nr_roscore` is a
standalone ROS master + parameter server (no ROS installed) that real ROS nodes
(rospy/roscpp), `rostopic`/`rosservice`/`rosparam`, and NoROSLib nodes all register
with. It implements the Master API (register/unregister publisher/subscriber/
service, `lookupService`/`lookupNode`, `getSystemState`, `getPublishedTopics`, …),
sends `publisherUpdate` to subscribers, and serves parameters.

After `pip install irap_noroslib`, **`nr_roscore` is on your PATH**:

```bash
nr_roscore                                  # bind every interface, port 11311
nr_roscore --bind 127.0.0.1 --port 11311    # loopback only
nr_roscore --bind 0.0.0.0 --port 11322      # every interface, custom port
nr_roscore --host 192.168.1.10              # advertise this IP to nodes

./cpp/build/nr_roscore --port 11311         # C++ (built with the examples)
```

`--bind` is the interface it **listens on**; `--host` is what it **tells nodes to
connect back to**. Give only `--bind` with a concrete IP and it is used for both —
so `nr_roscore --bind 127.0.0.1 --port 11311` advertises `http://127.0.0.1:11311/`,
not the system hostname (a loopback-only master must not hand nodes a name that
resolves elsewhere).

Configuration (same knobs as a real roscore):

| Knob | Source (in precedence order) |
|---|---|
| **port** | `--port` › port in `$ROS_MASTER_URI` › `11311` |
| **hostname** | `--host` › a concrete `--bind` › `$ROS_HOSTNAME` › `$ROS_IP` › system hostname |

Then point any node at it: `export ROS_MASTER_URI=http://<host>:<port>`. Both
masters handle **full (nested dict) parameter trees** and auto-start a `/rosout`
→ `/rosout_agg` aggregator (disable with `--no-rosout`). Verified with real ROS:
two `rostopic pub`/`echo` nodes talk **through** nr_roscore,
`rosservice`/`rosparam` (incl. nested dicts)/`rostopic list`/`info` work against
it, and NoROSLib nodes use it as their master. Stress-tested: the 13-type topic
matrix (TCPROS+UDPROS), a 3,600-call service flood, concurrent parameter churn,
and a 5,400-op registration storm all pass against both masters.

## All examples

Every example exists in **both languages** — same name, same behaviour. Run them
with a master up ([Step 2](#step-2--get-a-ros-master-running)) and `ROS_MASTER_URI`
set ([Step 3](#step-3--point-your-program-at-the-master)).

```bash
python3 python/examples/<name>.py       # Python: just run it

cmake -S cpp -B cpp/build               # C++: build once...
cmake --build cpp/build -j
./cpp/build/<name>                      # ...then run
```

| Example | Shows | Python | C++ |
|---|---|:---:|:---:|
| **talker** | publish `std_msgs/String` at 10 Hz | ✅ | ✅ |
| **listener** | subscribe `std_msgs/String` | ✅ | ✅ |
| **custom_msg** | your own message type — from text **and** from a `.msg` file | ✅ | ✅ |
| **md5_discovery** | subscribe with a wrong md5, auto-recover from the error | ✅ | ✅ |
| **stamped_pub** / **stamped_sub** | a custom message with a `std_msgs/Header` | ✅ | ✅ |
| **add_two_ints_server** / **add_two_ints_client** | a service (srv) server + client | ✅ | ✅ |
| **fibonacci_server** / **fibonacci_client** | an action (actionlib) server + client | ✅ | ✅ |
| **params_example** | parameters: get/set/has/delete (Python also search/list) | ✅ | ✅ |
| **udp_listener** | subscribe over **UDPROS** | ✅ | ✅ |
| **nr_roscore** | run your own ROS master | ✅ | ✅ |
| **nr_rostopic** | the `rostopic` CLI | ✅ | ✅ |
| **webcam_pub** | publish `sensor_msgs/Image` + `CompressedImage` from a webcam | ✅ | ✅ |
| **webcam_sub** | subscribe to those images and display them | ✅ | ✅ |

The two webcam examples **require OpenCV** (`cv2` in Python,
`<opencv2/opencv.hpp>` in C++). **OpenCV is not a dependency of NoROSLib** — the
core library needs nothing beyond the standard library / a C++17 toolchain; only
these optional demos use it. The CMake build compiles them only if OpenCV is found.
(`python/examples/ros_image_viewer.py` is a **real rospy** node, kept as a
cross-check that the images we publish are decodable by genuine ROS.)

Cross-check any example with real ROS tools — `rostopic echo`/`pub`,
`rosservice call`, `rosparam get`/`set`, and the `roscpp_tutorials` /
`rospy_tutorials` / `actionlib_tutorials` packages.

### Full demo with NoROSLib only (no ROS installed anywhere)

```bash
nr_roscore --port 11311 &                      # 1) your own master
export ROS_MASTER_URI=http://localhost:11311   # 2) point at it
python3 python/examples/listener.py &          # 3) receive
python3 python/examples/talker.py              # 4) send
```

### Message-type coverage (verified against real ROS)

Every built-in message type — and a **custom message created on real ROS** (a
catkin `noros_stress_msgs/CustomData` package) — round-trips through a real
`roscore` in **both languages**, decoded by genuine `rospy` subscribers (so the
md5 handshake + wire bytes are checked by ROS itself):

| | types published | verified by real ROS |
|---|:---:|:---:|
| **Python** | 64 built-ins + custom | 65 / 65 ✅ |
| **C++** | 64 built-ins + custom | 65 / 65 ✅ |

The custom message's md5 that NoROSLib computes (`90f5077…`) is **identical** to the
one real ROS's `rosmsg md5` generates for the same `.msg`, and it flows both ways
(NoROSLib → `rostopic echo`, and a real `rospy` publisher → NoROSLib subscriber) in
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
LICENSE    Non-Commercial License
```

## Credits

Created by **iRAP Robot**, King Mongkut's University of Technology North Bangkok
(KMUTNB).

## License

Non-Commercial License — see [LICENSE](LICENSE).
