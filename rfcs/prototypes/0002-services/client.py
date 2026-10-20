#!/usr/bin/env python3
"""A reference client for RFC 0002's `jsonrpc` provider, and the checks that pin down its behaviour.

    python3 rfcs/prototypes/0002-services/client.py

It starts service.py as a child process (an argument vector, no shell), and checks what the RFC specifies:
requests and results, errors, a streamed result with progress, cancellation, a timeout that cancels, a request from
the service answered by the client, and the framing limits that refuse a malformed or oversized message. The C++
provider (8.5a) is meant to behave the same way; these checks become its unit tests.

Threads, as in the RFC: one reader thread per service reads and parses frames, and everything else (results,
stream events, progress, timeouts) is delivered on the caller's thread through a queue, the stand-in for the
Executor's main thread.
"""

import io
import json
import os
import queue
import subprocess
import sys
import threading
import time

MAX_HEADER_BYTES = 8 * 1024
MAX_MESSAGE_BYTES = 8 * 1024 * 1024
MAX_PENDING = 256
REQUEST_CANCELLED = -32800


class FramingError(Exception):
    """A frame that breaks the rules: the service is then closed, as a channel closes on an oversized line."""


def read_frame(stream):
    """One Content-Length frame's JSON from a binary stream; None at the end. Raises FramingError, naming the
    problem, for a header block over MAX_HEADER_BYTES, a missing or bad Content-Length, a body over
    MAX_MESSAGE_BYTES, a short body, or a body that isn't a JSON object."""
    header_bytes, length = 0, None
    while True:
        line = stream.readline(MAX_HEADER_BYTES + 1)
        if not line:
            if header_bytes == 0:
                return None
            raise FramingError("the stream ended inside a header")
        header_bytes += len(line)
        if header_bytes > MAX_HEADER_BYTES:
            raise FramingError(f"headers longer than {MAX_HEADER_BYTES} bytes")
        if line in (b"\r\n", b"\n"):
            break
        name, separator, value = line.decode("ascii", "replace").partition(":")
        if not separator:
            raise FramingError(f"a header without ':': {line[:40]!r}")
        if name.strip().lower() == "content-length":
            if not value.strip().isdigit():
                raise FramingError(f"Content-Length isn't a number: {value.strip()[:20]!r}")
            length = int(value.strip())
    if length is None:
        raise FramingError("no Content-Length header")
    if length > MAX_MESSAGE_BYTES:
        raise FramingError(f"a message of {length} bytes (at most {MAX_MESSAGE_BYTES})")
    body = stream.read(length)
    if len(body) != length:
        raise FramingError(f"the stream ended {length - len(body)} bytes into a message")
    try:
        message = json.loads(body)
    except ValueError as error:
        raise FramingError(f"a message that isn't JSON: {error}") from None
    if not isinstance(message, dict):
        raise FramingError("a message that isn't a JSON object")
    return message


class Service:
    """One running service: requests with ids, deadlines and cancellation; streams; requests from the service."""

    def __init__(self, argv, handlers=None):
        self.process = subprocess.Popen(argv, stdin=subprocess.PIPE, stdout=subprocess.PIPE)
        self.main = queue.Queue()  # the "main thread": everything is delivered here
        self.handlers = handlers or {}  # requests the service may send us: method -> function(params) -> result
        self.streams = {}  # notification method -> function(params)
        self.pending = {}  # id -> {"done": callback, "deadline": time, "progress": callback}
        self.next_id = 0
        self.closed = None
        threading.Thread(target=self.read, daemon=True).start()

    def read(self):
        """The reader thread: parses frames and posts them; posts the reason when the service goes away."""
        try:
            while (message := read_frame(self.process.stdout)) is not None:
                self.main.put(("message", message))
            self.main.put(("closed", "the service ended"))
        except FramingError as error:
            self.main.put(("closed", str(error)))

    def send(self, message):
        """Writes one framed message to the service."""
        body = json.dumps(message, separators=(",", ":")).encode("utf-8")
        self.process.stdin.write(b"Content-Length: %d\r\n\r\n" % len(body) + body)
        self.process.stdin.flush()

    def invoke(self, method, params, done, timeout=30.0, progress=None):
        """Sends a request; `done(result, error)` runs once, on the main thread. Returns the request's id."""
        if len(self.pending) >= MAX_PENDING:
            done(None, {"code": -32000, "message": f"more than {MAX_PENDING} requests waiting"})
            return None
        self.next_id += 1
        self.pending[self.next_id] = {"done": done, "deadline": time.monotonic() + timeout, "progress": progress}
        self.send({"jsonrpc": "2.0", "id": self.next_id, "method": method, "params": params})
        return self.next_id

    def cancel(self, request_id, reason="cancelled"):
        """Ends a request now: its `done` gets RequestCancelled, the service is told, and a late answer is dropped."""
        waiting = self.pending.pop(request_id, None)
        if waiting:
            self.send({"jsonrpc": "2.0", "method": "$/cancelRequest", "params": {"id": request_id}})
            waiting["done"](None, {"code": REQUEST_CANCELLED, "message": reason})

    def subscribe(self, method, on_event):
        """Delivers every notification of a method to `on_event(params)`."""
        self.streams[method] = on_event

    def deliver(self, message):
        """Runs on the main thread: a response, a notification, or a request from the service."""
        if "method" in message and "id" in message:  # the service asks us something
            handler = self.handlers.get(message["method"])
            if handler:
                self.send({"jsonrpc": "2.0", "id": message["id"], "result": handler(message.get("params"))})
            else:
                self.send({"jsonrpc": "2.0", "id": message["id"],
                           "error": {"code": -32601, "message": f"no handler for '{message['method']}'"}})
        elif "method" in message:  # a notification: progress or a stream
            params = message.get("params") or {}
            if message["method"] == "$/progress":
                waiting = self.pending.get(params.get("token"))
                if waiting and waiting["progress"]:
                    waiting["progress"](params.get("value"))
            elif message["method"] in self.streams:
                self.streams[message["method"]](params)
        else:  # a response; one for a request that was cancelled or timed out is dropped
            waiting = self.pending.pop(message.get("id"), None)
            if waiting:
                waiting["done"](message.get("result"), message.get("error"))

    def run_until(self, condition, limit=10.0):
        """The main loop: delivers messages and fires deadlines until `condition()` holds."""
        end = time.monotonic() + limit
        while not condition():
            if time.monotonic() > end:
                raise AssertionError("the main loop ran out of time")
            try:
                kind, value = self.main.get(timeout=0.01)
                if kind == "message":
                    self.deliver(value)
                else:
                    self.closed = value
                    return
            except queue.Empty:
                pass
            now = time.monotonic()
            for request_id in [i for i, w in self.pending.items() if w["deadline"] < now]:
                self.cancel(request_id, "timed out")

    def close(self):
        """Ends the service: closes its stdin and waits, as ProcessChannel's destructor does."""
        self.process.stdin.close()
        self.process.wait(timeout=5)


