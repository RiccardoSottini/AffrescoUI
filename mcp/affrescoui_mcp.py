#!/usr/bin/env python3
"""AffrescoUI MCP server: connects an AI agent to a running AffrescoUI host, in both directions.

    requests   affrescoui_studio ──"request"──▶ affrescoui_host ──agent.sock──▶ this server ──channel event──▶ AI agent
                                                                                    (or wait_for_request)
    changes    AI agent ──tools (submit_revision, reply, ...)──▶ this server ──agent.sock──▶ affrescoui_host ──▶ screen

The host (examples/host.cpp) renders a screen and doesn't care where changes come from. A person asks for a change in
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
import socket
import stat
import subprocess
import sys
import threading
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PROTOCOLS = ["2025-11-25", "2025-06-18", "2025-03-26", "2024-11-05"]
TIMEOUT = 10  # seconds to wait for the host to answer


def log(text):
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
            raise RuntimeError("this Python has no Unix domain sockets (socket.AF_UNIX), which the host listens on: "
                               "on Windows no released CPython has them yet, see mcp/README.md")
        if not os.path.exists(self.path) and self.launch:
            log(f"no host running: starting {self.launch}")
            # Windows parses a command line itself, and shlex would drop the backslashes of its paths
            command = self.launch if os.name == "nt" else shlex.split(self.launch)
            subprocess.Popen(command, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, start_new_session=True)
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
        for line in sock.makefile("r", encoding="utf-8"):
            try:
                message = json.loads(line)
            except json.JSONDecodeError:
                continue
            kind = message.get("type")
            with self.lock:
                pending = self.waiting.get(message.get("id"))
                if pending and kind != "request":
                    pending[1] = message
                    pending[0].set()
                    continue
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
                rules = (message.get("mode"), [g.get("rule") for g in message.get("grants", [])])
                if rules != self.rules:
                    self.rules = rules
                    self.note({"mode": rules[0], "grants": rules[1]})
            elif kind == "error":
                self.note({"error": message.get("message")})
        with self.lock:
            if self.sock is sock:
                self.sock = None
        log("the host closed the connection")

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


def outline(document):
    """The node tree, one line per node: its address (#id, or a path from the nearest id), type and what it shows.
    Enough to choose where to patch, without the styling."""
    lines = []

    def label(node):
        kind = node.get("type") or ("use " + node["use"] if "use" in node else "panel")
        bits = [kind]
        for key in ("text", "placeholder", "bind", "style"):
            if key in node and isinstance(node[key], (str, int, float)):
                text = str(node[key])
                bits.append(f"{key}={text[:40] + '...' if len(text) > 40 else text}")
        if "repeat" in node:
            bits.append(f"repeat {node['repeat'].get('for', 'item')} in {node['repeat'].get('in')}")
        if "when" in node:
            bits.append("when")
        return " ".join(bits)

    def walk(node, address, depth):
        if not isinstance(node, dict):
            return
        if node.get("id"):
            address = "#" + node["id"]
        lines.append("  " * depth + f"{address}  {label(node)}")
        for i, child in enumerate(node.get("children") or []):
            child_address = "#" + child["id"] if isinstance(child, dict) and child.get("id") else f"{address}/children/{i}"
            walk(child, child_address, depth + 1)

    walk(document.get("root"), "/root", 0)
    for section in ("screens", "templates"):
        for name, node in (document.get(section) or {}).items():
            walk(node, f"/{section}/{name}", 0)
    for name, component in (document.get("components") or {}).items():
        walk((component or {}).get("node"), f"/components/{name}/node", 0)
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
    {"name": "get_screen",
     "description": "The running screen: its document (AffrescoUI Screen Format, with the live state as its \"state\" "
                    "and its permission rules), current revision and head commit, open proposals, the mode, the "
                    "reviewer's \"don't ask again\" grants, and your author name. The first call returns everything; "
                    "later calls return only what changed since your last read, as a JSON Patch (\"changes\"), or "
                    "\"unchanged\". Pass full=true to get everything again (e.g. after your context was cleared), "
                    "at=\"#id\" for one node only, or outline=true for the node tree with addresses and no styling.",
     "inputSchema": object_schema({"full": {"type": "boolean", "description": "Everything, not what changed"},
                                   "at": {"type": "string", "description": "Only this node: \"#id\" or a JSON Pointer"},
                                   "outline": {"type": "boolean",
                                               "description": "The node tree: one line per node (address, type, text)"}})},
    {"name": "get_catalog",
     "description": "What this screen's format offers: node types and their properties, events, steps, expression "
                    "functions, variables, sections, and the app's registered custom types and native functions.",
     "inputSchema": object_schema()},
    {"name": "get_format_guide",
     "description": "The AffrescoUI Screen Format specification: nodes, layout, bindings, actions, revisions, permissions.",
     "inputSchema": object_schema()},
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

PROMPTS = [
    {"name": "serve_studio",
     "description": "Work through the change requests people send from the AffrescoUI Studio, one after another."},
]

SERVE_PROMPT = (
    "You are the builder behind a live AffrescoUI app. People ask for changes in the AffrescoUI Studio. Repeat: call "
    "wait_for_request; for each request, read the screen with get_screen (after the first read it returns only what "
    "changed; get_format_guide the first time), make "
    "the change with submit_revision (fix and retry if it is rejected), then tell the person what you did with "
    "reply. Keep changes focused on what was asked, and keep the existing style (colors, fonts, spacing). If a "
    "follow-up request refines a change that is still waiting for review, push to that proposal instead of opening "
    "a new one.")

INSTRUCTIONS = (
    "This server connects you to a running AffrescoUI app (a native C++/OpenGL UI described by a JSON screen document). "
    "People ask for changes from a separate client, the AffrescoUI Studio. Their requests arrive as "
    "<channel source=\"affrescoui\" request_id=\"...\" sender=\"...\"> events (if this session loaded the channel), or "
    "from the wait_for_request tool. For each request: read the screen with get_screen (get_format_guide explains "
    "the format), change it with submit_revision, then answer with reply, passing the request_id. Your changes may "
    "wait for a reviewer's approval in the Studio; get_events tells you the decision.")


def describe(reply):
    kind = reply.get("type")
    if kind == "applied":
        return (f"applied: the screen is now at revision {reply.get('revision')} "
                f"(commit {str(reply.get('commit', ''))[:7]})"), False
    if kind == "proposed":
        return (f"proposed as proposal {reply.get('proposal')} (head {str(reply.get('commit', ''))[:7]}): a reviewer "
                f"decides in the Studio. It is not applied yet; get_events will report the decision. Pass "
                f"proposal={reply.get('proposal')} to submit_revision to add to it."), False
    return f"rejected, nothing changed: {reply.get('error')}", True


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
                   "from": message.get("from", "unknown"), "text": str(message.get("text", ""))}
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

    def call_tool(self, name, args):
        host = self.host
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
        if name == "get_screen":
            reply = host.request({"type": "getDocument"})
            document = dict(reply.get("document") or {})
            document["state"] = reply.get("state")  # the live state, once (the file's initial state isn't needed)
            head = reply.get("head") or {}
            info = {"revision": reply.get("revision"), "head": head.get("short"), "you": reply.get("you"),
                    "canReview": reply.get("canReview"), "mode": reply.get("mode"),
                    "grants": [g.get("rule") for g in reply.get("grants") or []],
                    "proposals": [{k: p.get(k) for k in ("id", "author", "message", "touches", "changes")}
                                  for p in reply.get("proposals") or []]}
            if args.get("outline"):
                return compact(info) + "\n" + outline(document), False
            if args.get("at"):
                at = str(args["at"])
                found = find_node(document, at[1:]) if at.startswith("#") else None
                if found:
                    return compact({**info, "at": found[0], "node": found[1]}), False
                node = document
                try:
                    for part in [p for p in at.split("/") if p]:
                        node = node[int(part)] if isinstance(node, list) else node[part.replace("~1", "/").replace("~0", "~")]
                except (KeyError, IndexError, ValueError, TypeError):
                    return f"error: nothing at {at}", True
                return compact({**info, "at": at, "node": node}), False
            if self.seen is not None and not args.get("full"):
                changes = diff(self.seen, document)
                for op in changes:  # addressed by node ids where there are some, as edits are written
                    if op["path"].startswith(("/root", "/screens", "/templates", "/components")):
                        op["path"] = by_id(document if op["op"] != "remove" else self.seen, op["path"])
                self.seen = document
                return compact({**info, "changes": changes} if changes else {**info, "unchanged": True}), False
            self.seen = document
            return compact({**info, "document": document}), False
        if name == "get_catalog":
            return compact(host.request({"type": "getCatalog"}).get("catalog")), False
        if name == "get_format_guide":
            with open(os.path.join(ROOT, "SCREEN_FORMAT.md"), encoding="utf-8") as guide:
                return guide.read(), False
        if name == "submit_revision":
            revision = {"type": "revision", "author": self.client, "message": args.get("message", "")}
            for key in ("patch", "set", "add", "remove", "move", "parent", "proposal"):
                if key in args:
                    revision[key] = args[key]
            return describe(host.request(revision))
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
        raise UnknownTool(name)

    def handle(self, request):
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
            self.client = (params.get("clientInfo") or {}).get("name", "agent")
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
                    text, is_error = self.call_tool(params.get("name"), params.get("arguments") or {})
                    result({"content": [{"type": "text", "text": text}], "isError": is_error})
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

    # MCP's stdio messages are UTF-8 lines ending in \n, whatever the console's code page and line ending
    sys.stdin.reconfigure(encoding="utf-8")
    sys.stdout.reconfigure(encoding="utf-8", newline="\n")
    server = Server(socket_dir, launch)
    log(f"ready; the host is expected at {server.host.path}")
    if not hasattr(socket, "AF_UNIX"):
        log("this Python has no Unix domain sockets (socket.AF_UNIX), so the tools can't reach the host: "
            "see mcp/README.md")
    for line in sys.stdin:
        if line.strip():
            try:
                server.handle(json.loads(line))
            except json.JSONDecodeError:
                server.send({"jsonrpc": "2.0", "id": None, "error": {"code": -32700, "message": "parse error"}})


if __name__ == "__main__":
    main()
