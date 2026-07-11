"""Offline regression tests for the noros message engine: md5sums must match
real ROS, and every type must round-trip through serialize/deserialize.

Run:  cd py && PYTHONPATH=. python3 tests/test_messages.py
"""
from noros import msg, define_message, srv
from noros.srv import define_service
from noros.message import registry

# service md5sums as reported by `rossrv md5 <type>`.
EXPECT_SRV_MD5 = {
    "std_srvs/Empty": "d41d8cd98f00b204e9800998ecf8427e",
    "std_srvs/Trigger": "937c9679a518e3a18d831e57125ea522",
    "std_srvs/SetBool": "09fb03525b03e7ea1fd3992bafd87e16",
    "rospy_tutorials/AddTwoInts": "6a2e34150c00229791cc89ff309fff21",
}

# md5sums as reported by `rosmsg md5 <type>` on ROS Noetic.
EXPECT_MD5 = {
    "std_msgs/String": "992ce8a1687cec8c8bd883ec73ca41d1",
    "std_msgs/Int32": "da5909fbe378aeaf85e547e830cc1bb7",
    "std_msgs/Int64": "34add168574510e6e17f5d23ecc077ef",
    "std_msgs/Float32": "73fcbf46b49191e672908e50842a83d4",
    "std_msgs/Float64": "fdb28210bfa9d7c91146260178d9a584",
    "std_msgs/Bool": "8b94c1b53db61fb6aed406028ad6332a",
    "std_msgs/Header": "2176decaecbce78abc3b96ef049fabed",
    "std_msgs/ColorRGBA": "a29a96539573343b1310c73607334b00",
    "std_msgs/Empty": "d41d8cd98f00b204e9800998ecf8427e",
    "geometry_msgs/Vector3": "4a842b65f413084dc2b10fb484ea7f17",
    "geometry_msgs/Point": "4a842b65f413084dc2b10fb484ea7f17",
    "geometry_msgs/Quaternion": "a779879fadf0160734f906b8c19c7004",
    "geometry_msgs/Pose": "e45d45a5a1ce597b249e23fb30fc871f",
    "geometry_msgs/Twist": "9f195f881246fdfa2798d1d3eebca84a",
    "geometry_msgs/PoseStamped": "d3812c3cbc69362b77dc0b19b345f8f5",
    "sensor_msgs/Image": "060021388200f6f0f447d0fcd9c64743",
    "sensor_msgs/PointCloud2": "1158d486dd51d683ce2f1be655c3c181",
    "sensor_msgs/PointField": "268eacb2962780ceac86cbd17e328150",
}


def test_md5():
    for t, expected in EXPECT_MD5.items():
        got = registry.get_spec(t).compute_md5()
        assert got == expected, "%s: %s != %s" % (t, got, expected)


def test_roundtrip_builtins():
    s = msg.String(data="hello world")
    assert msg.String.deserialize(s.serialize()) == s

    t = msg.Twist()
    t.linear.x, t.angular.z = 1.5, -2.0
    t2 = msg.Twist.deserialize(t.serialize())
    assert (t2.linear.x, t2.angular.z) == (1.5, -2.0)

    im = msg.Image(width=4, height=2, encoding="mono8", step=4,
                   data=bytes(range(8)))
    im.header.frame_id, im.header.stamp = "cam", (123, 456)
    im2 = msg.Image.deserialize(im.serialize())
    assert im2.data == im.data and im2.header.stamp == (123, 456)

    pc = msg.PointCloud2(width=1, height=1, point_step=12, row_step=12,
                         data=b"\x00" * 12)
    pc.fields = [msg.PointField(name="x", offset=0, datatype=7, count=1),
                 msg.PointField(name="y", offset=4, datatype=7, count=1)]
    pc2 = msg.PointCloud2.deserialize(pc.serialize())
    assert len(pc2.fields) == 2 and pc2.fields[1].name == "y"


def test_custom_and_constants():
    assert msg.PointField.FLOAT32 == 7

    Pose2D = define_message("noros_test/Pose2D",
                            "float64 x\nfloat64 y\nfloat64 theta\n")
    # same fields as geometry_msgs/Pose2D -> same md5
    assert Pose2D.md5sum() == "938fa65709584ad8e77d238529be13b8"
    p = Pose2D(x=1.0, y=2.0, theta=3.14)
    assert Pose2D.deserialize(p.serialize()) == p

    Path2D = define_message("noros_test/Path2D",
                            "std_msgs/Header header\nnoros_test/Pose2D[] poses\n")
    pa = Path2D()
    pa.poses = [Pose2D(x=1.0), Pose2D(x=2.0)]
    pa2 = Path2D.deserialize(pa.serialize())
    assert len(pa2.poses) == 2 and pa2.poses[1].x == 2.0


