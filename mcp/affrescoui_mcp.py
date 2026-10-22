#!/usr/bin/env python3
"""AffrescoUI MCP server: connects an AI agent to a running AffrescoUI host, in both directions.

    requests   affrescoui_studio ──"request"──▶ affrescoui_host ──agent.sock──▶ this server ──channel event──▶ AI agent
                                                                                    (or wait_for_request)
    changes    AI agent ──tools (submit_revision, reply, ...)──▶ this server ──agent.sock──▶ affrescoui_host ──▶ screen

The host (examples/host/host.cpp) renders a screen and doesn't care where changes come from. A person asks for a change in
plain words in the Studio; the request reaches the agent through this server; the agent reads the screen, sends
revisions (JSON Patch), and replies. Revisions follow the screen's permission rules (allow / ask / deny, like
Claude Code's): in the example templates the agent's changes become proposals the person reviews in the Studio,
unless the mode is acceptEdits or the reviewer said "don't ask again" for that kind of change. A proposal is a
branch the agent can keep pushing to. There is no tool to accept or reject: that is the reviewer's decision.

Requests reach the agent two ways:
  - pushed, as Claude Code channel events (research preview: start Claude Code with
    `--dangerously-load-development-channels server:affrescoui`);
  - pulled, with the wait_for_request tool, which works with any MCP client.

Usage:
    python3 mcp/affrescoui_mcp.py [--socket DIR] [--launch "command"]
    claude mcp add affrescoui -- python3 /path/to/AffrescoUI/mcp/affrescoui_mcp.py

DIR is where the host listens: --socket, else $AFFRESCOUI_SOCKET_DIR, else /tmp/affrescoui-<uid> on macOS and Linux
and %LOCALAPPDATA%\\affrescoui\\sockets on Windows. With --launch, the command is started if no host is running. Only
the Python standard library is needed (3.9+). The host is reached through a Unix domain socket (AF_UNIX), which no
released CPython offers on Windows yet: there the server starts, but its tools say why they can't reach the host.
"""

import collections
import itertools
import json
import os
import shlex
import shutil
import socket
import stat
import subprocess
import sys
import tempfile
import threading
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PROTOCOLS = ["2025-11-25", "2025-06-18", "2025-03-26", "2024-11-05"]
TIMEOUT = 10  # seconds to wait for the host to answer
GUIDE_TOPIC = 5000  # characters: about 1,500 tokens, the most a topic of the format guide gives
NO_UNIX_SOCKETS = ("this Python has no Unix domain sockets (socket.AF_UNIX), which the host listens on: on Windows "
                   "no released CPython has them yet, see mcp/README.md")


def guide_path():
    """SCREEN_FORMAT.md: next to this file in the installed package, else at the top of the repository."""
    packaged = os.path.join(os.path.dirname(os.path.abspath(__file__)), "SCREEN_FORMAT.md")
    return packaged if os.path.exists(packaged) else os.path.join(ROOT, "SCREEN_FORMAT.md")


def log(text):
    """Writes a line to stderr, the server's log. Its text can come from people (a request), so a backslash and the
    characters that aren't printable are escaped, as the host's log does: a line can't forge another one, nor send
    escape sequences to a terminal."""
    text = "".join(c if c.isprintable() else c.encode("unicode_escape").decode("ascii")
                   for c in str(text).replace("\\", "\\\\"))
    print(f"[affrescoui-mcp] {text}", file=sys.stderr, flush=True)


class UnknownTool(Exception):
    pass


def default_socket_dir():
    """Where the host listens unless --socket says otherwise: one rule for the host, the Studio and this server."""
    if os.environ.get("AFFRESCOUI_SOCKET_DIR"):
        return os.environ["AFFRESCOUI_SOCKET_DIR"]
    if os.name == "nt":  # a folder in the user's profile, which its ACL keeps private to the user
        local = os.environ.get("LOCALAPPDATA") or os.path.join(os.path.expanduser("~"), "AppData", "Local")
        return os.path.join(local, "affrescoui", "sockets")
    return os.path.join("/tmp", f"affrescoui-{os.getuid()}")


def check_socket_dir(path):
    """Refuses a socket directory another user could control (its path in /tmp is easy to guess and take first)."""
    try:
        info = os.lstat(path)
    except FileNotFoundError:
        return
    # Windows has no uid (st_uid is 0): its default folder is private by its ACL, so only the kind is checked there
    owned = os.name == "nt" or info.st_uid == os.getuid()
    if not stat.S_ISDIR(info.st_mode) or not owned:
        raise RuntimeError(f"{path} is not a directory owned by this user: remove it or pass --socket DIR")


def connect_unix(path):
    """A stream socket connected to the host at path, or a RuntimeError saying why there is none."""
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    try:
        sock.connect(path)
    except OSError:
        sock.close()
        raise RuntimeError(f"no AffrescoUI host is listening at {path}. Start one: ./build/affrescoui_host")
    return sock


class Host:
    """The running affrescoui_host, at the other end of agent.sock. Connects (again) when needed."""

    def __init__(self, socket_dir, launch, on_request):
        self.path = os.path.join(socket_dir, "agent.sock")
        self.launch = launch
        self.on_request = on_request
        self.sock = None
        self.lock = threading.Lock()
        self.ids = itertools.count(1)
        self.waiting = {}  # request id -> [threading.Event, reply]
        self.events = []   # what happened since the agent last asked
        self.rules = ("default", [])  # the mode and grants last reported

    def ensure(self):
        with self.lock:
            if self.sock:
                return
        # Before --launch: a host this Python can't reach would be started again on every call
        if not hasattr(socket, "AF_UNIX"):  # CPython on Windows, so far (python/cpython#77589)
            raise RuntimeError(NO_UNIX_SOCKETS)
        if not os.path.exists(self.path) and self.launch:
            log(f"no host running: starting {self.launch}")
            # Windows parses a command line itself, and shlex would drop the backslashes of its paths
            command = self.launch if os.name == "nt" else shlex.split(self.launch)
            try:
                subprocess.Popen(command, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, start_new_session=True)
            except OSError as failure:  # reported like a host that isn't there, not ending the server
                raise RuntimeError(f"couldn't start {self.launch}: {failure}")
            for _ in range(50):
                if os.path.exists(self.path):
                    break
                time.sleep(0.1)
        check_socket_dir(os.path.dirname(self.path))
        sock = connect_unix(self.path)
        with self.lock:
            self.sock = sock
        threading.Thread(target=self.read, args=(sock,), daemon=True).start()
        log(f"connected to {self.path}")

    def read(self, sock):
        """Reads the host's messages until it closes the connection. Whatever stops the reading closes it too, so the
        next call connects again instead of waiting for answers nobody reads."""
        try:
            for line in sock.makefile("r", encoding="utf-8", errors="replace"):
                try:
                    message = json.loads(line)
                except (ValueError, RecursionError):  # as Server.serve
                    continue
                if isinstance(message, dict):
                    self.dispatch(message)
        finally:
            with self.lock:
                if self.sock is sock:
                    self.sock = None
            sock.close()
            log("the host closed the connection")

    def dispatch(self, message):
        """One message from the host: the answer someone waits for, a person's request, or news for get_events."""
        kind = message.get("type")
        with self.lock:
            pending = self.waiting.get(message.get("id")) if isinstance(message.get("id"), str) else None
            if pending and kind != "request":
                pending[1] = message
                pending[0].set()
                return
        if kind == "request":
            self.on_request(message)
        elif kind == "event":
            self.note({"event": message.get("name"), "data": message.get("data")})
        elif kind in ("applied", "rejected") and "proposal" in message:  # a reviewer decided
            self.note({"proposal": message["proposal"],
                       "outcome": "merged" if kind == "applied" else "rejected",
                       "revision": message.get("revision"), "detail": message.get("error")})
        elif kind == "committed":
            self.note({"committed": message.get("revision"), "commit": message.get("short"),
                       "author": message.get("author"), "committer": message.get("committer"),
                       "message": message.get("message")})
        elif kind == "review":  # only news when the reviewer changed the mode or the "don't ask again" grants
            rules = (message.get("mode"), [g.get("rule") for g in message.get("grants") or [] if isinstance(g, dict)])
            if rules != self.rules:
                self.rules = rules
                self.note({"mode": rules[0], "grants": rules[1]})
        elif kind == "error":
            self.note({"error": message.get("message")})

    def note(self, event):
        with self.lock:
            self.events.append(event)

    def send(self, message):
        self.ensure()
        with self.lock:
            if not self.sock:
                raise RuntimeError("lost the connection to the host")
            self.sock.sendall((json.dumps(message) + "\n").encode("utf-8"))

    def request(self, message):
        """Sends a message and waits for the answer carrying the same id."""
        request_id = f"mcp-{next(self.ids)}"
        pending = [threading.Event(), None]
        with self.lock:
            self.waiting[request_id] = pending
        try:
            self.send({**message, "id": request_id})
            if not pending[0].wait(TIMEOUT):
                raise RuntimeError("the host did not answer")
            return pending[1]
        finally:
            with self.lock:
                self.waiting.pop(request_id, None)

    def take_events(self):
        with self.lock:
            events, self.events = self.events, []
        return events


