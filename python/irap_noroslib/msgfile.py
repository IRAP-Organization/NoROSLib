"""Load ROS `.msg`, `.srv` and `.action` FILES from disk.

Copy a `.msg` file off a real ROS robot and hand irap_noroslib its **full path** --
no catkin package, no ROS install, nothing else needed::

    import irap_noroslib
    from irap_noroslib import load_msg_file

    CustomData = load_msg_file("/home/me/msgs/CustomData.msg", "my_robot_msgs")

    pub = irap_noroslib.Publisher("/data", CustomData)
    pub.publish(CustomData(id=7, label="hi"))

The md5sum and the wire codec are derived from the file, so the type is exactly
what real ROS computes (`rosmsg md5`) and real ROS nodes accept it.

**One file, one call.** If you have several custom messages, load each one by its
own path::

    load_msg_file("/home/me/msgs/Reading.msg",      "my_robot_msgs")
    load_msg_file("/home/me/msgs/CustomData.msg",   "my_robot_msgs")
    load_msg_file("/home/me/msgs/StressNested.msg", "my_robot_msgs")

Order does not matter: a type that nests another (`Reading[] readings`) resolves it
when it is first used, so as long as every `.msg` it needs has been loaded by then,
you are fine. Miss one and you get an error naming exactly which file to add.

The **package name** is the `my_robot_msgs` part of the ROS type name
`my_robot_msgs/CustomData`. Real ROS identifies types by that full name, so pass
the package the message came from. If the file happens to still sit in a catkin
layout (`<pkg>/msg/<Type>.msg`), the package is inferred and you can omit it.
"""
import os

from .message import define_message, registry
from .srv import define_service
from .actionlib import define_action

# full_type -> the file it came from, for error messages and re-load checks.
_loaded = {}


def loaded_files():
    """Map of every type loaded from a file -> the path it came from."""
    return dict(_loaded)


def _read(path):
    p = os.path.abspath(os.path.expanduser(path))
    if not os.path.isfile(p):
        raise IOError("no such file: %s" % path)
    with open(p) as f:
        return p, f.read()


def _split(text, n, what):
    """Split .srv/.action text on lines that are exactly '---'."""
    parts, cur = [], []
    for line in text.splitlines():
        if line.strip() == "---":
            parts.append("\n".join(cur))
            cur = []
        else:
            cur.append(line)
    parts.append("\n".join(cur))
    if len(parts) != n:
        raise ValueError("%s: expected %d sections separated by '---', found %d"
                         % (what, n, len(parts)))
    return [p or "\n" for p in parts]


def _full_type(path, pkg, subdir, ext):
    """'<pkg>/<Type>' for a file. `pkg` wins; otherwise infer a catkin layout."""
    name = os.path.basename(path)
    if not name.endswith(ext):
        raise ValueError("expected a %s file, got %r" % (ext, path))
    type_name = name[:-len(ext)]
    if not pkg:
        # only works if the file still sits in <pkg>/<subdir>/<Type><ext>
        parent = os.path.dirname(path)
        if os.path.basename(parent) == subdir and os.path.dirname(parent):
            pkg = os.path.basename(os.path.dirname(parent))
        else:
            raise ValueError(
                "cannot tell which ROS package %r belongs to. Pass the package "
                "name, e.g. load_%s_file(%r, \"my_robot_msgs\") -- ROS names a "
                "type \"pkg/%s\", so it needs the package the message came from."
                % (path, subdir, path, type_name))
    return "%s/%s" % (pkg, type_name)


def load_msg_file(path, pkg=None):
    """Load one `.msg` file by full path and return the message class.

    `pkg` is the ROS package the message came from (the "my_robot_msgs" in
    "my_robot_msgs/CustomData"). It may be omitted only if the file is still in a
    `<pkg>/msg/<Type>.msg` layout.
    """
    p, text = _read(path)
    full_type = _full_type(p, pkg, "msg", ".msg")
    cls = define_message(full_type, text)
    _loaded[full_type] = p
    return cls


def load_srv_file(path, pkg=None):
    """Load one `.srv` file (request '---' response) and return the service class."""
    p, text = _read(path)
    full_type = _full_type(p, pkg, "srv", ".srv")
    req, resp = _split(text, 2, full_type)
    srv = define_service(full_type, req, resp)
    _loaded[full_type] = p
    return srv


def load_action_file(path, pkg=None):
    """Load one `.action` file (goal '---' result '---' feedback).

    Returns the action spec and registers all 7 ROS action message types.
    """
    p, text = _read(path)
    full_type = _full_type(p, pkg, "action", ".action")
    goal, result, feedback = _split(text, 3, full_type)
    act = define_action(full_type, goal, result, feedback)
    _loaded[full_type] = p
    return act


def load_msg_files(paths, pkg=None):
    """Convenience: load several `.msg` files (a list of paths) from one package."""
    return [load_msg_file(p, pkg) for p in paths]


def _missing_type_hint(full_type):
    """Tell the user exactly which file they still have to load."""
    pkg, _, name = full_type.partition("/")
    return ('unknown message type "%s". It is nested by a type you loaded, so load '
            'its file too:\n    load_msg_file("/path/to/%s.msg", "%s")'
            % (full_type, name or full_type, pkg))


registry.missing_hint = _missing_type_hint
