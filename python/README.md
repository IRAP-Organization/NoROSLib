# NoROSLib (Python) — `import irap_noroslib`

A rospy-flavoured ROS client library — **no ROS installed, no bridge, pure Python
stdlib.** It speaks the real ROS wire protocols (XML-RPC master/slave +
TCPROS/UDPROS) *directly*, so it connects to a **native, unmodified `roscore`** and
real ROS nodes treat it as a legitimate node — **not** rosbridge/roslibpy, and
nothing extra runs on the robot.

## Install

```bash
pip install irap_noroslib          # from PyPI (once published)
# or from a checkout:
pip install ./python       # run from the repo root
# or editable, for hacking on it:
pip install -e ./python
```

**No dependencies — just Python 3.6+.** The ROS communication library is pure
standard library; `requirements.txt` is intentionally empty and documents that
you install nothing to talk to ROS. (The `webcam_pub.py` / `ros_image_viewer.py`
demos are the only examples that want extra packages — OpenCV, and for the
viewer a real ROS `rospy`/`cv_bridge` — and are not needed for anything else.)

## Hello, topics

**Publisher** (`talker.py`):

```python
import irap_noroslib
from irap_noroslib.std_msgs.msg import String

irap_noroslib.init_node("talker")
pub = irap_noroslib.Publisher("/chatter", String)
rate = irap_noroslib.Rate(10)
while not irap_noroslib.is_shutdown():
    pub.publish(String(data="hello world"))
    rate.sleep()
```

**Subscriber** (`listener.py`):

```python
import irap_noroslib
from irap_noroslib.std_msgs.msg import String

irap_noroslib.init_node("listener")
irap_noroslib.Subscriber("/chatter", String, lambda m: irap_noroslib.loginfo("I heard: " + m.data))
irap_noroslib.spin()
```

That's a rospy node with `rospy` swapped for `irap_noroslib` — the message import is
the path you already know (`from std_msgs.msg import String`), just prefixed.

Point them at a master (defaults to `http://127.0.0.1:11311`):

```bash
export ROS_MASTER_URI=http://localhost:11311
export ROS_HOSTNAME=localhost
python3 examples/talker.py
python3 examples/listener.py
```

Cross-check with real ROS: `rostopic echo /chatter`, or
`rostopic pub -r 10 /chatter std_msgs/String "data: hi"`.

## All examples (`python/examples/`)

The examples just `import irap_noroslib`, so **install it first** (`pip install irap_noroslib`, or
`pip install ./python` from this repo). Then run any with
`python3 python/examples/<name>.py` (a master up, `ROS_MASTER_URI` set).
Every example calls `irap_noroslib.set_master_uri(...)` and `irap_noroslib.set_hostname(...)`
**before** `init_node` (falling back to `$ROS_MASTER_URI` / `$ROS_HOSTNAME`, else
a local roscore). The core examples mirror the C++ ones one-for-one.

| File | What it does |
|---|---|
| `talker.py` | publish `std_msgs/String` at 10 Hz |
| `listener.py` | subscribe `std_msgs/String` |
| `custom_msg.py` | define + publish your own message type |
| `md5_discovery.py` | subscribe with a wrong md5, recover automatically |
| `stamped_pub.py` / `stamped_sub.py` | a custom message with a `std_msgs/Header` |
| `add_two_ints_server.py` / `add_two_ints_client.py` | a service (srv) server + client |
| `fibonacci_server.py` / `fibonacci_client.py` | an action (actionlib) server + client |
| `params_example.py` | parameters get/set/has/delete/search/list |
| `udp_listener.py` | subscribe over UDPROS |
| `nr_roscore.py` | run your own ROS master (roscore) |
| `webcam_pub.py` | publish `sensor_msgs/Image` + `CompressedImage` from `/dev/video0` |
| `ros_image_viewer.py` | a real rospy node that `cv2.imshow`s the webcam feed |

## Configuration (from code, no env vars)

