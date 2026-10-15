#!/usr/bin/env python3
"""sGLGUI MCP server: connects an AI agent to a running sGLGUI host, in both directions.

    requests   sglgui_studio ──"request"──▶ sglgui_host ──agent.sock──▶ this server ──channel event──▶ AI agent
                                                                                    (or wait_for_request)
    changes    AI agent ──tools (submit_revision, reply, ...)──▶ this server ──agent.sock──▶ sglgui_host ──▶ screen

The host (examples/host.cpp) renders a screen and doesn't care where changes come from. A person asks for a change in
plain words in the Studio; the request reaches the agent through this server; the agent reads the screen, sends
revisions (JSON Patch), and replies. Revisions follow the screen's permissions: the dashboard template sends the
agent's changes to the Studio for review, unless auto-accept is on. There is no tool to accept or reject: that is
the reviewer's decision.

Requests reach the agent two ways:
  - pushed, as Claude Code channel events (research preview: start Claude Code with
    `--dangerously-load-development-channels server:sglgui`);
  - pulled, with the wait_for_request tool, which works with any MCP client.

Usage:
    python3 mcp/sglgui_mcp.py [--socket DIR] [--launch "command"]
    claude mcp add sglgui -- python3 /path/to/sGLGUI/mcp/sglgui_mcp.py

DIR is where the host listens (default /tmp/sglgui-<uid>, as for the host). With --launch, the command is started
if no host is running. Only the Python standard library is needed (3.9+).
"""

import collections
import itertools
import json
import os
import shlex
import socket
import subprocess
import sys
import threading
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PROTOCOLS = ["2025-11-25", "2025-06-18", "2025-03-26", "2024-11-05"]
TIMEOUT = 10  # seconds to wait for the host to answer


def log(text):
    print(f"[sglgui-mcp] {text}", file=sys.stderr, flush=True)


class Host:
    """The running sglgui_host, at the other end of agent.sock. Connects (again) when needed."""

    def __init__(self, socket_dir, launch, on_request):
        self.path = os.path.join(socket_dir, "agent.sock")
        self.launch = launch
        self.on_request = on_request
        self.sock = None
        self.lock = threading.Lock()
        self.ids = itertools.count(1)
        self.waiting = {}  # request id -> [threading.Event, reply]
        self.events = []   # what happened since the agent last asked

    def ensure(self):
        with self.lock:
            if self.sock:
                return
        if not os.path.exists(self.path) and self.launch:
            log(f"no host running: starting {self.launch}")
            subprocess.Popen(shlex.split(self.launch), stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                             start_new_session=True)
            for _ in range(50):
                if os.path.exists(self.path):
                    break
                time.sleep(0.1)
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        try:
            sock.connect(self.path)
        except OSError:
            sock.close()
            raise RuntimeError(f"no sGLGUI host is listening at {self.path}. Start one: ./build/sglgui_host")
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
                           "outcome": "accepted" if kind == "applied" else "rejected",
                           "revision": message.get("revision"), "detail": message.get("error")})
            elif kind == "committed":
                self.note({"committed": message.get("revision"), "author": message.get("author"),
                           "message": message.get("message")})
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


def object_schema(properties=None, required=None):
    schema = {"type": "object", "properties": properties or {}}
    if required:
        schema["required"] = required
    return schema


TOOLS = [
    {"name": "wait_for_request",
     "description": "Wait for the next change request a person sends from the sGLGUI Studio, and return it "
                    "(request_id, from, text). Returns right away if some are already waiting; returns "
                    "{\"request\": null} after the timeout. Not needed when requests arrive as <channel> events.",
     "inputSchema": object_schema({"timeout_seconds": {"type": "integer", "description": "Default 120, at most 600"}})},
    {"name": "reply",
     "description": "Answer a request in the Studio: say what you changed, or ask a question. Call it once you "
                    "have submitted the revisions for a request.",
     "inputSchema": object_schema({"request_id": {"type": "string"}, "text": {"type": "string"}},
                                  ["request_id", "text"])},
    {"name": "get_screen",
     "description": "The running screen: its document (sGLGUI Screen Format), live state, current revision, pending "
                    "proposals, auto-accept, and your author name for the permissions. Read it before changing "
                    "anything.",
     "inputSchema": object_schema()},
    {"name": "get_format_guide",
     "description": "The sGLGUI Screen Format specification: nodes, layout, bindings, actions, revisions, permissions.",
     "inputSchema": object_schema()},
    {"name": "submit_revision",
     "description": "Change the running screen (layout, styles, colors, behaviour/actions, or state) with a JSON "
                    "Patch (RFC 6902). Paths may address nodes by id, e.g. \"#header/children/-\"; \"/state/...\" "
                    "changes live data. The result is: applied; proposed (a reviewer decides in the Studio: "
                    "get_events reports the decision); or rejected with an error saying why (fix it and retry).",
     "inputSchema": object_schema({
         "message": {"type": "string", "description": "One line, for the reviewer, saying what the change does"},
         "patch": {"type": "array", "items": {"type": "object"}, "description": "JSON Patch operations"},
         "parent": {"type": "integer", "description": "The revision this is based on (optional; conflicts are rejected)"},
     }, ["message", "patch"])},
    {"name": "get_history",
     "description": "Every revision so far: number, author, message and time. Revision 0 is the file as loaded.",
     "inputSchema": object_schema()},
    {"name": "restore_revision",
     "description": "Bring the screen back to how it was at an earlier revision. This creates a new revision (the "
                    "history keeps growing) and follows the same permissions as any change.",
     "inputSchema": object_schema({"revision": {"type": "integer"}}, ["revision"])},
    {"name": "get_events",
     "description": "What happened since the last call: reviewers' decisions on your proposals, commits by anyone, "
                    "events the screen emitted, errors. Returns them and clears the list.",
     "inputSchema": object_schema()},
    {"name": "export_changes",
     "description": "The revisions after `from` as a change set: a portable \"plugin\" file another copy of the "
                    "same screen can import (sglgui_host --plugin).",
     "inputSchema": object_schema({"from": {"type": "integer"}, "name": {"type": "string"}})},
]