def compact(value):
    """Minified JSON: what tool results carry (a reader never sees them, the agent pays per token)."""
    return json.dumps(value, separators=(",", ":"), ensure_ascii=False)


def diff(before, after, path=""):
    """A small JSON Patch from before to after: objects key by key, lists item by item when only their tail differs
    in length, anything else replaced whole."""
    if before == after:
        return []
    if isinstance(before, dict) and isinstance(after, dict):
        ops = []
        for key in before:
            pointer = path + "/" + str(key).replace("~", "~0").replace("/", "~1")
            if key not in after:
                ops.append({"op": "remove", "path": pointer})
            else:
                ops.extend(diff(before[key], after[key], pointer))
        for key in after:
            if key not in before:
                ops.append({"op": "add", "path": path + "/" + str(key).replace("~", "~0").replace("/", "~1"),
                            "value": after[key]})
        return ops
    if isinstance(before, list) and isinstance(after, list):
        common = min(len(before), len(after))
        ops = []
        for i in range(common):
            ops.extend(diff(before[i], after[i], f"{path}/{i}"))
        for i in range(len(before) - 1, common - 1, -1):
            ops.append({"op": "remove", "path": f"{path}/{i}"})
        for i in range(common, len(after)):
            ops.append({"op": "add", "path": f"{path}/-", "value": after[i]})
        if len(compact(ops)) > len(compact(after)):
            return [{"op": "replace", "path": path, "value": after}]
        return ops
    return [{"op": "replace", "path": path, "value": after}]


def changes_since(seen, document):
    """What get_screen returns after a first read: a JSON Patch from what the agent saw to the document now, its
    paths written from node ids, as edits are."""
    changes = diff(seen, document)
    for op in changes:
        if op["path"].startswith(("/root", "/screens", "/templates", "/components")):
            op["path"] = by_id(document if op["op"] != "remove" else seen, op["path"])
    return changes


def by_id(document, pointer):
    """A JSON Pointer written from the nearest node with an id: "/root/children/2/children/1/text" is "#title/text"."""
    parts = pointer.split("/")[1:]
    node, best = document, None
    for i, part in enumerate(parts):
        try:
            node = node[int(part)] if isinstance(node, list) else node[part.replace("~1", "/").replace("~0", "~")]
        except (KeyError, IndexError, ValueError, TypeError):
            break
        if isinstance(node, dict) and isinstance(node.get("id"), str):
            found = find_node(document, node["id"])
            if found and found[0] == "/" + "/".join(parts[:i + 1]):  # the id finds this very node
                best = (node["id"], parts[i + 1:])
    if best is None:
        return pointer
    return "#" + best[0] + ("/" + "/".join(best[1]) if best[1] else "")


def find_node(document, node_id):
    """The JSON Pointer and node with an id (in root, screens, templates, components), like the host's #id paths."""
    def search(node, pointer):
        if not isinstance(node, dict):
            return None
        if node.get("id") == node_id:
            return pointer, node
        for i, child in enumerate(node.get("children") or []):
            found = search(child, f"{pointer}/children/{i}")
            if found:
                return found
        return None
    found = search(document.get("root"), "/root")
    for section in ("screens", "templates"):
        for name, node in (document.get(section) or {}).items():
            found = found or search(node, f"/{section}/{name}")
    for name, component in (document.get("components") or {}).items():
        found = found or search((component or {}).get("node"), f"/components/{name}/node")
    return found


def trees(document):
    """Where a document's nodes start, as (JSON Pointer, node): root, screens, templates, then components' nodes."""
    yield "/root", document.get("root")
    for section in ("screens", "templates"):
        for name, node in (document.get(section) or {}).items():
            yield f"/{section}/{name}", node
    for name, component in (document.get("components") or {}).items():
        yield f"/components/{name}/node", (component or {}).get("node")


def walk_nodes(node, address, depth=0, parent=None):
    """Every node of a tree, parents first, as (node, address, depth, parent's address). The address is "#id", or a
    path from the nearest node with one."""
    if not isinstance(node, dict) or node.get("slot") is True:
        return
    if isinstance(node.get("id"), str) and node["id"]:
        address = "#" + node["id"]
    yield node, address, depth, parent
    for i, child in enumerate(node.get("children") or []):
        yield from walk_nodes(child, f"{address}/children/{i}", depth + 1, address)


def node_type(node, styles=None):
    """A node's type as the outline writes it: its "type", else its style's (styles may name styles), else "panel";
    "use <component>" for a use."""
    if "use" in node:
        return "use " + str(node["use"])
    for _ in range(16):  # styles naming styles, as deep as the runtime follows them
        if isinstance(node.get("type"), str):
            return node["type"]
        style = node.get("style")
        if not isinstance(style, str) or not isinstance((styles or {}).get(style), dict):
            break
        node = styles[style]
    return "panel"


def short(value, length=24):
    """A value in a few characters: text quoted, cut with "..."."""
    text = value if isinstance(value, str) else compact(value)
    return compact(text[:length] + "..." if len(text) > length else text)


def summary(node, address, styles=None):
    """What an agent needs to know of a node to find and change it, without its styling: its type (unless its id
    starts with it, as given ids do: "#label-blocked"), its text, and what it binds, repeats, shows when, looks like
    (style) and runs on a click."""
    kind = node_type(node, styles)
    bits = [] if address.startswith(f"#{kind}-") else [kind]
    for key in ("text", "placeholder", "tooltip"):
        if isinstance(node.get(key), str):
            bits.append(short(node[key]) if key == "text" else f"{key}={short(node[key])}")
    for key in ("bind", "onClick"):
        if isinstance(node.get(key), str):
            bits.append(f"{key}={node[key]}")
    if isinstance(node.get("repeat"), dict):
        bits.append(f"repeat {node['repeat'].get('for', 'item')} in {node['repeat'].get('in')}")
    if "when" in node:
        bits.append("when")
    return " ".join(bits)


TYPE_WORDS = {"string": "text", "number": "number", "integer": "whole number", "boolean": "boolean",
              "object": "object", "array": "list", "null": "null"}


