# noros message stress test (against real ROS)

Proves that **every built-in message type** and a **custom message created on
real ROS** round-trip through a real `roscore`, in **both Python and C++**,
decoded by genuine `rospy` subscribers — so the md5 handshake and the wire bytes
are checked by ROS itself, not by noros.

## What it checks

- **All 64 built-in catalog types** (std_msgs, geometry_msgs, sensor_msgs,
  nav_msgs, diagnostic_msgs, trajectory_msgs, actionlib_msgs) — Python and C++
  catalogs are identical.
- **A custom message** — `noros_stress_msgs/CustomData` (Header + nested
  `geometry_msgs/Point` + arrays + scalars) built as a **real catkin package**
  (`msgs/noros_stress_msgs`). noros computes the *same* md5 real ROS does
  (`90f507711f5fc7a674b7527eafdf210d`), and it flows **both ways** (noros ⇄ real
  ROS) in both languages.

## Requirements

A real ROS install (tested on Noetic) + a running `roscore`; the noros Python
package (`../python`) and the C++ single header (`../cpp`).

## Run

```bash
export ROS_MASTER_URI=http://localhost:11311 ROS_HOSTNAME=localhost
unset ROS_IP

# 1) build + source the custom message package (creates it "on real ROS")
mkdir -p /tmp/noros_cmws/src && cp -r msgs/noros_stress_msgs /tmp/noros_cmws/src/
( cd /tmp/noros_cmws && source /opt/ros/noetic/setup.bash && catkin_make )
source /tmp/noros_cmws/devel/setup.bash
rosmsg md5 noros_stress_msgs/CustomData        # == 90f507711f5fc7a674b7527eafdf210d

# 2) PYTHON: publish the whole catalog + custom, verify with a real rospy node
python3 pub_all_py.py &
python3 verify_ros.py /stress/py 10            # expect 65/65 PASS
kill %1

# 3) C++ (single header): build the impl once, then the publisher
g++ -std=c++17 -c -DNOROS_IMPLEMENTATION -I../cpp/include ../cpp/noros_impl.cpp -o /tmp/noros_impl.o
g++ -std=c++17 -I../cpp/include pub_all_cpp.cpp /tmp/noros_impl.o -pthread -o /tmp/pub_all_cpp
/tmp/pub_all_cpp &
python3 verify_ros.py /stress/cpp 12           # expect 65/65 PASS
kill %1

# 4) reverse direction for the custom message (real ROS -> noros)
python3 rospy_pub_custom.py &
python3 noros_sub_custom.py                     # noros (Python) decodes it -> PASS
g++ -std=c++17 -I../cpp/include sub_custom_cpp.cpp /tmp/noros_impl.o -pthread -o /tmp/sub_cpp
/tmp/sub_cpp                                     # noros (C++) decodes it -> PASS
kill %1
```

## Result (verified)

| Language | types | verified by real ROS |
|---|:---:|:---:|
| Python | 64 built-ins + custom | **65 / 65 ✅** |
| C++ | 64 built-ins + custom | **65 / 65 ✅** |

Custom message md5 (noros) == `rosmsg md5` == `90f507711f5fc7a674b7527eafdf210d`,
and it round-trips noros ⇄ real ROS in both languages.