PROMPTS = [
    {"name": "serve_studio",
     "description": "Work through the change requests people send from the sGLGUI Studio, one after another."},
]

SERVE_PROMPT = (
    "You are the builder behind a live sGLGUI app. People ask for changes in the sGLGUI Studio. Repeat: call "
    "wait_for_request; for each request, read the screen with get_screen (and get_format_guide the first time), make "
    "the change with submit_revision (fix and retry if it is rejected), then tell the person what you did with "
    "reply. Keep changes focused on what was asked, and keep the existing style (colors, fonts, spacing).")

INSTRUCTIONS = (
    "This server connects you to a running sGLGUI app (a native C++/OpenGL UI described by a JSON screen document). "
    "People ask for changes from a separate client, the sGLGUI Studio. Their requests arrive as "
    "<channel source=\"sglgui\" request_id=\"...\" sender=\"...\"> events (if this session loaded the channel), or "
    "from the wait_for_request tool. For each request: read the screen with get_screen (get_format_guide explains "
    "the format), change it with submit_revision, then answer with reply, passing the request_id. Your changes may "
    "wait for a reviewer's approval in the Studio; get_events tells you the decision.")


def describe(reply):
    kind = reply.get("type")
    if kind == "applied":
        return f"applied: the screen is now at revision {reply.get('revision')}", False
    if kind == "proposed":
        return (f"proposed as proposal {reply.get('proposal')}: a reviewer decides in the Studio. It is not applied "
                f"yet; get_events will report the decision."), False
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
            keys = ("revision", "you", "canReview", "autoAccept", "proposals", "state", "document")
            return json.dumps({k: reply.get(k) for k in keys}, indent=2), False
        if name == "get_format_guide":
            with open(os.path.join(ROOT, "SCREEN_FORMAT.md"), encoding="utf-8") as guide:
                return guide.read(), False
        if name == "submit_revision":
            revision = {"type": "revision", "author": self.client, "message": args.get("message", ""),
                        "patch": args.get("patch", [])}
            if "parent" in args:
                revision["parent"] = args["parent"]
            return describe(host.request(revision))
        if name == "get_history":
            return json.dumps(host.request({"type": "getHistory"}).get("entries"), indent=2), False
        if name == "restore_revision":
            return describe(host.request({"type": "restore", "author": self.client, "revision": args.get("revision")}))
        if name == "get_events":
            return json.dumps(host.take_events(), indent=2), False
        if name == "export_changes":
            reply = host.request({"type": "exportChanges", "from": args.get("from", 0),
                                  "name": args.get("name", "changes")})
            return json.dumps(reply.get("changes"), indent=2), False
        raise KeyError(name)

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
                    "serverInfo": {"name": "sglgui", "version": "0.3.0"},
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
                except KeyError:
                    error(-32602, f"unknown tool {params.get('name')!r}")
                except Exception as failure:  # no host, it went away, did not answer, ...
                    result({"content": [{"type": "text", "text": f"error: {failure}"}], "isError": True})
            threading.Thread(target=call, daemon=True).start()
        else:
            error(-32601, f"method not found: {method}")


def main():
    args = sys.argv[1:]
    socket_dir = os.path.join("/tmp", f"sglgui-{os.getuid()}")
    launch = None
    while args:
        arg = args.pop(0)
        if arg == "--socket" and args:
            socket_dir = args.pop(0)
        elif arg == "--launch" and args:
            launch = args.pop(0)
        else:
            sys.exit(f"usage: {sys.argv[0]} [--socket DIR] [--launch \"command\"]")

    server = Server(socket_dir, launch)
    log(f"ready; the host is expected at {server.host.path}")
    for line in sys.stdin:
        if line.strip():
            try:
                server.handle(json.loads(line))
            except json.JSONDecodeError:
                server.send({"jsonrpc": "2.0", "id": None, "error": {"code": -32700, "message": "parse error"}})


if __name__ == "__main__":
    main()
