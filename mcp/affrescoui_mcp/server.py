"""The MCP server: JSON-RPC over stdio with the client, each tool call answered from the host or the affrescoui tool."""

import collections
import itertools
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
from .replies import client_name, describe, host_failure, malformed, scenarios_report, screen_info
from .repository import ROOT, confined_path, find_cli, guide_path
from .tools import INSTRUCTIONS, PROMPTS, SERVE_PROMPT, TOOLS

OUTSIDE = "error: {}: must be inside the repository, the working folder or a --root folder"

PROTOCOLS = ["2025-11-25", "2025-06-18", "2025-03-26", "2024-11-05"]


def attachments_of(value):
    """The attachments a request carries, as the host sent them (metadata, never bytes): at most 32, each with only
    the fields an attachment has, so a request can't smuggle anything else to the agent."""
    if not isinstance(value, list):
        return []
    keys = ("id", "name", "type", "size", "duration", "width", "height")
    return [{key: item[key] for key in keys if key in item and isinstance(item[key], (str, int, float))}
            for item in value[:32] if isinstance(item, dict) and isinstance(item.get("id"), str)]


class UnknownTool(Exception):
    """A tools/call names no tool: the client gets an "unknown tool" error rather than a tool's answer."""


def cli_failure(result, command):
    """The answer when the affrescoui tool failed: what it printed, else that it failed.

    @param result   its subprocess.CompletedProcess
    @param command  its command, e.g. "test"
    @return (text, True)
    """
    return f"error: {(result.stderr or result.stdout).strip() or f'affrescoui {command} failed'}", True


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
    """The MCP server: one client on stdio, one host on agent.sock. Tool calls run on their own threads, so what
    they share (the requests, the output) is locked."""

    def __init__(self, socket_dir, launch, roots=()):
        """A server whose host listens in `socket_dir`; `launch` (a program and its arguments, or None) starts one if
        none runs. Tools read files only inside the repository, the working folder and `roots`."""
        self.client = "agent"
        self.roots = [ROOT, os.getcwd(), *roots]
        self.out = threading.Lock()
        self.ready = False  # the client said notifications/initialized: channel events may be sent
        self.requests = collections.OrderedDict()  # request_id -> request, until answered
        self.delivered = set()  # given out by wait_for_request
        self.unnamed = itertools.count(1)  # numbers the requests that come without an id: r1, r2, ...
        self.arrived = threading.Condition()
        self.host = Host(socket_dir, launch, self.on_request)
        self.seen = None  # the document (with the live state) the agent last read, for get_screen's changes

    def send(self, message):
        """Sends one JSON-RPC message to the client, one line, whole even when several threads answer at once."""
        with self.out:
            sys.stdout.write(json.dumps(message) + "\n")
            sys.stdout.flush()

    def on_request(self, message):
        """A person's request, relayed by the host: pushed to the agent if it listens to channel events, and kept for
        wait_for_request until it is answered. One without an id gets a number no other had: one counted from the
        requests kept would be taken again once one of them was answered, replacing a request not answered yet."""
        if message.get("type") == "cancel":
            self.on_cancel(message)
            return
        # Only the screen itself says "screen": the host drops the field from what its peers relay
        request = {"request_id": str(message.get("id") or f"r{next(self.unnamed)}"),
                   "from": str(message.get("from") or "unknown"), "text": str(message.get("text", "")),
                   "origin": "app" if message.get("source") == "screen" else "studio"}
        if isinstance(message.get("options"), dict):  # what the chat asks with it, e.g. {"model": "sonnet"}
            request["options"] = message["options"]
        attachments = attachments_of(message.get("attachments"))
        if attachments:  # files the person attached: metadata only; name one in submit_revision's assets
            request["attachments"] = attachments
        if message.get("kind") == "repair":  # a change set whose template changed: answer with answer_repair
            request["kind"], request["repair"] = "repair", message.get("repair")
        with self.arrived:
            self.requests[request["request_id"]] = request
            self.arrived.notify_all()
        log(f"request {request['request_id']} from {request['from']} ({request['origin']}): {request['text']}")
        if self.ready:  # the channel tag has a "source" of its own: the server's name
            meta = {"request_id": request["request_id"], "sender": request["from"], "origin": request["origin"]}
            if isinstance(request.get("options", {}).get("model"), str):  # the model the chat chose
                meta["model"] = request["options"]["model"]
            content = request["text"]
            if attachments:
                meta["attachments"] = json.dumps(attachments, separators=(",", ":"))
                content += "\n\nAttached: " + "; ".join(
                    f"{a.get('name')} ({a.get('type')}, {a.get('size')} bytes, attachment {a.get('id')})"
                    for a in attachments)
            self.send({"jsonrpc": "2.0", "method": "notifications/claude/channel",
                       "params": {"content": content, "meta": meta}})
            self.working(request)

    def on_cancel(self, message):
        """The person cancelled a request from inside the app: wait_for_request no longer gives it out, the app drops
        a reply to it, and an agent listening to channel events is told to stop working on it."""
        request_id = message.get("request")
        if not isinstance(request_id, str) or not request_id:
            return
        with self.arrived:
            self.requests.pop(request_id, None)
        log(f"request {request_id} cancelled by the person")
        if self.ready:
            self.send({"jsonrpc": "2.0", "method": "notifications/claude/channel",
                       "params": {"content": f"The person cancelled request {request_id}: stop working on it and "
                                             "don't reply to it.",
                                  "meta": {"request_id": request_id, "sender": "user", "origin": "app",
                                           "cancelled": "true"}}})

    def working(self, request):
        """Tells the app that a request asked from inside it reached the agent: its chat shows the agent working
        until the reply. A client on studio.sock shows its requests' progress itself, so nothing is sent for those."""
        if request["origin"] != "app":
            return
        try:
            self.host.send({"type": "reply", "request": request["request_id"], "done": False})
        except (OSError, RuntimeError) as failure:  # the reply will say it again, or the app is gone
            log(f"couldn't tell the app request {request['request_id']} arrived: {failure}")

    def wait_for_request(self, timeout):
        """The first request not given out yet, waiting up to `timeout` seconds (1 to 600) for one; None if none came.
        One asked from inside the app is told the agent has it (working), unless the channel told it already."""
        deadline = time.monotonic() + max(1, min(int(timeout), 600))
        with self.arrived:
            request = self.undelivered()
            while request is None:
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    return None
                self.arrived.wait(remaining)
                request = self.undelivered()
        if not self.ready:
            self.working(request)
        return request

    def undelivered(self):
        """The first request not given out yet, now marked given out; None if there is none. Called with
        self.arrived held."""
        for request_id, request in self.requests.items():
            if request_id not in self.delivered:
                self.delivered.add(request_id)
                return request
        return None

    def render_screen(self, args):
        """A picture of main, or of an open proposal applied, as an MCP image block and a caption."""
        reply = self.host.request({"type": "render", **{k: args[k] for k in ("proposal", "scale") if k in args}})
        if reply.get("type") != "image":
            return host_failure(reply)
        shown = "main" if reply.get("proposal") is None else f"proposal {reply['proposal']} applied to main"
        caption = (f"{shown} at revision {reply.get('revision')}: {reply.get('width')}x{reply.get('height')} pixels, "
                   f"scale {reply.get('scale')}")
        return [{"type": "image", "data": reply.get("data", ""), "mimeType": "image/png"},
                {"type": "text", "text": caption}], False

    def get_evidence(self, args):
        """A proposal's evidence: both pictures as MCP image blocks, then the rest as compact JSON."""
        reply = self.host.request({"type": "getEvidence", **{k: args[k] for k in ("proposal", "scale") if k in args}})
        if reply.get("type") != "evidence":
            return host_failure(reply)
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
        path = confined_path(str(args.get("file", "")), self.roots)
        if path is None:
            return OUTSIDE.format("file"), True
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
            return cli_failure(result, "test")
        return scenarios_report(json.loads(result.stdout.strip().splitlines()[-1]), shown), False

    def get_blame(self, args):
        """The commit on main that last changed a node or a value."""
        reply = self.host.request({"type": "blame", "target": args.get("target", "")})
        if reply.get("type") != "blame":
            return host_failure(reply)
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
        lisp = args.get("notation") == "lisp"
        try:
            if args.get("outline"):
                return compact(info) + "\n" + outline(document, args.get("at"), depth), False
            if args.get("at"):
                pointer, node = locate(document, args["at"])
                address = str(args["at"]) if str(args["at"]).startswith("#") else pointer
                node = pruned(node, address, depth) if depth is not None else node
                if lisp:
                    return self.in_lisp({**info, "at": pointer}, node)
                return compact({**info, "at": pointer, "node": node}), False
        except ValueError as failure:
            return f"error: {failure}", True
        if self.seen is not None and not args.get("full"):
            changes = changes_since(self.seen, document)
            self.seen = document
            return compact({**info, "changes": changes} if changes else {**info, "unchanged": True}), False
        self.seen = document
        if lisp:
            return self.in_lisp(info, document)
        return compact({**info, "document": document}), False

    def in_lisp(self, info, value):
        """A read in the Lisp notation: the facts as JSON on the first line, then the value as Lisp text, written by
        the host (one implementation of the notation, the library's)."""
        reply = self.host.request({"type": "convert", "notation": "lisp", "value": value})
        if reply.get("type") != "converted":
            return host_failure(reply)
        return compact(info) + "\n" + str(reply.get("text", "")), False

    def get_contributions(self, args):
        """An app's contribution points, exports and installable parts, as affrescoui contributions prints them."""
        screen = confined_path(str(args.get("file", "")), self.roots)
        if screen is None:
            return OUTSIDE.format("file"), True
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
            return cli_failure(result, "contributions")
        return result.stdout.strip(), False

    def compose(self, args, install):
        """Proves a feature from another app on the running document (affrescoui check --compose), and with install
        submits the install revision the proof made. Returns the proof's reasons, never the document."""
        screen, feature = (confined_path(str(args.get(key, "")), self.roots) for key in ("screen", "feature"))
        if screen is None or feature is None:
            return OUTSIDE.format("screen" if screen is None else "feature"), True
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
                return cli_failure(result, "check")
            reasons = result.stdout.strip()
            if not install or result.returncode != 0:
                return reasons, result.returncode != 0
            with open(revision, encoding="utf-8") as file:
                submitted = json.load(file)
        submitted.pop("author", None)  # the host names the author: this connection
        text, error = describe(self.host.request({"type": "revision", **submitted}))
        return f"{reasons}\n{text}", error

    def next_request(self, args):
        """The wait_for_request tool: the next request no call has given out yet, waiting for one up to a timeout."""
        self.host.ensure()  # so requests can arrive at all
        request = self.wait_for_request(args.get("timeout_seconds", 120))
        return json.dumps(request if request else {"request": None, "note": "nothing yet: call again"}), False

    def forget(self, request_id):
        """Drops an answered request, so wait_for_request doesn't give it out again."""
        with self.arrived:
            self.requests.pop(request_id, None)
            self.delivered.discard(request_id)

    def reply(self, args):
        """The reply tool: the agent's answer to a request, shown where it was asked (the app's chat or the client).
        One with done=false is a progress note: the request stays open for the answer that follows."""
        request_id = str(args.get("request_id", ""))
        done = args.get("done") is not False
        with self.arrived:
            asked = self.requests.get(request_id) or {}
        self.host.send({"type": "reply", "request": request_id, "text": str(args.get("text", "")), "done": done})
        if done:
            self.forget(request_id)
        return f"sent to {'the app' if asked.get('origin') == 'app' else 'the client'}", False

    def answer_repair(self, args):
        """The answer_repair tool: the repaired change set, proposed for review in answer to a repair request."""
        request_id = str(args.get("request_id", ""))
        reply = self.host.request({"type": "repair", "request": request_id,
                                   "revision": {"message": args.get("message", ""), "patch": args.get("patch", [])}})
        self.forget(request_id)
        if reply.get("type") != "proposed":
            return f"refused, nothing changed: {reply.get('message') or reply.get('error')}", True
        return describe(reply), False

    def get_catalog(self, args):
        """The get_catalog tool: what the screen's format and the app offer."""
        return compact(self.host.request({"type": "getCatalog"}).get("catalog")), False

    def search_nodes(self, args):
        """The find_nodes tool: the nodes of the running document that match, one outline line each."""
        reply = self.host.request({"type": "getDocument"})
        return find_nodes(reply.get("document") or {}, args.get("type"), args.get("text"), args.get("property"),
                          args.get("value"), args.get("limit", 20)), False

    def get_format_guide(self, args):
        """The get_format_guide tool: the guide's contents, or one topic of it."""
        with open(guide_path(), encoding="utf-8") as guide:
            part = format_guide(guide.read(), args.get("topic"))
        if part is None:
            return f"error: no topic '{args.get('topic')}': call get_format_guide without one to list them", True
        return part, False

    def submit_revision(self, args):
        """The submit_revision tool: a change, authored by this connection, applied or proposed as the rules say. With
        notation "lisp", the change is the Lisp text in `revision`, which the host reads into the same JSON first."""
        fields = args
        if args.get("notation") == "lisp":
            reply = self.host.request({"type": "convert", "notation": "lisp", "text": str(args.get("revision", ""))})
            if reply.get("type") != "converted":
                return host_failure(reply)
            fields = reply.get("value")
            if not isinstance(fields, dict):
                return "error: revision: expected an object, e.g. {:set {:\"#title/text\" Hello}}", True
        revision = {"type": "revision", "author": self.client, "message": args.get("message", "")}
        for key in ("patch", "set", "add", "remove", "move", "parent", "proposal", "assets", "run"):
            if key in fields:
                revision[key] = fields[key]
        return describe(self.host.request(revision))

    def get_history(self, args):
        """The get_history tool: main's commits."""
        reply = self.host.request({"type": "getHistory", **({"limit": args["limit"]} if "limit" in args else {})})
        return compact(reply.get("entries")), False

    def get_diff(self, args):
        """The get_diff tool: a JSON Patch between two revisions, or what an open proposal would change; or, with
        `files`, the same change file by file."""
        if args.get("files"):
            keys = ("from", "to", "proposal", "file", "notation")
            reply = self.host.request({"type": "fileChanges", **{k: args[k] for k in keys if k in args}})
            if reply.get("type") != "fileChanges":
                return f"error: {reply.get('error')}", True
            return compact(reply.get("changes")), False
        reply = self.host.request({"type": "diff", **{k: args[k] for k in ("from", "to", "proposal") if k in args}})
        if reply.get("type") != "diff":
            return f"error: {reply.get('error')}", True
        impact = reply.get("impact")  # the layers it touches: view, logic, model, data
        return compact(reply.get("patch")) + (f"\nimpact: {compact(impact)}" if impact else ""), False

    def get_models(self, args):
        """The get_models tool: the data layer's models and how many records each collection holds, never a record."""
        catalog = self.host.request({"type": "getCatalog"}).get("catalog") or {}
        return compact(catalog.get("data") or {"collections": {}, "queries": []}), False

    def change_main(self, kind, args):
        """The restore_revision and revert_revision tools: main brought back to a revision, or one revision undone."""
        return describe(self.host.request({"type": kind, "author": self.client, "revision": args.get("revision")}))

    def get_events(self, args):
        """The get_events tool: what happened since the last call."""
        return compact(self.host.take_events()), False

    def export_changes(self, args):
        """The export_changes tool: the revisions after a point, as a change set."""
        reply = self.host.request({"type": "exportChanges", "from": args.get("from", 0),
                                   "name": args.get("name", "changes")})
        return json.dumps(reply.get("changes"), indent=2), False

    # The tools this server answers itself, by name: each takes the call's arguments and returns (content, is_error)
    ANSWERS = {
        "get_blame": get_blame,
        "wait_for_request": next_request,
        "reply": reply,
        "answer_repair": answer_repair,
        "get_screen": get_screen,
        "get_catalog": get_catalog,
        "get_models": get_models,
        "find_nodes": search_nodes,
        "get_contributions": get_contributions,
        "check_composition": lambda self, args: self.compose(args, False),
        "install_feature": lambda self, args: self.compose(args, True),
        "get_format_guide": get_format_guide,
        "submit_revision": submit_revision,
        "render_screen": render_screen,
        "run_scenarios": run_scenarios,
        "get_evidence": get_evidence,
        "get_history": get_history,
        "get_diff": get_diff,
        "restore_revision": lambda self, args: self.change_main("restore", args),
        "revert_revision": lambda self, args: self.change_main("revert", args),
        "get_events": get_events,
        "export_changes": export_changes,
    }

    def call_tool(self, name, args):
        """Answers a tools/call: (content, is_error), the content text or MCP content blocks. Raises UnknownTool for a
        name no tool has."""
        answer = self.ANSWERS.get(name) if isinstance(name, str) else None  # a name may be any JSON value
        if answer is not None:
            return answer(self, args)
        coordinated = coordination_tool(self.host, self.client, name, args)
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
        """Answers one JSON-RPC message from the client: a refusal when it is malformed, nothing for a notification,
        else a result or an error with its id."""
        refusal = malformed(request)
        if refusal:
            self.send(refusal)
            return
        method, params = request.get("method"), request.get("params") or {}
        if "id" not in request:  # a notification
            self.notified(method)
        elif method == "tools/call":
            # On its own thread: wait_for_request can block for minutes while pings and other calls go on
            threading.Thread(target=self.answer_call, args=(request["id"], params), daemon=True).start()
        else:
            self.answer(request["id"], method, params)

    def notified(self, method):
        """A notification from the client: once it is initialized, channel events may be sent."""
        if method == "notifications/initialized":
            self.ready = True
            try:
                self.host.ensure()  # connect early, so requests sent before the first tool call arrive too
            except RuntimeError as failure:
                log(str(failure))

    def result(self, rid, value):
        """Sends the result of the client's request `rid`."""
        self.send({"jsonrpc": "2.0", "id": rid, "result": value})

    def error(self, rid, code, message):
        """Sends a JSON-RPC error for the client's request `rid`."""
        self.send({"jsonrpc": "2.0", "id": rid, "error": {"code": code, "message": message}})

    def answer(self, rid, method, params):
        """Answers a request other than tools/call: the handshake, ping, and the tool and prompt lists."""
        if method == "initialize":
            self.client = client_name(params)
            requested = params.get("protocolVersion")
            self.result(rid, {"protocolVersion": requested if requested in PROTOCOLS else PROTOCOLS[0],
                              "capabilities": {"tools": {"listChanged": False}, "prompts": {"listChanged": False},
                                               "experimental": {"claude/channel": {}}},
                              "serverInfo": {"name": "affrescoui", "version": "1.0.0"},
                              "instructions": INSTRUCTIONS})
        elif method == "ping":
            self.result(rid, {})
        elif method == "tools/list":
            self.result(rid, {"tools": TOOLS})
        elif method == "prompts/list":
            self.result(rid, {"prompts": PROMPTS})
        elif method == "prompts/get" and params.get("name") == "serve_studio":
            self.result(rid, {"description": PROMPTS[0]["description"],
                              "messages": [{"role": "user", "content": {"type": "text", "text": SERVE_PROMPT}}]})
        elif method == "prompts/get":
            self.error(rid, -32602, f"unknown prompt {params.get('name')!r}")
        else:
            self.error(rid, -32601, f"method not found: {method}")

    def answer_call(self, rid, params):
        """Answers a tools/call, on its own thread. A tool that fails answers with the failure as an error result."""
        try:
            content, is_error = self.call_tool(params.get("name"), params.get("arguments") or {})
            if isinstance(content, str):  # most tools answer in text; render_screen with an image
                content = [{"type": "text", "text": content}]
            self.result(rid, {"content": content, "isError": is_error})
        except UnknownTool:
            self.error(rid, -32602, f"unknown tool {params.get('name')!r}")
        except Exception as failure:  # no host, it went away, did not answer, ...
            self.result(rid, {"content": [{"type": "text", "text": f"error: {failure}"}], "isError": True})


