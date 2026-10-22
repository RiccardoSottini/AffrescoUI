"""The MCP server: JSON-RPC over stdio with the client, each tool call answered from the host or the affrescoui tool."""

import collections
import json
import os
import socket
import subprocess
import sys
import tempfile
import threading
import time

from .document import apply_patch, changes_since, compact, find_nodes, locate, outline, pruned
from .guide import format_guide
from .host import NO_UNIX_SOCKETS, Host, default_socket_dir, log
from .replies import client_name, describe, malformed, scenarios_report, screen_info
from .repository import ROOT, find_cli, guide_path, repository_path
from .tools import INSTRUCTIONS, PROMPTS, SERVE_PROMPT, TOOLS

PROTOCOLS = ["2025-11-25", "2025-06-18", "2025-03-26", "2024-11-05"]


class UnknownTool(Exception):
    pass


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
        screen = repository_path(str(args.get("file", "")))
        if not screen.endswith(".screen.json"):
            return "error: file: expected a *.screen.json file", True
        with tempfile.TemporaryDirectory(prefix="affrescoui-contributions-") as scratch:
            command = [find_cli(), "contributions", screen]
            if args.get("running"):
                document = os.path.join(scratch, "running.json")
                with open(document, "w", encoding="utf-8") as file:
                    json.dump(self.host.request({"type": "getDocument"}).get("document") or {}, file)
                command += ["--document", document]
            result = subprocess.run(command, capture_output=True, text=True, timeout=60)
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
