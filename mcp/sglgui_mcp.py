#!/usr/bin/env python3
"""sGLGUI MCP server: lets an AI agent (Claude, or any MCP client) read and change a running sGLGUI app.

    MCP client  ──MCP (JSON-RPC, stdio)──▶  this server  ──JSON-lines stream──▶  app --stdio  ──▶  sGLGUI engine
    (Claude)    ◀── tool results ────────   (Python)     ◀── replies, events ──  (any example)

The client starts this server; this server starts the app and owns its stream. Everything the agent changes goes
through the screen's permissions: with "review", the user sees a proposal in the app and accepts or rejects it there.
There are no tools to accept or reject proposals: that decision stays with the user.

Usage (the app command comes after "--"; default: ./build/chatbot_demo):
    python3 mcp/sglgui_mcp.py -- ./build/chatbot_demo
    claude mcp add sglgui -- python3 /path/to/sGLGUI/mcp/sglgui_mcp.py -- /path/to/sGLGUI/build/chatbot_demo

Only the Python standard library is needed (3.9+).
"""

import itertools
import json
import os
import subprocess
import sys
import threading

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PROTOCOLS = ["2025-11-25", "2025-06-18", "2025-03-26", "2024-11-05"]
TIMEOUT = 10  # seconds to wait for the app to answer


def log(text):
    print(f"[sglgui-mcp] {text}", file=sys.stderr, flush=True)


class App:
    """The running app, at the other end of the stream (its stdin/stdout)."""

    def __init__(self, command):
        self.process = subprocess.Popen(command + ["--stdio"], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                        text=True, bufsize=1)
        self.lock = threading.Lock()
        self.ids = itertools.count(1)
        self.waiting = {}  # request id -> [threading.Event, reply]
        self.events = []   # what happened in the app since the agent last asked
        self.hello = None
        threading.Thread(target=self.read, daemon=True).start()

    def read(self):
        for line in self.process.stdout:
            try:
                message = json.loads(line)
            except json.JSONDecodeError:
                log(f"ignoring a line from the app: {line.strip()[:120]}")
                continue

            with self.lock:
                pending = self.waiting.get(message.get("id"))
                if pending:
                    pending[1] = message
                    pending[0].set()
                elif message.get("type") == "hello":
                    self.hello = message
                elif message.get("type") == "event":
                    self.events.append({"event": message.get("name"), "data": message.get("data")})
                elif "proposal" in message:  # the user decided on a proposal
                    outcome = "accepted" if message.get("type") == "applied" else "rejected"
                    self.events.append({"proposal": message["proposal"], "outcome": outcome,
                                        "revision": message.get("revision"), "detail": message.get("error")})
                elif message.get("type") == "error":
                    self.events.append({"error": message.get("message")})
        log("the app closed its stream")

    def request(self, message):
        """Sends a message and waits for the reply carrying the same id."""
        if self.process.poll() is not None:
            raise RuntimeError("the app is not running")
        request_id = f"mcp-{next(self.ids)}"
        pending = [threading.Event(), None]
        with self.lock:
            self.waiting[request_id] = pending
        self.process.stdin.write(json.dumps({**message, "id": request_id}) + "\n")
        self.process.stdin.flush()
        if not pending[0].wait(TIMEOUT):
            raise RuntimeError("the app did not answer")
        with self.lock:
            del self.waiting[request_id]
        return pending[1]

    def take_events(self):
        with self.lock:
            events, self.events = self.events, []
        return events

    def close(self):
        try:
            self.process.stdin.close()  # the app sees the stream end and quits
            self.process.wait(5)
        except Exception:
            self.process.kill()


def object_schema(properties=None, required=None):
    schema = {"type": "object", "properties": properties or {}}
    if required:
        schema["required"] = required
    return schema


TOOLS = [
    {"name": "get_screen",
     "description": "The running app's screen: its document (sGLGUI Screen Format), live state, current revision, "
                    "pending proposals, and your author name for the permissions. Read it before changing anything.",
     "inputSchema": object_schema()},
    {"name": "get_format_guide",
     "description": "The sGLGUI Screen Format specification: screens, bindings, actions, revisions, permissions.",
     "inputSchema": object_schema()},
    {"name": "submit_revision",
     "description": "Change the running app (layout, styles, behaviour/actions, or state) with a JSON Patch "
                    "(RFC 6902). Paths may address nodes by id, e.g. \"#header/children/-\"; \"/state/...\" changes "
                    "live data. The result is: applied; proposed (the user will review it in the app: call "
                    "get_events later to learn the decision); or rejected with an error saying why (fix and retry).",
     "inputSchema": object_schema({
         "message": {"type": "string", "description": "One line, for the user, saying what the change does"},
         "patch": {"type": "array", "items": {"type": "object"}, "description": "JSON Patch operations"},
         "parent": {"type": "integer", "description": "The revision this is based on (optional; conflicts are rejected)"},
     }, ["message", "patch"])},
    {"name": "get_history",
     "description": "Every revision so far: number, author, message and time. Revision 0 is the file as loaded.",
     "inputSchema": object_schema()},
    {"name": "restore_revision",
     "description": "Bring the app back to how it was at an earlier revision. This creates a new revision (the "
                    "history keeps growing) and follows the same permissions as any change.",
     "inputSchema": object_schema({"revision": {"type": "integer"}}, ["revision"])},
    {"name": "get_events",
     "description": "What happened in the app since the last call: user events (e.g. chat messages) and the user's "
                    "decisions on your proposals. Returns them and clears the list.",
     "inputSchema": object_schema()},
    {"name": "export_changes",
     "description": "The revisions after `from` as a change set: a portable \"plugin\" file another copy of the "
                    "same app can import (--plugin).",
     "inputSchema": object_schema({"from": {"type": "integer"}, "name": {"type": "string"}})},
]