def test_action_md5():
    from noros import define_action
    # actionlib_msgs (registered on import of noros.msg)
    assert registry.get_spec("actionlib_msgs/GoalStatus").compute_md5() == \
        "d388f9b87b3c471f784434d671988d4a"
    assert registry.get_spec("actionlib_msgs/GoalStatusArray").compute_md5() == \
        "8b2b82f13216d0a8ea88bd3af735e619"
    # the 7 generated Fibonacci action messages
    define_action("actionlib_tutorials/Fibonacci",
                  "int32 order", "int32[] sequence", "int32[] sequence")
    exp = {
        "actionlib_tutorials/FibonacciGoal": "6889063349a00b249bd1661df429d822",
        "actionlib_tutorials/FibonacciActionGoal": "006871c7fa1d0e3d5fe2226bf17b2a94",
        "actionlib_tutorials/FibonacciActionResult": "bee73a9fe29ae25e966e105f5553dd03",
        "actionlib_tutorials/FibonacciActionFeedback": "73b8497a9f629a31c0020900e4148f07",
        "actionlib_tutorials/FibonacciAction": "f59df5767bf7634684781c92598b2406",
    }
    for t, e in exp.items():
        assert registry.get_spec(t).compute_md5() == e, t


def test_bare_header_md5():
    # A bare `Header` field (used by the canonical rosgraph_msgs/Log.msg and many
    # real .msg files) must be normalized to std_msgs/Header and replaced by its
    # md5 -- exactly like writing the fully-qualified name. Regression guard for
    # the md5_text bug where bare Header was hashed literally as "Header header".
    Log = define_message(
        "rosgraph_msgs/Log",
        "byte DEBUG=1\nbyte INFO=2\nbyte WARN=4\nbyte ERROR=8\nbyte FATAL=16\n"
        "Header header\nbyte level\nstring name\nstring msg\nstring file\n"
        "string function\nuint32 line\nstring[] topics\n")
    assert Log.md5sum() == "acffd30cd6b6de30f120938c17c593fb", Log.md5sum()

    # bare `Header` must produce the SAME md5 as fully-qualified `std_msgs/Header`.
    bare = define_message("noros_test/StampedBare",
                          "Header header\ngeometry_msgs/Pose pose\n")
    full = define_message("noros_test/StampedFull",
                          "std_msgs/Header header\ngeometry_msgs/Pose pose\n")
    assert bare.md5sum() == full.md5sum(), (bare.md5sum(), full.md5sum())
    # same fields as geometry_msgs/PoseStamped -> same md5 as `rosmsg md5`.
    assert bare.md5sum() == "d3812c3cbc69362b77dc0b19b345f8f5", bare.md5sum()

    # a bare-Header message still round-trips through serialize/deserialize.
    m = bare()
    m.header.frame_id = "map"
    m.header.stamp = (7, 8)
    m.pose.orientation.w = 1.0
    m2 = bare.deserialize(m.serialize())
    assert m2.header.frame_id == "map" and m2.header.stamp == (7, 8)
    assert m2.pose.orientation.w == 1.0


def test_service_md5_and_roundtrip():
    AddTwoInts = define_service("rospy_tutorials/AddTwoInts",
                                "int64 a\nint64 b", "int64 sum")
    got = {
        "std_srvs/Empty": srv.Empty.md5sum(),
        "std_srvs/Trigger": srv.Trigger.md5sum(),
        "std_srvs/SetBool": srv.SetBool.md5sum(),
        "rospy_tutorials/AddTwoInts": AddTwoInts.md5sum(),
    }
    for t, expected in EXPECT_SRV_MD5.items():
        assert got[t] == expected, "%s: %s != %s" % (t, got[t], expected)

    req = AddTwoInts.Request(a=3, b=4)
    assert AddTwoInts.Request.deserialize(req.serialize()) == req
    resp = AddTwoInts.Response(sum=7)
    assert AddTwoInts.Response.deserialize(resp.serialize()).sum == 7


if __name__ == "__main__":
    test_md5()
    test_roundtrip_builtins()
    test_custom_and_constants()
    test_bare_header_md5()
    test_service_md5_and_roundtrip()
    test_action_md5()
    print("all message + service + action tests passed")