USAGE = "[--socket DIR] [--root DIR]... [--launch PROGRAM [--launch-arg ARG]...]"


def main():
    """Runs the server: `USAGE`, then JSON-RPC lines on stdin until it closes. The launch program and each of its
    arguments come one per option, never split from a command line, so no shell or parser decides what runs."""
    args = sys.argv[1:]
    socket_dir = default_socket_dir()
    launch, roots = None, []
    while args:
        arg = args.pop(0)
        if arg == "--socket" and args:
            socket_dir = args.pop(0)
        elif arg == "--root" and args:
            roots.append(args.pop(0))
        elif arg == "--launch" and args:
            launch = [args.pop(0), *(launch or [])[1:]]
        elif arg == "--launch-arg" and args and launch:
            launch.append(args.pop(0))
        else:
            sys.exit(f"usage: {sys.argv[0]} {USAGE}")

    # MCP's stdio messages are UTF-8 lines ending in \n, whatever the console's code page and line ending. A byte that
    # isn't UTF-8 becomes U+FFFD, so it costs that message, not the server
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", newline="\n")
    server = Server(socket_dir, launch, roots)
    log(f"ready; the host is expected at {server.host.path}")
    if not hasattr(socket, "AF_UNIX"):
        log(NO_UNIX_SOCKETS)
    for line in sys.stdin:
        if line.strip():
            server.serve(line)
