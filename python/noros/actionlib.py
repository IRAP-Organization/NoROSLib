"""Minimal actionlib for noros: define_action + SimpleActionClient +
SimpleActionServer, built on the five standard action topics
(goal/cancel/status/feedback/result) and actionlib_msgs. Interoperates with real
ROS action servers/clients (verified vs actionlib_tutorials Fibonacci).
"""
import threading
import time

from . import msg as _msg
from .message import define_message
from .node import (Publisher, Subscriber, Rate, get_node, is_shutdown, now,
                   loginfo, logwarn)

# actionlib_msgs/GoalStatus values
PENDING = 0
ACTIVE = 1
PREEMPTED = 2
SUCCEEDED = 3
ABORTED = 4
REJECTED = 5
PREEMPTING = 6
RECALLING = 7
RECALLED = 8
LOST = 9

_TERMINAL = {PREEMPTED, SUCCEEDED, ABORTED, REJECTED, RECALLED, LOST}


def define_action(action_type, goal_text, result_text, feedback_text):
    """Register the 7 auto-generated action message types from the .action parts
    and return a spec exposing Goal/Result/Feedback/ActionGoal/ActionResult/
    ActionFeedback classes. md5sums match `rosmsg md5` exactly.

        Fibonacci = define_action("actionlib_tutorials/Fibonacci",
                                  "int32 order", "int32[] sequence", "int32[] sequence")
    """
    Goal = define_message(action_type + "Goal", goal_text or "\n")
    Result = define_message(action_type + "Result", result_text or "\n")
    Feedback = define_message(action_type + "Feedback", feedback_text or "\n")
    ActionGoal = define_message(action_type + "ActionGoal",
        "std_msgs/Header header\nactionlib_msgs/GoalID goal_id\n%sGoal goal\n" % action_type)
    ActionResult = define_message(action_type + "ActionResult",
        "std_msgs/Header header\nactionlib_msgs/GoalStatus status\n%sResult result\n" % action_type)
    ActionFeedback = define_message(action_type + "ActionFeedback",
        "std_msgs/Header header\nactionlib_msgs/GoalStatus status\n%sFeedback feedback\n" % action_type)
    define_message(action_type + "Action",
        "%sActionGoal action_goal\n%sActionResult action_result\n"
        "%sActionFeedback action_feedback\n" % (action_type, action_type, action_type))

    class ActionSpec(object):
        type = action_type
    ActionSpec.Goal = Goal
    ActionSpec.Result = Result
    ActionSpec.Feedback = Feedback
    ActionSpec.ActionGoal = ActionGoal
    ActionSpec.ActionResult = ActionResult
    ActionSpec.ActionFeedback = ActionFeedback
    ActionSpec.__name__ = str(action_type.split("/")[-1])
    return ActionSpec


_goal_seq = [0]
_goal_lock = threading.Lock()


def _new_goal_id():
    with _goal_lock:
        _goal_seq[0] += 1
        n = _goal_seq[0]
    stamp = now()
    gid = _msg.GoalID(stamp=stamp,
                      id="%s-%d-%d.%d" % (get_node().name, n, stamp[0], stamp[1]))
    return gid


class SimpleActionClient(object):
    """Send a goal to an action server and get feedback + result.

        client = SimpleActionClient("/fibonacci", Fibonacci)
        client.wait_for_server()
        client.send_goal(Fibonacci.Goal(order=10), feedback_cb=lambda fb: ...)
        client.wait_for_result()
        client.get_result()          # Fibonacci.Result
    """

    def __init__(self, ns, action_spec):
        self.ns = ns if ns.startswith("/") else "/" + ns
        self.spec = action_spec
        # Initialize all state BEFORE creating Subscribers: each Subscriber spawns
        # a background thread that may invoke a callback (e.g. _on_status) at once,
        # which would otherwise touch self._lock before it exists.
        self._lock = threading.Lock()
        self._goal_id = None
        self._result = None
        self._state = None
        self._feedback_cb = None
        self._done_cb = None
        self._done = threading.Event()
        self._seen_status = False
        self._goal_pub = Publisher(self.ns + "/goal", action_spec.ActionGoal)
        self._cancel_pub = Publisher(self.ns + "/cancel", _msg.GoalID)
        Subscriber(self.ns + "/status", _msg.GoalStatusArray, self._on_status)
        Subscriber(self.ns + "/feedback", action_spec.ActionFeedback, self._on_feedback)
        Subscriber(self.ns + "/result", action_spec.ActionResult, self._on_result)

    def wait_for_server(self, timeout=None):
        """Block until the server is connected on goal + status. Returns bool."""
        deadline = None if timeout is None else time.time() + timeout
        while not is_shutdown():
            if self._goal_pub.get_num_connections() > 0 and self._seen_status:
                return True
            if deadline is not None and time.time() >= deadline:
                return False
            time.sleep(0.02)
        return False

    def send_goal(self, goal, feedback_cb=None, done_cb=None):
        gid = _new_goal_id()
        ag = self.spec.ActionGoal()
        ag.header.stamp = now()
        ag.goal_id = gid
        ag.goal = goal
        with self._lock:
            self._goal_id = gid.id
            self._result = None
            self._state = PENDING
            self._feedback_cb = feedback_cb
            self._done_cb = done_cb
            self._done.clear()
        # Resend until the server acks (status shows our id) or a result arrives,
        # covering the pub/sub connection race that would otherwise drop the goal.
        threading.Thread(target=self._deliver_goal, args=(ag,), daemon=True).start()

    def _deliver_goal(self, ag):
        for _ in range(50):
            self._goal_pub.publish(ag)
            time.sleep(0.1)
            with self._lock:
                if self._done.is_set() or (self._state is not None and self._state != PENDING):
                    return

    def cancel_goal(self):
        with self._lock:
            gid = self._goal_id
        if gid is not None:
            self._cancel_pub.publish(_msg.GoalID(stamp=now(), id=gid))

    def wait_for_result(self, timeout=None):
        return self._done.wait(timeout)

    def get_result(self):
        with self._lock:
            return self._result

    def get_state(self):
        with self._lock:
            return self._state

    # -- topic callbacks ---------------------------------------------------
    def _on_status(self, m):
        self._seen_status = True
        with self._lock:
            gid = self._goal_id
        for st in m.status_list:
            if st.goal_id.id == gid:
                with self._lock:
                    self._state = st.status

    def _on_feedback(self, m):
        with self._lock:
            if m.status.goal_id.id != self._goal_id:
                return
            cb = self._feedback_cb
        if cb:
            cb(m.feedback)

    def _on_result(self, m):
        with self._lock:
            if m.status.goal_id.id != self._goal_id:
                return
            self._result = m.result
            self._state = m.status.status
            self._done.set()
            done_cb = self._done_cb
        if done_cb:
            done_cb(self._state, m.result)


