#!/usr/bin/env python3
"""A tiny JSON-RPC 2.0 service over stdio with LSP's Content-Length framing: the kind of out-of-process service RFC
0002's `jsonrpc` provider talks to (a language server, a search engine, a git helper).

It is started by client.py, which checks the behaviour the RFC specifies. Methods:

- `initialize`: asks the client for its settings (a request from the service to the client,
  `workspace/configuration`), then answers with them and the methods it offers.
- `files/read` {path}: the text of an in-memory file.
- `search/find` {query}: streams `search/match` notifications and `$/progress`, then answers {count}. Honours
  `$/cancelRequest` between matches, answering with LSP's RequestCancelled (-32800).
- `slow/sleep` {ms}: answers after that long (for timeouts).

Only the standard library: the prototype runs anywhere Python 3.9 does.
"""

import json
import queue
import sys
import threading
import time

FILES = {
    "src/main.cpp": "#include <cstdio>\nint main() {\n    std::puts(\"hello\");\n}\n",
    "README.md": "# Example\nA small project to search in.\n",
}
LINES = [f"line {n}: {'alpha' if n % 3 == 0 else 'beta'} {n}" for n in range(1, 61)]

METHOD_NOT_FOUND = -32601
REQUEST_CANCELLED = -32800


def write(message):
    """Sends one framed message on stdout."""
    body = json.dumps(message, separators=(",", ":")).encode("utf-8")
    sys.stdout.buffer.write(b"Content-Length: %d\r\n\r\n" % len(body) + body)
    sys.stdout.buffer.flush()


def read_messages(inbox):
    """Reads framed messages from stdin into a queue, on its own thread; None when stdin closes."""
    stream = sys.stdin.buffer
    while True:
        length = None
        while True:
            line = stream.readline()
            if not line:
                inbox.put(None)
                return
            if line in (b"\r\n", b"\n"):
                break
            name, _, value = line.decode("ascii").partition(":")
            if name.strip().lower() == "content-length":
                length = int(value.strip())
        inbox.put(json.loads(stream.read(length)))


class Service:
    """Answers requests one at a time; notifications and cancellations are read while a search streams."""

    def __init__(self):
        self.inbox = queue.Queue()
        self.cancelled = set()
        self.next_id = 0
        self.waiting = {}  # requests this service sent to the client: id -> response

    def request_client(self, method, params):
        """Sends a request to the client and waits for its response, handling other messages meanwhile."""
        self.next_id += 1
        request_id = f"service-{self.next_id}"
        write({"jsonrpc": "2.0", "id": request_id, "method": method, "params": params})
        while request_id not in self.waiting:
            self.handle(self.inbox.get())
        return self.waiting.pop(request_id)

    def drain(self):
        """Handles what arrived without blocking: cancellations during a stream."""
        while not self.inbox.empty():
            self.handle(self.inbox.get_nowait())

    def handle(self, message):
        """Dispatches one message from the client."""
        if message is None:
            sys.exit(0)
        if "method" not in message:  # a response to one of our requests
            self.waiting[message["id"]] = message
            return
        method, params = message["method"], message.get("params") or {}
        if method == "$/cancelRequest":
            self.cancelled.add(params["id"])
            return
        if "id" not in message:  # another notification: nothing to answer
            return
        self.answer(message["id"], method, params)

    def answer(self, request_id, method, params):
        """Runs a request and writes its result or error."""
        if method == "initialize":
            settings = self.request_client("workspace/configuration", {"items": [{"section": "example"}]})
            result = {"methods": ["files/read", "search/find", "slow/sleep"], "settings": settings.get("result")}
        elif method == "files/read":
            if params.get("path") not in FILES:
                return write({"jsonrpc": "2.0", "id": request_id,
                              "error": {"code": -32602, "message": f"no file '{params.get('path')}'"}})
            result = {"text": FILES[params["path"]]}
        elif method == "search/find":
            result = self.search(request_id, params.get("query", ""))
            if result is None:
                return write({"jsonrpc": "2.0", "id": request_id,
                              "error": {"code": REQUEST_CANCELLED, "message": "cancelled"}})
        elif method == "slow/sleep":
            time.sleep(params.get("ms", 0) / 1000)
            result = {"slept": params.get("ms", 0)}
        else:
            return write({"jsonrpc": "2.0", "id": request_id,
                          "error": {"code": METHOD_NOT_FOUND, "message": f"no method '{method}'"}})
        write({"jsonrpc": "2.0", "id": request_id, "result": result})

    def search(self, request_id, query):
        """Streams the lines that contain the query; None if the client cancelled."""
        count = 0
        for number, line in enumerate(LINES, 1):
            time.sleep(0.005)  # a slow engine, so a cancellation can arrive mid-stream
            self.drain()
            if request_id in self.cancelled:
                return None
            if query in line:
                count += 1
                write({"jsonrpc": "2.0", "method": "search/match",
                       "params": {"request": request_id, "line": number, "text": line}})
            if number % 20 == 0:
                write({"jsonrpc": "2.0", "method": "$/progress",
                       "params": {"token": request_id, "value": {"done": number, "total": len(LINES)}}})
        return {"count": count}

    def run(self):
        """Serves until stdin closes."""
        threading.Thread(target=read_messages, args=(self.inbox,), daemon=True).start()
        while True:
            self.handle(self.inbox.get())


if __name__ == "__main__":
    Service().run()
