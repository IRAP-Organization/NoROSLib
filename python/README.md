# noros (Python)

A rospy-flavoured ROS client library — **no ROS installed, no bridge, pure Python
stdlib.** It speaks the real ROS wire protocols (XML-RPC master/slave +
TCPROS/UDPROS) *directly*, so it connects to a **native, unmodified `roscore`** and
real ROS nodes treat it as a legitimate node — **not** rosbridge/roslibpy, and
nothing extra runs on the robot.

## Install

```bash
pip install noros          # from PyPI (once published)
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
import noros
from noros import msg

noros.init_node("talker")
pub = noros.Publisher("/chatter", msg.String)
rate = noros.Rate(10)
while not noros.is_shutdown():
    pub.publish(msg.String(data="hello world"))
    rate.sleep()
```

**Subscriber** (`listener.py`):

```python
import noros
from noros import msg

noros.init_node("listener")
noros.Subscriber("/chatter", msg.String, lambda m: noros.loginfo("I heard: " + m.data))
noros.spin()
```

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

The examples just `import noros`, so **install it first** (`pip install noros`, or
`pip install ./python` from this repo). Then run any with
`python3 python/examples/<name>.py` (a master up, `ROS_MASTER_URI` set).
Every example calls `noros.set_master_uri(...)` and `noros.set_hostname(...)`
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
noros.set_master_uri("http://192.168.10.5:11311")   # overrides $ROS_MASTER_URI
noros.set_hostname("192.168.10.2")                    # overrides $ROS_IP / $ROS_HOSTNAME
noros.init_node("my_node")
# or all at once:  noros.init_node("my_node", master_uri=..., host=...)
```

## Messages

Built-ins live in `noros.msg`. Every md5sum matches `rosmsg md5` exactly, so they
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

Get a message class two ways — attribute or full name:

```python
from noros import msg

s   = msg.String                     # by attribute
odo = msg.get("nav_msgs/Odometry")   # by full "pkg/Type" name
```

Construct with keyword fields, or set them afterwards. Nested messages, arrays,
`Header`, and `time`/`duration` all just work:

```python
from noros import msg

# simple scalar wrappers
msg.Int32(data=7)
msg.Float64(data=1.5)
msg.String(data="hello")

# nested messages + arrays
t = msg.Twist()
t.linear  = msg.Vector3(x=1.0, y=0.0, z=0.0)   # nested
t.angular.z = 0.5                               # or reach in directly

# a Header-stamped message (time is a (secs, nsecs) tuple)
import noros
o = msg.Odometry()
o.header.seq = 0
o.header.stamp = noros.now()                    # (secs, nsecs)
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
import noros
from noros import msg

pub = noros.Publisher("/odom", msg.Odometry)          # advertise the type
pub.publish(o)

noros.Subscriber("/odom", msg.Odometry,
                 lambda m: noros.loginfo(m.child_frame_id))
```

### Your own message types

Not in the catalog? Define it in one line from `.msg` text — noros derives the
md5sum and wire codec. Nesting built-ins (or your own registered types) works;
a `std_msgs/Header` first field is handled for you:

```python
from noros import define_message

Pose2D = define_message("my_pkg/Pose2D", "float64 x\nfloat64 y\nfloat64 theta")
p = Pose2D(x=1.0, y=2.0)

Reading = define_message("my_pkg/Reading", """
    std_msgs/Header header
    float64 value
    geometry_msgs/Point where
""")
```

See `examples/custom_msg.py` and `examples/stamped_pub.py` / `stamped_sub.py`.

## Services

```python
from noros import define_service
AddTwoInts = define_service("rospy_tutorials/AddTwoInts", "int64 a\nint64 b", "int64 sum")

noros.Service("/add_two_ints", AddTwoInts,
              lambda req: AddTwoInts.Response(sum=req.a + req.b))          # server

noros.wait_for_service("/add_two_ints")
add = noros.ServiceProxy("/add_two_ints", AddTwoInts)                     # client
print(add(AddTwoInts.Request(a=3, b=4)).sum)                              # -> 7
```

Built-ins in `noros.srv`: `Empty`, `Trigger`, `SetBool`.

## Actions (actionlib)

```python
from noros import define_action, SimpleActionClient, SimpleActionServer

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
noros.set_param("/demo/rate", 30)          # int/float/str/bool/list/dict
noros.get_param("/demo/rate")              # -> 30
noros.get_param("/demo/missing", default=5)
noros.has_param(...); noros.delete_param(...); noros.search_param(...); noros.get_param_names()
```

Round-trips with `rosparam get/set/list`.

## UDPROS (unreliable transport)

```python
noros.Subscriber("/chatter", msg.String, cb, transport="udpros")
```

Publishers offer UDPROS automatically. See `examples/udp_listener.py`.

## Run your own ROS master — `nr_roscore`

noros can also *be* the roscore. `nr_roscore` is a standalone ROS master +
parameter server (pure stdlib) that real ROS nodes and noros nodes register with.

```bash
python3 examples/nr_roscore.py                 # binds :11311, advertises this host
python3 examples/nr_roscore.py --port 11322
ROS_MASTER_URI=http://myhost:11311 ROS_HOSTNAME=myhost python3 examples/nr_roscore.py
```

Config precedence — **port:** `--port` › `$ROS_MASTER_URI` › `11311`;
**hostname:** `--host` › `$ROS_HOSTNAME` › `$ROS_IP` › system hostname. Or embed
it:

```python
from noros.roscore import serve
serve(host="192.168.1.10", port=11311)      # blocks; Ctrl-C to stop
```

Implements the Master API + full (dict) Parameter Server, and auto-starts a
`/rosout` → `/rosout_agg` aggregator (disable with `--no-rosout`; run it
standalone with `python3 -m noros.rosout`). Verified: two real `rostopic
pub`/`echo` nodes talk through it; `rosservice`/`rosparam`/`rostopic` work
against it; stress-tested with the topic matrix, service flood, and a
registration storm.

## Automatic md5 discovery

Subscribe with `data_class=None` (or a wrong md5) and noros learns the
publisher's real type/md5 from the handshake and reconnects — see
`examples/md5_discovery.py`.

## Tests

```bash
cd python && PYTHONPATH=. python3 tests/test_messages.py
```