def check(name, condition):
    """Prints one check's result; stops at the first failure."""
    print(("ok    " if condition else "FAIL  ") + name)
    if not condition:
        sys.exit(1)


def check_service():
    """The behaviour of requests, streams, progress, cancellation, timeouts and service requests."""
    here = os.path.dirname(os.path.abspath(__file__))
    service = Service([sys.executable, "-I", os.path.join(here, "service.py")],
                      handlers={"workspace/configuration": lambda params: [{"tabSize": 4}]})
    results = {}

    def keep(name):
        return lambda result, error: results.__setitem__(name, (result, error))

    service.invoke("initialize", {}, keep("initialize"))
    service.run_until(lambda: "initialize" in results)
    check("a request from the service is answered by the client",
          results["initialize"][0]["settings"] == [{"tabSize": 4}])

    service.invoke("files/read", {"path": "README.md"}, keep("read"))
    service.run_until(lambda: "read" in results)
    check("a request gets its result", results["read"][0]["text"].startswith("# Example"))

    service.invoke("files/read", {"path": "nope"}, keep("missing"))
    service.invoke("no/such", {}, keep("unknown"))
    service.run_until(lambda: "missing" in results and "unknown" in results)
    check("an error comes back as an error, with the service's message",
          results["missing"][1]["message"] == "no file 'nope'")
    check("an unknown method is MethodNotFound", results["unknown"][1]["code"] == -32601)

    matches, progress = [], []
    service.subscribe("search/match", matches.append)
    service.invoke("search/find", {"query": "alpha"}, keep("search"), progress=progress.append)
    service.run_until(lambda: "search" in results)
    check("a stream delivers every event before the result", len(matches) == results["search"][0]["count"] == 20)
    check("progress arrives for the request", [p["done"] for p in progress] == [20, 40, 60])

    matches.clear()
    search = service.invoke("search/find", {"query": "beta"}, keep("cancelled"))
    service.run_until(lambda: len(matches) >= 3)
    service.cancel(search)
    service.run_until(lambda: "cancelled" in results)
    seen = len(matches)
    late = {}
    service.invoke("files/read", {"path": "README.md"}, lambda r, e: late.setdefault("read", r))
    service.run_until(lambda: "read" in late)  # by now any answer to the cancelled search has arrived
    check("cancelling ends the request with RequestCancelled", results["cancelled"][1]["code"] == REQUEST_CANCELLED)
    check("the service stops streaming after a cancellation", seen < 40 and len(matches) <= seen + 2)

    service.invoke("slow/sleep", {"ms": 400}, keep("slow"), timeout=0.1)
    service.run_until(lambda: "slow" in results)
    check("a timeout ends the request and cancels it", results["slow"][1] == {"code": REQUEST_CANCELLED,
                                                                              "message": "timed out"})
    service.invoke("slow/sleep", {"ms": 0}, keep("after"))
    service.run_until(lambda: "after" in results)
    check("the late answer to a timed-out request is dropped", results["slow"][1]["message"] == "timed out")
    service.close()


def check_framing():
    """Malformed and oversized frames are refused, each with a message that names the problem."""
    cases = {
        "headers over the limit": b"X-Padding: " + b"a" * MAX_HEADER_BYTES + b"\r\n\r\n{}",
        "no Content-Length": b"Content-Type: x\r\n\r\n{}",
        "a length that isn't a number": b"Content-Length: 12a\r\n\r\n{}",
        "a message over the limit": b"Content-Length: %d\r\n\r\n" % (MAX_MESSAGE_BYTES + 1),
        "a short body": b"Content-Length: 10\r\n\r\n{}",
        "a body that isn't JSON": b"Content-Length: 3\r\n\r\nabc",
        "a body that isn't an object": b"Content-Length: 2\r\n\r\n[]",
    }
    for name, data in cases.items():
        try:
            read_frame(io.BytesIO(data))
            refused = False
        except FramingError:
            refused = True
        check(f"framing refuses {name}", refused)
    check("framing reads two frames in a row",
          read_frame(io.BytesIO(b"Content-Length: 2\r\n\r\n{}Content-Length: 2\r\n\r\n{}")) == {})


if __name__ == "__main__":
    check_framing()
    check_service()
    print("all checks passed")