```python
irap_noroslib.set_master_uri("http://192.168.10.5:11311")   # overrides $ROS_MASTER_URI
irap_noroslib.set_hostname("192.168.10.2")                    # overrides $ROS_IP / $ROS_HOSTNAME
irap_noroslib.init_node("my_node")
# or all at once:  irap_noroslib.init_node("my_node", master_uri=..., host=...)
```

## Messages

Built-ins live in `irap_noroslib.msg`. Every md5sum matches `rosmsg md5` exactly, so they
interoperate with real ROS nodes. This is the **same 64-type catalog as the C++
library** — the two are in lock-step.

### The full catalog (64 types)

| Package | Types |
|---|---|
| **std_msgs** (19) | `Bool`, `Byte`, `Char`, `ColorRGBA`, `Duration`, `Empty`, `Float32`, `Float64`, `Header`, `Int8`, `Int16`, `Int32`, `Int64`, `String`, `Time`, `UInt8`, `UInt16`, `UInt32`, `UInt64` |
| **geometry_msgs** (16) | `Accel`, `Point`, `Point32`, `Polygon`, `Pose`, `PoseArray`, `PoseStamped`, `PoseWithCovariance`, `Quaternion`, `Transform`, `TransformStamped`, `Twist`, `TwistStamped`, `TwistWithCovariance`, `Vector3`, `Wrench` |
| **sensor_msgs** (14) | `CameraInfo`, `CompressedImage`, `Image`, `Imu`, `JointState`, `LaserScan`, `MagneticField`, `NavSatFix`, `NavSatStatus`, `PointCloud2`, `PointField`, `Range`, `RegionOfInterest`, `Temperature` |
| **nav_msgs** (5) | `GridCells`, `MapMetaData`, `OccupancyGrid`, `Odometry`, `Path` |
| **diagnostic_msgs** (3) | `DiagnosticArray`, `DiagnosticStatus`, `KeyValue` |
| **trajectory_msgs** (4) | `JointTrajectory`, `JointTrajectoryPoint`, `MultiDOFJointTrajectory`, `MultiDOFJointTrajectoryPoint` |
| **actionlib_msgs** (3) | `GoalID`, `GoalStatus`, `GoalStatusArray` |

### How to use them

**ROS-style imports.** Every type is importable on the exact path rospy uses,
just prefixed with `irap_noroslib` — so porting a rospy node is a one-word edit:

```python
# rospy                                 # irap_noroslib
from std_msgs.msg import String         from irap_noroslib.std_msgs.msg import String
from geometry_msgs.msg import Twist     from irap_noroslib.geometry_msgs.msg import Twist
from sensor_msgs.msg import Image       from irap_noroslib.sensor_msgs.msg import Image
from std_srvs.srv import Trigger        from irap_noroslib.std_srvs.srv import Trigger
```

All seven message packages (`std_msgs`, `geometry_msgs`, `sensor_msgs`,
`nav_msgs`, `diagnostic_msgs`, `trajectory_msgs`, `actionlib_msgs`) and
`std_srvs` work this way. These are the *same* class objects as the ones below —
re-exported, not copies — so the md5sums and the registry stay identical.

Or get a class from the flat module — by attribute, or by full name:

```python
from irap_noroslib import msg

s   = msg.String                     # by attribute
odo = msg.get("nav_msgs/Odometry")   # by full "pkg/Type" name
```

Construct with keyword fields, or set them afterwards. Nested messages, arrays,
`Header`, and `time`/`duration` all just work:

```python
from irap_noroslib import msg

# simple scalar wrappers
msg.Int32(data=7)
msg.Float64(data=1.5)
msg.String(data="hello")

# nested messages + arrays
t = msg.Twist()
t.linear  = msg.Vector3(x=1.0, y=0.0, z=0.0)   # nested
t.angular.z = 0.5                               # or reach in directly

# a Header-stamped message (time is a (secs, nsecs) tuple)
import irap_noroslib
o = msg.Odometry()
o.header.seq = 0
o.header.stamp = irap_noroslib.now()                    # (secs, nsecs)
o.header.frame_id = "odom"
o.child_frame_id = "base_link"
o.pose.pose.position.x = 1.5                     # nested-in-nested
o.twist.twist.linear.x = 0.3

# variable-length arrays are plain Python lists; uint8[] is bytes
js = msg.JointState()
js.name     = ["j1", "j2"]
js.position = [0.1, 0.2]
img = msg.Image()
img.data = b"\x00\x01\x02"                       # uint8[] -> bytes
```