def describe(reply):
    kind = reply.get("type")
    if kind == "applied":
        return f"applied: the app is now at revision {reply.get('revision')}", False
    if kind == "proposed":
        return (f"proposed as proposal {reply.get('proposal')}: the user will review it in the app. It is not applied "
                f"yet; get_events will report the decision."), False
    return f"rejected, nothing changed: {reply.get('error')}", True


class Server:
    def __init__(self, app):
        self.app = app
        self.client = "agent"
        self.out = threading.Lock()

    def send(self, message):
        with self.out:
            sys.stdout.write(json.dumps(message) + "\n")
            sys.stdout.flush()

    def call_tool(self, name, args):
        app = self.app
        if name == "get_screen":
            reply = app.request({"type": "getDocument"})
            return json.dumps({k: reply.get(k) for k in ("revision", "you", "proposals", "state", "document")}, indent=2), False
        if name == "get_format_guide":
            with open(os.path.join(ROOT, "SCREEN_FORMAT.md"), encoding="utf-8") as guide:
                return guide.read(), False
        if name == "submit_revision":
            revision = {"type": "revision", "author": self.client, "message": args.get("message", ""),
                        "patch": args.get("patch", [])}
            if "parent" in args:
                revision["parent"] = args["parent"]
            return describe(app.request(revision))
        if name == "get_history":
            return json.dumps(app.request({"type": "getHistory"}).get("entries"), indent=2), False
        if name == "restore_revision":
            return describe(app.request({"type": "restore", "author": self.client, "revision": args.get("revision")}))
        if name == "get_events":
            return json.dumps(app.take_events(), indent=2), False
        if name == "export_changes":
            reply = app.request({"type": "exportChanges", "from": args.get("from", 0), "name": args.get("name", "changes")})
            return json.dumps(reply.get("changes"), indent=2), False
        raise KeyError(name)

    def handle(self, request):
        if "id" not in request:
            return  # a notification, e.g. notifications/initialized
        method, params, rid = request.get("method"), request.get("params") or {}, request["id"]

        def result(value):
            self.send({"jsonrpc": "2.0", "id": rid, "result": value})

        def error(code, message):
            self.send({"jsonrpc": "2.0", "id": rid, "error": {"code": code, "message": message}})

        if method == "initialize":
            self.client = (params.get("clientInfo") or {}).get("name", "agent")
            requested = params.get("protocolVersion")
            result({"protocolVersion": requested if requested in PROTOCOLS else PROTOCOLS[0],
                    "capabilities": {"tools": {"listChanged": False}},
                    "serverInfo": {"name": "sglgui", "version": "0.2.0"},
                    "instructions": "This app's whole UI is a live sGLGUI screen. Read it with get_screen (and "
                                    "get_format_guide), change it with submit_revision. Changes may need the user's "
                                    "approval and are kept in a history you can read and restore."})
        elif method == "ping":
            result({})
        elif method == "tools/list":
            result({"tools": TOOLS})
        elif method == "tools/call":
            try:
                text, is_error = self.call_tool(params.get("name"), params.get("arguments") or {})
                result({"content": [{"type": "text", "text": text}], "isError": is_error})
            except KeyError:
                error(-32602, f"unknown tool {params.get('name')!r}")
            except Exception as failure:  # the app went away, did not answer, ...
                result({"content": [{"type": "text", "text": f"error: {failure}"}], "isError": True})
        else:
            error(-32601, f"method not found: {method}")


def main():
    command = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [os.path.join(ROOT, "build", "chatbot_demo")]
    app = App(command)
    server = Server(app)
    log(f"started {' '.join(command)} --stdio")
    try:
        for line in sys.stdin:
            if line.strip():
                try:
                    server.handle(json.loads(line))
                except json.JSONDecodeError:
                    server.send({"jsonrpc": "2.0", "id": None, "error": {"code": -32700, "message": "parse error"}})
    finally:
        app.close()


if __name__ == "__main__":
    main()
