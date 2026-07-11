"""Minimal /rosout aggregator.

A real roscore auto-starts a `rosout` node that subscribes to `/rosout`
(rosgraph_msgs/Log, where every node sends its log messages) and republishes them
on `/rosout_agg` (the aggregated feed that rqt_console / rosconsole read). This is
that node, built on the noros client -- nr_roscore starts it automatically.

Run standalone against any master:

    python3 -m noros.rosout                 # uses $ROS_MASTER_URI
"""
import threading

LOG_TYPE = "rosgraph_msgs/Log"
LOG_DEF = """byte DEBUG=1
byte INFO=2
byte WARN=4
byte ERROR=8
byte FATAL=16
Header header
byte level
string name
string msg
string file
string function
uint32 line
string[] topics
"""


def log_class():
    """Return the rosgraph_msgs/Log message class, registering it if needed."""
    import noros
    cls = None
    try:
        cls = noros.get_message_class(LOG_TYPE)
    except Exception:
        cls = None
    if cls is None:
        cls = noros.define_message(LOG_TYPE, LOG_DEF)
    return cls


def run_aggregator(master_uri=None, host=None):
    """Subscribe /rosout and republish to /rosout_agg. Blocks (spins)."""
    import noros
    Log = log_class()
    if master_uri:
        noros.set_master_uri(master_uri)
    if host:
        noros.set_hostname(host)
    noros.init_node("/rosout")
    agg = noros.Publisher("/rosout_agg", Log)
    noros.Subscriber("/rosout", Log, lambda m: agg.publish(m))
    noros.spin()


def start_in_background(master_uri=None, host=None):
    """Start the aggregator in a daemon thread. Returns the thread."""
    t = threading.Thread(target=_safe_run, args=(master_uri, host),
                         name="nr-rosout", daemon=True)
    t.start()
    return t


def _safe_run(master_uri, host):
    try:
        run_aggregator(master_uri=master_uri, host=host)
    except Exception as e:  # never let the aggregator crash the master
        print("[nr_roscore] rosout aggregator stopped: %s" % e, flush=True)


def main(argv=None):
    import os
    run_aggregator(master_uri=os.environ.get("ROS_MASTER_URI"))


if __name__ == "__main__":
    main()