def describe_type(kind, depth=0):
    """A state type in a few words, as the runtime says it (StateTypes.cpp, describeType): "number", "'all' | 'bug'",
    "list of {title: text}"."""
    if isinstance(kind, bool):
        return "anything" if kind else "nothing"
    if not isinstance(kind, dict):
        return "anything"
    if isinstance(kind.get("enum"), list):
        values = ["'" + v + "'" if isinstance(v, str) else compact(v) for v in kind["enum"][:8]]
        return " | ".join(values) + (" | ..." if len(kind["enum"]) > 8 else "")
    if "const" in kind:
        return compact(kind["const"])
    names = kind.get("type")
    said = []
    for name in names if isinstance(names, list) else [names]:
        if name == "array" and "items" in kind:
            said.append("list of ..." if depth >= 3 else "list of " + describe_type(kind["items"], depth + 1))
        elif name == "object" and isinstance(kind.get("properties"), dict):
            said.append("{...}" if depth >= 3 else "{" + ", ".join(
                f"{key}: {describe_type(inside, depth + 1)}" for key, inside in kind["properties"].items()) + "}")
        elif name in TYPE_WORDS:
            said.append(TYPE_WORDS[name])
    return " | ".join(said) or "anything"


def state_types(document):
    """The state's declared types ("stateTypes") on one line, for the outline: "state: filter: 'all' | 'bug';
    volume: number"; "" if it declares none."""
    types = document.get("stateTypes")
    if not isinstance(types, dict) or not types:
        return ""
    return "state: " + "; ".join(f"{key}: {describe_type(kind)}" for key, kind in types.items())


def outline(document, at=None, depth=None):
    """The node tree, one line per node: its address, then summary(). `at` (a node's address or pointer, found with
    locate) starts it there; `depth` stops it that many levels down, and a node with more inside says how many
    ("+12"). Enough to choose where to edit, without the styling. The whole outline starts with the state's types,
    when the screen declares them."""
    starts = [locate(document, at)] if at else trees(document)
    styles = document.get("styles")
    lines = [[state_types(document), 0]] if not at and state_types(document) else []  # [text, nodes not shown]
    for pointer, start in starts:
        cut = None  # the last line at the deepest level shown: what is below it isn't
        for node, address, level, _ in walk_nodes(start, pointer):
            if depth is not None and level > depth:
                cut[1] += 1
                continue
            lines.append([" " * level + f"{address} {summary(node, address, styles)}".rstrip(), 0])
            if depth is not None and level == depth:
                cut = lines[-1]
    return "\n".join(text + (f" +{inside}" if inside else "") for text, inside in lines)


def locate(document, at):
    """The JSON Pointer and value at "#id" (a node), "#id/rest", or a JSON Pointer; a ValueError says why there is
    none."""
    pointer = str(at)
    if pointer.startswith("#"):
        node_id, _, rest = pointer[1:].partition("/")
        found = find_node(document, node_id)
        if not found:
            raise ValueError(f"no node with id '{node_id}'")
        pointer = found[0] + ("/" + rest if rest else "")
    value = document
    try:
        for part in [p for p in pointer.split("/") if p]:
            value = value[int(part)] if isinstance(value, list) else value[part.replace("~1", "/").replace("~0", "~")]
    except (KeyError, IndexError, ValueError, TypeError):
        raise ValueError(f"nothing at {at}")
    return pointer, value


def pruned(node, address, depth):
    """A node with what is deeper than `depth` levels below it replaced by its children's addresses:
    "children": ["#label-a", "#row-b"]. Read one of them with at= to go further."""
    if not isinstance(node, dict) or not isinstance(node.get("children"), list):
        return node
    children = []
    for i, child in enumerate(node["children"]):
        named = isinstance(child, dict) and isinstance(child.get("id"), str) and child["id"]
        child_address = "#" + child["id"] if named else f"{address}/children/{i}"
        children.append(child_address if depth <= 0 else pruned(child, child_address, depth - 1))
    return {**node, "children": children}


def find_nodes(document, kind=None, text=None, prop=None, value=None, limit=20):
    """The nodes that match every criterion given: their type (`kind`, or the component of a use), text (in their
    text, placeholder or tooltip, any case), and a property they have (`prop`), with a value (`value`, compared as
    written). One line each, as in the outline, with the parent's address."""
    needle = text.lower() if isinstance(text, str) else None
    styles = document.get("styles")
    found = []
    for pointer, start in trees(document):
        for node, address, _, parent in walk_nodes(start, pointer):
            if kind and kind not in (node_type(node, styles), node.get("use")):
                continue
            if needle is not None and not any(isinstance(node.get(k), str) and needle in node[k].lower()
                                              for k in ("text", "placeholder", "tooltip")):
                continue
            if prop and (prop not in node or (value is not None and node[prop] != value
                                              and compact(node[prop]) != str(value))):
                continue
            found.append(f"{address} {summary(node, address, styles)}".rstrip() + (f" in {parent}" if parent else ""))
    shown = found[:max(1, int(limit))]
    more = f"\n... and {len(found) - len(shown)} more: narrow the search or raise limit" if len(found) > len(shown) \
        else ""
    return f"{len(found)} found" + ("\n" + "\n".join(shown) if shown else "") + more


def guide_sections(text):
    """SCREEN_FORMAT.md as its sections: (title, text) for the introduction ("introduction") and each "##" and "###"
    heading; a "##" section includes its "###" ones, or names them if they would make it longer than GUIDE_TOPIC."""
    lines = text.split("\n")
    sections, starts = [], [(0, 1, "introduction")]
    fence = False
    for i, line in enumerate(lines):
        fence = fence != line.startswith("```")
        if not fence and line.startswith("## "):
            starts.append((i, 2, line[3:].strip()))
        elif not fence and line.startswith("### "):
            starts.append((i, 3, line[4:].strip()))
    for n, (start, level, title) in enumerate(starts):
        end = next((s for s, other, _ in starts[n + 1:] if other <= level), len(lines))
        if level == 1:  # the introduction ends where the first section starts
            end = starts[1][0] if len(starts) > 1 else len(lines)
        part = "\n".join(lines[start:end]).strip()
        inner = [(s, t) for s, other, t in starts[n + 1:] if start < s < end and other == level + 1]
        if level == 2 and inner and len(part) > GUIDE_TOPIC:
            part = ("\n".join(lines[start:inner[0][0]]).strip() + "\n\nMore in the topics: " +
                    ", ".join(t.replace("`", "") for _, t in inner) + ".")
        sections.append((title.replace("`", ""), part))
    return sections


def format_guide(text, topic=None):
    """The part of the format guide an agent asks for: by default the introduction and the list of topics; a topic
    (a heading, any case, or a word in it) gives that section; "all" gives everything."""
    sections = guide_sections(text)
    if topic and str(topic).strip().lower() == "all":
        return text
    if topic:
        wanted = str(topic).strip().lower().replace("`", "")
        matches = [s for s in sections if s[0].lower() == wanted] or [s for s in sections if wanted in s[0].lower()]
        if not matches:
            return None
        also = [title for title, _ in matches[1:]]
        return matches[0][1] + (f"\n\n(Also matching: {', '.join(also)}.)" if also else "")
    titles = ", ".join(title for title, _ in sections[1:])
    return (sections[0][1] + "\n\nTopics (pass one as `topic`, or \"all\" for the whole guide): " + titles + ".")


def pointer_parts(pointer):
    """A JSON Pointer's segments, unescaped (RFC 6901): "/a~1b/0" -> ["a/b", "0"]."""
    if pointer == "":
        return []
    if not pointer.startswith("/"):
        raise ValueError(f"not a JSON Pointer: {pointer!r}")
    return [part.replace("~1", "/").replace("~0", "~") for part in pointer[1:].split("/")]


def apply_patch(document, patch):
    """A copy of document with an RFC 6902 JSON Patch applied (what get_diff returns for a proposal)."""
    root = {"": json.loads(json.dumps(document))}  # a holder, so the whole document can be replaced too

    def parent(pointer):
        parts = [""] + pointer_parts(pointer)
        at = root
        for part in parts[:-1]:
            at = at[int(part)] if isinstance(at, list) else at[part]
        return at, parts[-1]

    def take(pointer, remove):
        holder, key = parent(pointer)
        index = int(key) if isinstance(holder, list) else key
        return holder.pop(index) if remove else holder[index]

    def put(pointer, value, replace):
        holder, key = parent(pointer)
        if isinstance(holder, list):
            index = len(holder) if key == "-" else int(key)
            holder[index:index + (1 if replace else 0)] = [value]
        else:
            holder[key] = value

    for operation in patch:
        kind, path = operation["op"], operation["path"]
        if kind in ("add", "replace"):
            put(path, operation["value"], kind == "replace")
        elif kind == "remove":
            take(path, True)
        elif kind in ("move", "copy"):
            put(path, take(operation["from"], kind == "move"), False)
        elif kind == "test" and take(path, False) != operation["value"]:
            raise ValueError(f"test failed at {path}")
    return root[""]