class SimpleActionServer(object):
    """Serve one action. `execute_cb(goal, server)` runs per goal in its own
    thread; call server.publish_feedback(fb) and server.set_succeeded(result)
    (or set_aborted / set_preempted). is_preempt_requested() reports cancels.
    """

    def __init__(self, ns, action_spec, execute_cb):
        self.ns = ns if ns.startswith("/") else "/" + ns
        self.spec = action_spec
        self.execute_cb = execute_cb
        # State before Subscribers (their callback threads touch self._lock).
        self._lock = threading.Lock()
        self._cur_id = None
        self._cur_status = None
        self._preempt = False
        self._statuses = {}           # goal_id -> status (retain terminal briefly)
        self._status_pub = Publisher(self.ns + "/status", _msg.GoalStatusArray)
        self._result_pub = Publisher(self.ns + "/result", action_spec.ActionResult)
        self._feedback_pub = Publisher(self.ns + "/feedback", action_spec.ActionFeedback)
        Subscriber(self.ns + "/goal", action_spec.ActionGoal, self._on_goal)
        Subscriber(self.ns + "/cancel", _msg.GoalID, self._on_cancel)
        threading.Thread(target=self._status_loop, name="noros-as-status",
                         daemon=True).start()

    # -- status publishing -------------------------------------------------
    def _status_loop(self):
        rate = Rate(10)
        while not is_shutdown():
            self._publish_status()
            rate.sleep()

    def _publish_status(self):
        arr = _msg.GoalStatusArray()
        arr.header.stamp = now()
        with self._lock:
            items = list(self._statuses.items())
        for gid, status in items:
            st = _msg.GoalStatus()
            st.goal_id.id = gid
            st.status = status
            arr.status_list.append(st)
        self._status_pub.publish(arr)

    # -- goal / cancel handling -------------------------------------------
    def _on_goal(self, ag):
        gid = ag.goal_id.id
        with self._lock:
            if gid in self._statuses:      # duplicate (goal resend) -> ignore
                return
            self._cur_id = gid
            self._cur_status = ACTIVE
            self._preempt = False
            self._statuses[gid] = ACTIVE
        loginfo("action %s: new goal %s" % (self.ns, gid))
        threading.Thread(target=self._run, args=(ag,), daemon=True).start()

    def _on_cancel(self, gid_msg):
        with self._lock:
            if gid_msg.id == self._cur_id or gid_msg.id == "":
                self._preempt = True

    def _run(self, ag):
        gid = ag.goal_id.id
        try:
            self.execute_cb(ag.goal, self)
        except Exception as e:                       # noqa
            logwarn("action %s handler error: %s" % (self.ns, e))
            self.set_aborted(self.spec.Result())
        with self._lock:
            if self._cur_status == ACTIVE:           # handler forgot to finish
                self._finish(gid, ABORTED, self.spec.Result())

    # -- API used inside execute_cb ---------------------------------------
    def is_preempt_requested(self):
        with self._lock:
            return self._preempt

    def publish_feedback(self, feedback):
        fb = self.spec.ActionFeedback()
        fb.header.stamp = now()
        with self._lock:
            fb.status.goal_id.id = self._cur_id or ""
            fb.status.status = ACTIVE
        fb.feedback = feedback
        self._feedback_pub.publish(fb)

    def set_succeeded(self, result=None):
        self._terminal(SUCCEEDED, result)

    def set_aborted(self, result=None):
        self._terminal(ABORTED, result)

    def set_preempted(self, result=None):
        self._terminal(PREEMPTED, result)

    def _terminal(self, status, result):
        with self._lock:
            gid = self._cur_id
        self._finish(gid, status, result if result is not None else self.spec.Result())

    def _finish(self, gid, status, result):
        res = self.spec.ActionResult()
        res.header.stamp = now()
        res.status.goal_id.id = gid or ""
        res.status.status = status
        res.result = result
        with self._lock:
            self._cur_status = status
            self._statuses[gid] = status
        self._result_pub.publish(res)
        self._publish_status()
        loginfo("action %s: goal %s finished (status %d)" % (self.ns, gid, status))