Publish / subscribe with any of them:

```python
import irap_noroslib
from irap_noroslib import msg

pub = irap_noroslib.Publisher("/odom", msg.Odometry)          # advertise the type
pub.publish(o)

irap_noroslib.Subscriber("/odom", msg.Odometry,
                 lambda m: irap_noroslib.loginfo(m.child_frame_id))
```

### Your own message types

Not in the catalog? Define it in one line from `.msg` text — irap_noroslib derives the
md5sum and wire codec. Nesting built-ins (or your own registered types) works;
a `std_msgs/Header` first field is handled for you.

*(If you already **have** the `.msg` file, skip this and just
[load it](#loading-a-msg--srv--action-file) — same result, no retyping.)*

```python
from irap_noroslib import define_message

Pose2D = define_message("my_pkg/Pose2D", "float64 x\nfloat64 y\nfloat64 theta")
p = Pose2D(x=1.0, y=2.0)

Reading = define_message("my_pkg/Reading", """
    std_msgs/Header header
    float64 value
    geometry_msgs/Point where
""")
```

See `examples/custom_msg.py` and `examples/stamped_pub.py` / `stamped_sub.py`.

### Loading a `.msg` / `.srv` / `.action` **file**

Already have the files? Copy them off the robot and give irap_noroslib the **full
path of each one** — no catkin package, no ROS install, nothing else.

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

```python
import irap_noroslib
from irap_noroslib import load_msg_file, load_srv_file

MSGS = "/home/me/robot_msgs"          # wherever you put them
PKG  = "my_robot_msgs"                # the package they came from

Reading    = load_msg_file(f"{MSGS}/Reading.msg",    PKG)
CustomData = load_msg_file(f"{MSGS}/CustomData.msg", PKG)
GetStatus  = load_srv_file(f"{MSGS}/GetStatus.srv",  PKG)

irap_noroslib.init_node("my_node")

pub = irap_noroslib.Publisher("/data", CustomData)
m = CustomData(id=7, label="hi", samples=[1.0, 2.0])
m.header.frame_id = "base_link"
m.readings = [Reading(value=1.5, unit="C")]
m.where.x = 3.0
pub.publish(m)

irap_noroslib.Subscriber("/data", CustomData,
                         lambda m: print(m.id, m.label, m.where.x))
```

`Reading.msg` is loaded too, because `CustomData` nests it — **every custom type
you use needs its own call**. Order doesn't matter, and if you forget one the error
names the file to add:

```
unknown message type "my_robot_msgs/Reading". It is nested by a type you loaded,
so load its file too:
    load_msg_file("/path/to/Reading.msg", "my_robot_msgs")
```

Built-in types (`std_msgs/Header`, `geometry_msgs/Point`, …) need no call at all —
all 64 are already there.

The md5sum and the wire codec are derived from the file, so the type is exactly
what real ROS computes — `rosmsg md5` agrees, and real ROS nodes accept it.

The **package name** (`my_robot_msgs`) is the first half of the ROS type name
`my_robot_msgs/CustomData`. ROS identifies types by that full name, so pass the
package the message came from. If the file still sits in a catkin layout
(`<pkg>/msg/<Type>.msg`) it's inferred and you can omit it.

Services and actions work the same way — `load_action_file` registers all 7 ROS
action message types for you:

```python
from irap_noroslib import load_srv_file, load_action_file

Srv = load_srv_file("/home/me/msgs/StressSrv.srv",     "my_robot_msgs")   # req '---' resp
Act = load_action_file("/home/me/msgs/StressAct.action", "my_robot_msgs") # goal/result/feedback
```

| Function | Loads |
|---|---|
| `load_msg_file(path, pkg)` | one `.msg` → a message class |
| `load_msg_files([paths], pkg)` | several `.msg` files at once |
| `load_srv_file(path, pkg)` | one `.srv` (split on `---`) → a service class |
| `load_action_file(path, pkg)` | one `.action` → an action spec + all 7 action types |
| `loaded_files()` | every type loaded from a file → the path it came from |

Verified against real ROS: a custom package built with `catkin_make`, then loaded
from its bare files — all 11 md5s (3 messages, 1 service, 7 action types) match
`rosmsg md5` / `rossrv md5` exactly, and the types round-trip both ways with
genuine rospy nodes over topics, a service and an action.

## Services

```python
from irap_noroslib import define_service
AddTwoInts = define_service("rospy_tutorials/AddTwoInts", "int64 a\nint64 b", "int64 sum")

irap_noroslib.Service("/add_two_ints", AddTwoInts,
              lambda req: AddTwoInts.Response(sum=req.a + req.b))          # server

irap_noroslib.wait_for_service("/add_two_ints")
add = irap_noroslib.ServiceProxy("/add_two_ints", AddTwoInts)                     # client
print(add(AddTwoInts.Request(a=3, b=4)).sum)                              # -> 7
```

Built-ins in `irap_noroslib.srv`: `Empty`, `Trigger`, `SetBool`.

## Actions (actionlib)

```python
from irap_noroslib import define_action, SimpleActionClient, SimpleActionServer

Fibonacci = define_action("actionlib_tutorials/Fibonacci",
                          "int32 order", "int32[] sequence", "int32[] sequence")

c = SimpleActionClient("/fibonacci", Fibonacci)
c.wait_for_server()
c.send_goal(Fibonacci.Goal(order=10), feedback_cb=lambda fb: ...)
c.wait_for_result(); c.get_result().sequence
```

See `examples/fibonacci_client.py` / `fibonacci_server.py`.

## Parameters

```python
irap_noroslib.set_param("/demo/rate", 30)          # int/float/str/bool/list/dict
irap_noroslib.get_param("/demo/rate")              # -> 30
irap_noroslib.get_param("/demo/missing", default=5)
irap_noroslib.has_param(...); irap_noroslib.delete_param(...); irap_noroslib.search_param(...); irap_noroslib.get_param_names()
```

Round-trips with `rosparam get/set/list`.

## UDPROS (unreliable transport)

```python
irap_noroslib.Subscriber("/chatter", msg.String, cb, transport="udpros")
```

Publishers offer UDPROS automatically. See `examples/udp_listener.py`.

## Run your own ROS master — `nr_roscore`

irap_noroslib can also *be* the roscore. `nr_roscore` is a standalone ROS master +
parameter server (pure stdlib) that real ROS nodes and irap_noroslib nodes register with.

```bash
python3 examples/nr_roscore.py                 # binds :11311, advertises this host
python3 examples/nr_roscore.py --port 11322
ROS_MASTER_URI=http://myhost:11311 ROS_HOSTNAME=myhost python3 examples/nr_roscore.py
```

Config precedence — **port:** `--port` › `$ROS_MASTER_URI` › `11311`;
**hostname:** `--host` › `$ROS_HOSTNAME` › `$ROS_IP` › system hostname. Or embed
it:

```python
from irap_noroslib.roscore import serve
serve(host="192.168.1.10", port=11311)      # blocks; Ctrl-C to stop
```

Implements the Master API + full (dict) Parameter Server, and auto-starts a
`/rosout` → `/rosout_agg` aggregator (disable with `--no-rosout`; run it
standalone with `python3 -m irap_noroslib.rosout`). Verified: two real `rostopic
pub`/`echo` nodes talk through it; `rosservice`/`rosparam`/`rostopic` work
against it; stress-tested with the topic matrix, service flood, and a
registration storm.

## Automatic md5 discovery

Subscribe with `data_class=None` (or a wrong md5) and irap_noroslib learns the
publisher's real type/md5 from the handshake and reconnects — see
`examples/md5_discovery.py`.

## Tests

```bash
cd python && PYTHONPATH=. python3 tests/test_messages.py
```