def find_cli():
    """The affrescoui command-line tool: $AFFRESCOUI_CLI, else on the PATH, else in the repository's build folder."""
    candidates = [os.environ.get("AFFRESCOUI_CLI"), shutil.which("affrescoui"),
                  os.path.join(ROOT, "build", "affrescoui.exe" if os.name == "nt" else "affrescoui")]
    for candidate in candidates:
        if candidate and os.path.isfile(candidate):
            return candidate
    raise RuntimeError("the affrescoui tool isn't found: build it (cmake --build build), put it on the PATH, or set "
                       "AFFRESCOUI_CLI")


def repository_path(path):
    """A path as given, or relative to the repository when it doesn't exist as given."""
    return path if os.path.isabs(path) or os.path.exists(path) else os.path.join(ROOT, path)


def scenarios_report(report, shown):
    """affrescoui test --json's report of one file, as a few lines: the totals, then one line per failure."""
    lines = [f"{report.get('file')} on {shown}: {report.get('passed')} passed, {report.get('failed')} failed"]
    for failure in report.get("failures") or []:
        step = f", step {failure['step']}" if failure.get("step") else ""
        lines.append(f"FAILED {failure.get('scenario')}{step}: {failure.get('problem')}")
    return "\n".join(lines)


def object_schema(properties=None, required=None):
    schema = {"type": "object", "properties": properties or {}}
    if required:
        schema["required"] = required
    return schema


TOOLS = [
    {"name": "wait_for_request",
     "description": "Wait for the next change request a person sends from the AffrescoUI Studio, and return it "
                    "(request_id, from, text). Returns right away if some are already waiting; returns "
                    "{\"request\": null} after the timeout. Not needed when requests arrive as <channel> events.",
     "inputSchema": object_schema({"timeout_seconds": {"type": "integer", "description": "Default 120, at most 600"}})},
    {"name": "reply",
     "description": "Answer a request in the Studio: say what you changed, or ask a question. Call it once you "
                    "have submitted the revisions for a request.",
     "inputSchema": object_schema({"request_id": {"type": "string"}, "text": {"type": "string"}},
                                  ["request_id", "text"])},
    {"name": "answer_repair",
     "description": "Answer a repair request (a wait_for_request result with kind \"repair\": a change set that no "
                    "longer applies, or whose scenarios fail, on the new template). Give the change set's request "
                    "redone on the new base as one JSON Patch; it always becomes a proposal for a reviewer, and the "
                    "scenarios in repair.proof must pass on it.",
     "inputSchema": object_schema({"request_id": {"type": "string"}, "patch": {"type": "array"},
                                   "message": {"type": "string"}}, ["request_id", "patch", "message"])},
    {"name": "get_screen",
     "description": "The running screen: its document (AffrescoUI Screen Format, with the live state as its \"state\" "
                    "and its permission rules), current revision and head commit, open proposals, the mode, the "
                    "reviewer's \"don't ask again\" grants, and your author name. The first call returns everything; "
                    "later calls return only what changed since your last read, as a JSON Patch (\"changes\"), or "
                    "\"unchanged\". Pass full=true to get everything again (e.g. after your context was cleared), "
                    "at=\"#id\" for one node only, or outline=true for the node tree: one line per node, its "
                    "address (every node has an id: \"#label-blocked\"), type, text, bind, style, onClick, repeat and "
                    "when, without the styling. depth limits both to that many levels below (outline lines then say "
                    "\"+12\" for the nodes inside; a node's deeper children become their addresses). Read the outline "
                    "first, then the nodes you change with at.",
     "inputSchema": object_schema({"full": {"type": "boolean", "description": "Everything, not what changed"},
                                   "at": {"type": "string", "description": "Only this node: \"#id\" or a JSON Pointer"},
                                   "outline": {"type": "boolean",
                                               "description": "The node tree: one line per node (address, type, text, "
                                                              "bind, style, onClick, repeat, when)"},
                                   "depth": {"type": "integer",
                                             "description": "With at or outline: levels below to include (0: the "
                                                            "node itself)"}})},
    {"name": "find_nodes",
     "description": "Search the screen's nodes (root, screens, templates, components) and get one line per match, as "
                    "in the outline, with its parent: by type (\"label\", or a component's name), by text (in the "
                    "text, placeholder or tooltip, any case), by a property it has (property, optionally with value). "
                    "Criteria combine. Cheaper than reading the whole screen to find where to change something.",
     "inputSchema": object_schema({"type": {"type": "string", "description": "A node type or component name"},
                                   "text": {"type": "string", "description": "Text the node shows (part of it)"},
                                   "property": {"type": "string", "description": "A property the node has"},
                                   "value": {"description": "The property's value (optional)"},
                                   "limit": {"type": "integer", "description": "Most lines (default 20)"}}),
     "annotations": {"readOnlyHint": True}},
    {"name": "get_catalog",
     "description": "What this screen's format offers: node types and their properties, events, steps, expression "
                    "functions, variables, sections, and the app's registered custom types and native functions.",
     "inputSchema": object_schema()},
    {"name": "get_contributions",
     "description": "What an app offers to features from other apps, and what it can install: its contribution "
                    "points (slot, the exports each receives), its typed exports (state, lists, actions, models, "
                    "surfaces, images) and the installable parts it ships (each one's needs and provides), one line "
                    "each. Without file, the running screen; with file, another app's screen file (e.g. "
                    "\"examples/sinopia/sinopia.screen.json\"), to pick a part to install from it.",
     "inputSchema": object_schema({"file": {"type": "string", "description": "An app's *.screen.json (optional)"}}),
     "annotations": {"readOnlyHint": True}},
    {"name": "check_composition",
     "description": "Prove that a feature from another app fits the running screen: binds its needs to the "
                    "screen's exports (by name, or through your adapter), checks kinds, slots, ids and imports, "
                    "refuses widened permissions and capabilities, checks the composed document, then plays the "
                    "feature's and the app's scenarios on it. Returns the proof's verdict and reasons: each unmet need "
                    "and why, each failed check and scenario. An adapter is {\"format\": \"affrescoui-adapter\", "
                    "\"version\": 1, \"feature\", \"host\", \"bind\": {need: {\"export\": name} | {\"value\": "
                    "\"{{expression over app.<export>}}\"} | {\"action\": name, \"with\": {...}} | {\"point\": name}}} "
                    "(SCREEN_FORMAT.md, \"Adapters\").",
     "inputSchema": object_schema({
         "screen": {"type": "string", "description": "The running app's screen file, for its folder and scenarios"},
         "feature": {"type": "string", "description": "The feature's folder or feature.json, in its own app"},
         "adapter": {"type": "object", "description": "The adapter (optional: needs bind by name without one)"},
     }, ["screen", "feature"]),
     "annotations": {"readOnlyHint": True}},
    {"name": "install_feature",
     "description": "Install a feature from another app once check_composition proves it: the same proof, then one "
                    "revision that adds the feature's record (its bindings and proof) to the screen's features. It "
                    "is reviewed like any of your revisions (applied, proposed or rejected), shows in history and "
                    "blame, and undoes like one. A failed proof installs nothing and returns its reasons.",
     "inputSchema": object_schema({
         "screen": {"type": "string", "description": "The running app's screen file, for its folder and scenarios"},
         "feature": {"type": "string", "description": "The feature's folder or feature.json, in its own app"},
         "adapter": {"type": "object", "description": "The adapter (optional)"},
     }, ["screen", "feature"])},
    {"name": "get_format_guide",
     "description": "The AffrescoUI Screen Format specification, a part at a time: without topic, its introduction "
                    "and the list of topics (nodes, layout, values and expressions, events, actions, revisions, ...); "
                    "with topic, that section (a heading or a word of one, e.g. \"Revisions\", \"label\", "
                    "\"Events\"); topic \"all\" gives the whole guide.",
     "inputSchema": object_schema({"topic": {"type": "string",
                                             "description": "A section's heading, a word of it, or \"all\""}})},
    {"name": "submit_revision",
     "description": "Change the running screen (layout, styles, colors, behaviour/actions, or state), like a git "
                    "commit, with compact edits: set (path -> value: replaces, or adds where nothing is), add (path -> "
                    "value: inserts; \"/-\" appends to a list), remove (paths), move (from -> to); or a JSON Patch "
                    "(RFC 6902) in patch. Paths address nodes by id: \"#statusBar/color\", \"#card/children/-\", "
                    "\"#oldBadge\" (the node itself); \"/state/...\" changes live data. Example: {\"message\": \"Dark "
                    "status bar\", \"set\": {\"#statusBar/color\": \"@topBar\"}}. The result is: applied (a commit on "
                    "main); proposed (a branch a reviewer previews and merges or rejects in the Studio: get_events "
                    "reports the decision); or rejected with an error saying why (fix it and retry). To refine an "
                    "open proposal of yours (e.g. after the person asks for more), pass its number as `proposal`: the "
                    "commit is added to that branch instead of opening a new one.",
     "inputSchema": object_schema({
         "message": {"type": "string", "description": "One line, for the reviewer, saying what the change does"},
         "set": {"type": "object", "description": "path -> new value (replace, or add where nothing is)"},
         "add": {"type": "object", "description": "path -> value to insert (\"/-\" appends to a list)"},
         "remove": {"type": "array", "items": {"type": "string"}, "description": "Paths to remove"},
         "move": {"type": "object", "description": "from path -> to path"},
         "patch": {"type": "array", "items": {"type": "object"}, "description": "JSON Patch operations (optional "
                                                                             "when using set/add/remove/move)"},
         "parent": {"type": ["integer", "string"],
                    "description": "The revision or commit id this is based on (optional; if the screen has moved "
                                   "on, it is rejected as a conflict)"},
         "proposal": {"type": "integer", "description": "Push onto this open proposal of yours (optional)"},
     }, ["message"])},
    {"name": "render_screen",
     "description": "A picture of the running screen (PNG), to check your work before a person reviews it: main as "
                    "it is now, or with an open proposal applied (`proposal`, e.g. the one submit_revision just "
                    "opened). Drawn offscreen: the person's window doesn't change. `scale` 2 draws at twice the "
                    "size, like a Retina display.",
     "inputSchema": object_schema({"proposal": {"type": "integer",
                                                "description": "An open proposal to show applied (optional)"},
                                   "scale": {"type": "integer", "description": "1 (default) or 2"}}),
     "annotations": {"readOnlyHint": True}},
    {"name": "get_evidence",
     "description": "The evidence a reviewer sees for an open proposal, gathered by the host itself (an author can't "
                    "supply it): pictures of main and of the proposal applied, the region of pixels that differ "
                    "({x, y, width, height}), lines saying what it changes in behaviour, the screen's scenarios played "
                    "on it (name, passed, message; those that need a window aren't played), and the texts it changes "
                    "(node, original, modified). Check it before asking for a review.",
     "inputSchema": object_schema({"proposal": {"type": "integer", "description": "The open proposal"},
                                   "scale": {"type": "integer", "description": "1 (default) or 2"}}, ["proposal"]),
     "annotations": {"readOnlyHint": True}},
    {"name": "run_scenarios",
     "description": "Check your work with the screen's own scenarios (scripted clicks, typing, waits and expectations "
                    "of state and nodes, played headless): on main as it runs now, or with an open proposal applied "
                    "(`proposal`, e.g. the one submit_revision just opened). `file` is the scenarios file shipped "
                    "next to the screen (examples/flow/kanban.scenarios.json). Pass `scenarios` (a list of "
                    "{name, steps}) to play your own instead of the file's, `only` for one by name. Answers in a few "
                    "lines: the totals, then \"FAILED <scenario>, step <n>: <what>\" per failure. Nothing changes: "
                    "scenarios play on a copy, without a window. SCREEN_FORMAT.md, \"Scenarios\", lists the steps.",
     "inputSchema": object_schema({
         "file": {"type": "string", "description": "A *.scenarios.json file: absolute, or relative to the repository"},
         "proposal": {"type": "integer", "description": "An open proposal to check, applied to main (optional)"},
         "only": {"type": "string", "description": "Play only the scenario of this name (optional)"},
         "scenarios": {"type": "array", "items": {"type": "object"},
                       "description": "Scenarios to play instead of the file's, on its screen (optional)"},
     }, ["file"]),
     "annotations": {"readOnlyHint": True}},
    {"name": "get_history",
     "description": "main's commits, oldest first (like git log): number, short id, author, committer (who approved "
                    "it), message, time, and parents (two for a merged proposal). Revision 0 is the file as loaded.",
     "inputSchema": object_schema({"limit": {"type": "integer", "description": "Only the newest N (optional)"}})},
    {"name": "get_diff",
     "description": "What changed, as a JSON Patch (like git diff): between two revisions (`from`, `to`: numbers, "
                    "commit ids, \"HEAD\", \"HEAD~1\"; default HEAD~1 to HEAD), or what accepting an open proposal "
                    "would change now (`proposal`).",
     "inputSchema": object_schema({"from": {"type": ["integer", "string"]}, "to": {"type": ["integer", "string"]},
                                   "proposal": {"type": "integer"}})},
    {"name": "get_blame",
     "description": "Who last changed a node or a value, and when (like git blame): the commit on main that last "
                    "changed `target`, a node (\"#board\", or \"#board/text\" inside it) or a path "
                    "(\"/colors/accent\", \"/state/cards\"). Gives its revision, id, author, committer (who applied "
                    "or approved it), message and time; revision 0 if it is as the screen file has it.",
     "inputSchema": object_schema({"target": {"type": "string"}}, ["target"])},
    {"name": "restore_revision",
     "description": "Bring the whole screen back to how it was at an earlier revision (like git restore). This "
                    "creates a new revision (the history only grows) and follows the same permissions as any change.",
     "inputSchema": object_schema({"revision": {"type": ["integer", "string"]}}, ["revision"])},
    {"name": "revert_revision",
     "description": "Undo one earlier revision and keep everything after it (like git revert): a new revision with "
                    "the inverse change. Refused as a conflict if later revisions changed the same parts. Follows "
                    "the same permissions as any change.",
     "inputSchema": object_schema({"revision": {"type": ["integer", "string"]}}, ["revision"])},
    {"name": "get_events",
     "description": "What happened since the last call: reviewers' decisions on your proposals (merged or rejected, "
                    "with their reason), commits by anyone, mode and grant changes, events the screen emitted, "
                    "errors. Returns them and clears the list.",
     "inputSchema": object_schema()},
    {"name": "export_changes",
     "description": "The revisions after `from` as a change set: a portable \"plugin\" file another copy of the "
                    "same screen can import (affrescoui_host --plugin).",
     "inputSchema": object_schema({"from": {"type": ["integer", "string"]}, "name": {"type": "string"}})},
]

# Several agents at once (VERSION_CONTROL.md, "Leases"): claim a part of the screen while you work on it, so other
# authors' revisions that touch it are refused; it ends after its seconds, when released, or when you disconnect.
COORDINATION_TOOLS = [
    {"name": "lease_part",
     "description": "Claim a part of the screen (\"#id\" or a JSON Pointer such as /state/columns/0) while you work "
                    "on it: other authors' revisions that touch it are refused until it ends (after `seconds`, 300 "
                    "by default, at most 3600; when you release it; or when you disconnect). Refused if someone "
                    "else holds it or a part inside it, or if your permissions don't let you change it.",
     "inputSchema": object_schema({"target": {"type": "string"}, "seconds": {"type": "integer"}}, ["target"])},
    {"name": "renew_lease",
     "description": "Make a lease you hold last `seconds` (300 by default, at most 3600) from now.",
     "inputSchema": object_schema({"target": {"type": "string"}, "seconds": {"type": "integer"}}, ["target"])},
    {"name": "release_part",
     "description": "End a lease you hold, as soon as you are done with that part.",
     "inputSchema": object_schema({"target": {"type": "string"}}, ["target"])},
    {"name": "get_usage",
     "description": "The screen's usage summaries (VERSION_CONTROL.md, \"Usage\"): per node id and action, how often "
                    "it ran and failed; the screens visited; the controls never used, over the last 30 days. Only "
                    "ids, counts and times. Refused unless the host switched them on with the person's consent and "
                    "your connection may review.",
     "inputSchema": object_schema(),
     "annotations": {"readOnlyHint": True}},
]
TOOLS += COORDINATION_TOOLS


def coordination_tool(host, client, name, args):
    """Runs a lease tool or get_usage; None if `name` is none of them."""
    if name == "get_usage":
        reply = host.request({"type": "getUsage"})
        if reply.get("type") == "usage":
            return compact(reply.get("usage")), False
        return f"refused: {reply.get('error')}", True
    if name not in ("lease_part", "renew_lease", "release_part"):
        return None
    message = {"type": "release" if name == "release_part" else "lease", "author": client,
               "target": str(args.get("target", ""))}
    if name != "release_part":
        message["seconds"] = args.get("seconds", 300)
    reply = host.request(message)
    if reply.get("type") in ("leased", "released"):
        return compact(reply), False
    return f"refused: {reply.get('error')}", True


PROMPTS = [
    {"name": "serve_studio",
     "description": "Work through the change requests people send from the AffrescoUI Studio, one after another."},
]

SERVE_PROMPT = (
    "You are the builder behind a live AffrescoUI app. People ask for changes in the AffrescoUI Studio. Repeat: call "
    "wait_for_request; for each request, find what to change (get_screen with outline=true, or find_nodes; every "
    "node has an id), read just those parts (get_screen at=\"#id\"; after a first full read get_screen returns only "
    "what changed), and look up the format's parts you need (get_format_guide lists its topics), make "
    "the change with submit_revision (fix and retry if it is rejected), then tell the person what you did with "
    "reply. Keep changes focused on what was asked, and keep the existing style (colors, fonts, spacing). If a "
    "follow-up request refines a change that is still waiting for review, push to that proposal instead of opening "
    "a new one.")

INSTRUCTIONS = (
    "This server connects you to a running AffrescoUI app (a native C++/OpenGL UI described by a JSON screen document). "
    "People ask for changes from a separate client, the AffrescoUI Studio. Their requests arrive as "
    "<channel source=\"affrescoui\" request_id=\"...\" sender=\"...\"> events (if this session loaded the channel), or "
    "from the wait_for_request tool. For each request: find what to change with get_screen (outline=true, then "
    "at=\"#id\": every node has an id) or find_nodes (get_format_guide explains the format, one topic at a time), "
    "change it with submit_revision, then answer with reply, passing the request_id. Your changes may "
    "wait for a reviewer's approval in the Studio; get_events tells you the decision.")


# Permissions, budgets, data rules, roles and leases (VERSION_CONTROL.md): what a denial means, from the rule it names
DENIALS = (
    ("over the budget", "the screen's permissions limit how many nodes you add, remove and change in one revision; "
                        "split the change into smaller revisions"),
    ("data rule", "the screen's data rules limit the paths you may read and write; change only the paths they allow"),
    ("holds a lease", "another author is working on that part of the screen; wait for its lease to end or change "
                      "another part"),
    ("(rule \"", "a deny rule of the screen's permissions covers what you changed; leave that part as it is"),
    ("revisions can't change", "that section is a capability of the app, which no revision changes"),
)


def explain_denial(error):
    """The error of a refused revision, followed by what the rule it names means, if it is a permission denial."""
    text = str(error)
    if "permission denied" not in text:
        return text
    for marker, meaning in DENIALS:
        if marker in text:
            return f"{text}. The rule that matched: {meaning}."
    return text


def describe(reply):
    kind = reply.get("type")
    if kind == "applied":
        return (f"applied: the screen is now at revision {reply.get('revision')} "
                f"(commit {str(reply.get('commit', ''))[:7]})"), False
    if kind == "proposed":
        return (f"proposed as proposal {reply.get('proposal')} (head {str(reply.get('commit', ''))[:7]}): a reviewer "
                f"decides in the Studio. It is not applied yet; get_events will report the decision. Pass "
                f"proposal={reply.get('proposal')} to submit_revision to add to it."), False
    return f"rejected, nothing changed: {explain_denial(reply.get('error'))}", True


def screen_info(reply):
    """What get_screen says of the screen besides its document: revision, head, your author name, whether you can
    review, the mode, the grants and the open proposals."""
    head = reply.get("head") or {}
    return {"revision": reply.get("revision"), "head": head.get("short"), "you": reply.get("you"),
            "canReview": reply.get("canReview"), "mode": reply.get("mode"),
            "grants": [g.get("rule") for g in reply.get("grants") or []],
            "proposals": [{k: p.get(k) for k in ("id", "author", "message", "touches", "changes")}
                          for p in reply.get("proposals") or []]}


def malformed(request):
    """The error for a message the server can't use, or None: a batch (a JSON array, which MCP dropped in 2025-06-18),
    anything else that isn't an object, or params that aren't one. The server answers it and reads on."""
    if isinstance(request, dict) and isinstance(request.get("params", {}), dict):
        return None
    return {"jsonrpc": "2.0", "id": request.get("id") if isinstance(request, dict) else None,
            "error": {"code": -32600, "message": "invalid request: expected one JSON-RPC object"}}


def client_name(params):
    """The name the client gives itself in initialize (the revisions' author after "agent/"), or "agent"."""
    client = params.get("clientInfo")
    name = client.get("name") if isinstance(client, dict) else None
    return name if isinstance(name, str) and name else "agent"


class Server:
    def __init__(self, socket_dir, launch):
        self.client = "agent"
        self.out = threading.Lock()
        self.ready = False  # the client said notifications/initialized: channel events may be sent
        self.requests = collections.OrderedDict()  # request_id -> request, until answered
        self.delivered = set()  # given out by wait_for_request
        self.arrived = threading.Condition()
        self.host = Host(socket_dir, launch, self.on_request)
        self.seen = None  # the document (with the live state) the agent last read, for get_screen's changes

    def send(self, message):
        with self.out:
            sys.stdout.write(json.dumps(message) + "\n")
            sys.stdout.flush()

    # A person's request, relayed by the host: pushed to the agent if it listens to channel events, and kept for
    # wait_for_request until it is answered
    def on_request(self, message):
        request = {"request_id": str(message.get("id") or f"r{len(self.requests) + 1}"),
                   "from": str(message.get("from") or "unknown"), "text": str(message.get("text", ""))}
        if message.get("kind") == "repair":  # a change set whose template changed: answer with answer_repair
            request["kind"], request["repair"] = "repair", message.get("repair")
        with self.arrived:
            self.requests[request["request_id"]] = request
            self.arrived.notify_all()
        log(f"request {request['request_id']} from {request['from']}: {request['text']}")
        if self.ready:
            self.send({"jsonrpc": "2.0", "method": "notifications/claude/channel",
                       "params": {"content": request["text"],
                                  "meta": {"request_id": request["request_id"], "sender": request["from"]}}})

    def wait_for_request(self, timeout):
        deadline = time.monotonic() + max(1, min(int(timeout), 600))
        with self.arrived:
            while True:
                for request_id, request in self.requests.items():
                    if request_id not in self.delivered:
                        self.delivered.add(request_id)
                        return request
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    return None
                self.arrived.wait(remaining)

    def render_screen(self, args):
        """A picture of main, or of an open proposal applied, as an MCP image block and a caption."""
        reply = self.host.request({"type": "render", **{k: args[k] for k in ("proposal", "scale") if k in args}})
        if reply.get("type") != "image":
            return f"error: {reply.get('error') or reply.get('message')}", True
        shown = "main" if reply.get("proposal") is None else f"proposal {reply['proposal']} applied to main"
        caption = (f"{shown} at revision {reply.get('revision')}: {reply.get('width')}x{reply.get('height')} pixels, "
                   f"scale {reply.get('scale')}")
        return [{"type": "image", "data": reply.get("data", ""), "mimeType": "image/png"},
                {"type": "text", "text": caption}], False

    def get_evidence(self, args):
        """A proposal's evidence: both pictures as MCP image blocks, then the rest as compact JSON."""
        reply = self.host.request({"type": "getEvidence", **{k: args[k] for k in ("proposal", "scale") if k in args}})
        if reply.get("type") != "evidence":
            return f"error: {reply.get('error') or reply.get('message')}", True
        evidence = reply.get("evidence") or {}
        facts = {key: evidence.get(key) for key in ("region", "behaviour", "scenarios", "texts")}
        blocks = []
        for side in ("before", "after"):
            picture = evidence.get(side) or {}
            blocks.append({"type": "text", "text": f"{side}: {picture.get('width')}x{picture.get('height')} pixels"})
            blocks.append({"type": "image", "data": picture.get("data", ""), "mimeType": "image/png"})
        blocks.append({"type": "text", "text": compact({"proposal": reply.get("proposal"),
                                                        "revision": reply.get("revision"), **facts})})
        return blocks, False

    def checked_document(self, args):
        """The document scenarios play on: main as the host has it now, or with an open proposal applied."""
        reply = self.host.request({"type": "getDocument"})
        document, shown = reply.get("document") or {}, f"main at revision {reply.get('revision')}"
        if "proposal" in args:
            diff = self.host.request({"type": "diff", "proposal": args["proposal"]})
            if diff.get("type") != "diff":
                raise RuntimeError(f"proposal {args['proposal']}: {diff.get('error') or diff.get('message')}")
            document, shown = apply_patch(document, diff.get("patch") or []), f"proposal {args['proposal']}"
        return document, shown

    def run_scenarios(self, args):
        """Plays a scenarios file (or the agent's own scenarios, on its screen) on the running screen's document,
        with affrescoui test, and reports compactly."""
        path = str(args.get("file", ""))
        path = path if os.path.isabs(path) or os.path.exists(path) else os.path.join(ROOT, path)
        if not os.path.basename(path).endswith(".scenarios.json"):  # no other file: affrescoui test's errors
            return "error: file: expected a *.scenarios.json file", True  # would quote its keys to the agent
        with open(path, encoding="utf-8") as file:
            shipped = json.load(file)
        document, shown = self.checked_document(args)
        with tempfile.TemporaryDirectory(prefix="affrescoui-scenarios-") as scratch:
            screen = os.path.join(scratch, "screen.json")
            with open(screen, "w", encoding="utf-8") as file:
                json.dump(document, file)
            if "scenarios" in args:  # the agent's own, on the file's screen: its folder holds the fonts and includes
                own = {"format": "affrescoui-scenarios", "version": 2,
                       "screens": os.path.abspath(os.path.join(os.path.dirname(path), shipped.get("screens", "."))),
                       "scenarios": args["scenarios"], **({"screen": shipped["screen"]} if "screen" in shipped else {})}
                path = os.path.join(scratch, "own.scenarios.json")
                with open(path, "w", encoding="utf-8") as file:
                    json.dump(own, file)
            command = [find_cli(), "test", "--json", "--document", screen, path]
            command += ["--only", str(args["only"])] if args.get("only") else []
            result = subprocess.run(command, capture_output=True, text=True, timeout=300)
        if result.returncode not in (0, 1) or not result.stdout.strip():
            return f"error: {(result.stderr or result.stdout).strip() or 'affrescoui test failed'}", True
        return scenarios_report(json.loads(result.stdout.strip().splitlines()[-1]), shown), False

    def get_blame(self, args):
        """The commit on main that last changed a node or a value."""
        reply = self.host.request({"type": "blame", "target": args.get("target", "")})
        if reply.get("type") != "blame":
            return f"error: {reply.get('error') or reply.get('message')}", True
        commit = reply.get("commit") or {}
        return compact({k: commit.get(k) for k in ("number", "short", "author", "committer", "message", "time")
                        if k in commit}), False

    def get_screen(self, args):
        """The get_screen tool: screen_info(), then the document (everything, or what changed since the last read),
        one node (at) or the outline."""
        reply = self.host.request({"type": "getDocument"})
        document = dict(reply.get("document") or {})
        document["state"] = reply.get("state")  # the live state, once (the file's initial state isn't needed)
        info = screen_info(reply)
        depth = args.get("depth")
        depth = max(0, int(depth)) if isinstance(depth, (int, float)) and not isinstance(depth, bool) else None
        try:
            if args.get("outline"):
                return compact(info) + "\n" + outline(document, args.get("at"), depth), False
            if args.get("at"):
                pointer, node = locate(document, args["at"])
                address = str(args["at"]) if str(args["at"]).startswith("#") else pointer
                node = pruned(node, address, depth) if depth is not None else node
                return compact({**info, "at": pointer, "node": node}), False
        except ValueError as failure:
            return f"error: {failure}", True
        if self.seen is not None and not args.get("full"):
            changes = changes_since(self.seen, document)
            self.seen = document
            return compact({**info, "changes": changes} if changes else {**info, "unchanged": True}), False
        self.seen = document
        return compact({**info, "document": document}), False

    def get_contributions(self, args):
        """An app's contribution points, exports and installable parts, as affrescoui contributions prints them."""
        with tempfile.TemporaryDirectory(prefix="affrescoui-contributions-") as scratch:
            if args.get("file"):
                screen = repository_path(str(args["file"]))
                if not screen.endswith(".screen.json"):
                    return "error: file: expected a *.screen.json file", True
            else:
                screen = os.path.join(scratch, "running.screen.json")
                with open(screen, "w", encoding="utf-8") as file:
                    json.dump(self.host.request({"type": "getDocument"}).get("document") or {}, file)
            result = subprocess.run([find_cli(), "contributions", screen], capture_output=True, text=True, timeout=60)
        if result.returncode != 0:
            return f"error: {(result.stderr or result.stdout).strip() or 'affrescoui contributions failed'}", True
        return result.stdout.strip(), False

    def compose(self, args, install):
        """Proves a feature from another app on the running document (affrescoui check --compose), and with install
        submits the install revision the proof made. Returns the proof's reasons, never the document."""
        screen, feature = repository_path(str(args.get("screen", ""))), repository_path(str(args.get("feature", "")))
        if not screen.endswith(".screen.json"):
            return "error: screen: expected the running app's *.screen.json file", True
        reply = self.host.request({"type": "getDocument"})
        with tempfile.TemporaryDirectory(prefix="affrescoui-compose-") as scratch:
            document, revision = os.path.join(scratch, "document.json"), os.path.join(scratch, "install.json")
            with open(document, "w", encoding="utf-8") as file:
                json.dump(reply.get("document") or {}, file)
            command = [find_cli(), "check", "--compose", screen, feature, "--document", document,
                       "--base", str((reply.get("head") or {}).get("id", "")), "--revision", revision]
            if isinstance(args.get("adapter"), dict):
                adapter = os.path.join(scratch, "adapter.json")
                with open(adapter, "w", encoding="utf-8") as file:
                    json.dump(args["adapter"], file)
                command += ["--adapter", adapter]
            result = subprocess.run(command, capture_output=True, text=True, timeout=300)
            if result.returncode not in (0, 1) or not result.stdout.strip():
                return f"error: {(result.stderr or result.stdout).strip() or 'affrescoui check failed'}", True
            reasons = result.stdout.strip()
            if not install or result.returncode != 0:
                return reasons, result.returncode != 0
            with open(revision, encoding="utf-8") as file:
                submitted = json.load(file)
        submitted.pop("author", None)  # the host names the author: this connection
        text, error = describe(self.host.request({"type": "revision", **submitted}))
        return f"{reasons}\n{text}", error

    def call_tool(self, name, args):
        host = self.host
        if name == "get_blame":
            return self.get_blame(args)
        if name == "wait_for_request":
            host.ensure()  # so requests can arrive at all
            request = self.wait_for_request(args.get("timeout_seconds", 120))
            return json.dumps(request if request else {"request": None, "note": "nothing yet: call again"}), False
        if name == "reply":
            request_id = str(args.get("request_id", ""))
            host.send({"type": "reply", "request": request_id, "text": args.get("text", "")})
            with self.arrived:
                self.requests.pop(request_id, None)
                self.delivered.discard(request_id)
            return "sent to the Studio", False
        if name == "answer_repair":
            request_id = str(args.get("request_id", ""))
            reply = host.request({"type": "repair", "request": request_id,
                                  "revision": {"message": args.get("message", ""), "patch": args.get("patch", [])}})
            with self.arrived:
                self.requests.pop(request_id, None)
                self.delivered.discard(request_id)
            if reply.get("type") != "proposed":
                return f"refused, nothing changed: {reply.get('message') or reply.get('error')}", True
            return describe(reply), False
        if name == "get_screen":
            return self.get_screen(args)
        if name == "get_catalog":
            return compact(host.request({"type": "getCatalog"}).get("catalog")), False
        if name == "find_nodes":
            reply = host.request({"type": "getDocument"})
            return find_nodes(reply.get("document") or {}, args.get("type"), args.get("text"), args.get("property"),
                              args.get("value"), args.get("limit", 20)), False
        if name == "get_contributions":
            return self.get_contributions(args)
        if name in ("check_composition", "install_feature"):
            return self.compose(args, name == "install_feature")
        if name == "get_format_guide":
            with open(guide_path(), encoding="utf-8") as guide:
                part = format_guide(guide.read(), args.get("topic"))
            if part is None:
                return f"error: no topic '{args.get('topic')}': call get_format_guide without one to list them", True
            return part, False
        if name == "submit_revision":
            revision = {"type": "revision", "author": self.client, "message": args.get("message", "")}
            for key in ("patch", "set", "add", "remove", "move", "parent", "proposal"):
                if key in args:
                    revision[key] = args[key]
            return describe(host.request(revision))
        if name == "render_screen":
            return self.render_screen(args)
        if name == "run_scenarios":
            return self.run_scenarios(args)
        if name == "get_evidence":
            return self.get_evidence(args)
        if name == "get_history":
            reply = host.request({"type": "getHistory", **({"limit": args["limit"]} if "limit" in args else {})})
            return compact(reply.get("entries")), False
        if name == "get_diff":
            reply = host.request({"type": "diff", **{k: args[k] for k in ("from", "to", "proposal") if k in args}})
            if reply.get("type") != "diff":
                return f"error: {reply.get('error')}", True
            return compact(reply.get("patch")), False
        if name in ("restore_revision", "revert_revision"):
            kind = "restore" if name == "restore_revision" else "revert"
            return describe(host.request({"type": kind, "author": self.client, "revision": args.get("revision")}))
        if name == "get_events":
            return compact(host.take_events()), False
        if name == "export_changes":
            reply = host.request({"type": "exportChanges", "from": args.get("from", 0),
                                  "name": args.get("name", "changes")})
            return json.dumps(reply.get("changes"), indent=2), False
        coordinated = coordination_tool(host, self.client, name, args)
        if coordinated is not None:
            return coordinated
        raise UnknownTool(name)

    def serve(self, line):
        """One line from the client. A line Python can't read as JSON gets a parse error: bad syntax, nesting deeper
        than its recursion limit, or (3.11 and later) an integer of more digits than it converts."""
        try:
            message = json.loads(line)
        except (ValueError, RecursionError):  # json.JSONDecodeError is a ValueError
            self.send({"jsonrpc": "2.0", "id": None, "error": {"code": -32700, "message": "parse error"}})
            return
        self.handle(message)

    def handle(self, request):
        refusal = malformed(request)
        if refusal:
            self.send(refusal)
            return
        method, params = request.get("method"), request.get("params") or {}
        if "id" not in request:  # a notification
            if method == "notifications/initialized":
                self.ready = True
                try:
                    self.host.ensure()  # connect early, so requests sent before the first tool call arrive too
                except RuntimeError as failure:
                    log(str(failure))
            return
        rid = request["id"]

        def result(value):
            self.send({"jsonrpc": "2.0", "id": rid, "result": value})

        def error(code, message):
            self.send({"jsonrpc": "2.0", "id": rid, "error": {"code": code, "message": message}})

        if method == "initialize":
            self.client = client_name(params)
            requested = params.get("protocolVersion")
            result({"protocolVersion": requested if requested in PROTOCOLS else PROTOCOLS[0],
                    "capabilities": {"tools": {"listChanged": False}, "prompts": {"listChanged": False},
                                     "experimental": {"claude/channel": {}}},
                    "serverInfo": {"name": "affrescoui", "version": "0.1.0"},
                    "instructions": INSTRUCTIONS})
        elif method == "ping":
            result({})
        elif method == "tools/list":
            result({"tools": TOOLS})
        elif method == "prompts/list":
            result({"prompts": PROMPTS})
        elif method == "prompts/get":
            if params.get("name") != "serve_studio":
                error(-32602, f"unknown prompt {params.get('name')!r}")
            else:
                result({"description": PROMPTS[0]["description"],
                        "messages": [{"role": "user", "content": {"type": "text", "text": SERVE_PROMPT}}]})
        elif method == "tools/call":
            # On its own thread: wait_for_request can block for minutes while pings and other calls go on
            def call():
                try:
                    content, is_error = self.call_tool(params.get("name"), params.get("arguments") or {})
                    if isinstance(content, str):  # most tools answer in text; render_screen with an image
                        content = [{"type": "text", "text": content}]
                    result({"content": content, "isError": is_error})
                except UnknownTool:
                    error(-32602, f"unknown tool {params.get('name')!r}")
                except Exception as failure:  # no host, it went away, did not answer, ...
                    result({"content": [{"type": "text", "text": f"error: {failure}"}], "isError": True})
            threading.Thread(target=call, daemon=True).start()
        else:
            error(-32601, f"method not found: {method}")


def main():
    args = sys.argv[1:]
    socket_dir = default_socket_dir()
    launch = None
    while args:
        arg = args.pop(0)
        if arg == "--socket" and args:
            socket_dir = args.pop(0)
        elif arg == "--launch" and args:
            launch = args.pop(0)
        else:
            sys.exit(f"usage: {sys.argv[0]} [--socket DIR] [--launch \"command\"]")

    # MCP's stdio messages are UTF-8 lines ending in \n, whatever the console's code page and line ending. A byte that
    # isn't UTF-8 becomes U+FFFD, so it costs that message, not the server
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", newline="\n")
    server = Server(socket_dir, launch)
    log(f"ready; the host is expected at {server.host.path}")
    if not hasattr(socket, "AF_UNIX"):
        log(NO_UNIX_SOCKETS)
    for line in sys.stdin:
        if line.strip():
            server.serve(line)


if __name__ == "__main__":
    main()
