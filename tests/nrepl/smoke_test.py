#!/usr/bin/env python3
"""The editor server end to end: `affrescoui run <app> --nrepl` on a small app written as a program, and an nREPL
client over loopback TCP that does what CIDER, Calva and Conjure do first, none of which needs the person's yes.

The host writes `.nrepl-port` in the app's folder; the client reads the port there, asks `describe` (the twelve
operations, `versions` without `clojure`), `clone`, and evaluates `*ns*`, a client's own form, answered without asking
the person. The host runs for a few seconds (AFFRESCOUI_SCREENSHOT_AFTER) and returns as a closed window does; then
the port file is gone.

Usage: smoke_test.py BUILD_DIR
"""

import os
import shutil
import socket
import subprocess
import sys
import tempfile
import time

os.environ.setdefault("AFFRESCOUI_HIDDEN", "1")  # its programs never show a window (CONTRIBUTING.md); =0 shows them

MANIFEST = '{:format affrescoui-app :version 1 :id nrepl-smoke :name "Smoke" :entry index.screen.lisp}\n'
ENTRY = """;;; index.screen.lisp --- the smallest app an editor connects to
(defstate title "Smoke" "The title.")
(defview greeting "Says hello." [] `(label#hello :text "Hello"))
(defscreen main
  (panel#root ~(greeting)))
"""
RUN_MS = 6000  # how long the host runs before it returns
OPERATIONS = {"clone", "close", "describe", "ls-sessions", "eval", "load-file", "interrupt", "completions", "complete",
              "lookup", "info", "eldoc"}


def encode(value):
    """Bencode of a str, int, list or dict."""
    if isinstance(value, int):
        return b"i%de" % value
    if isinstance(value, str):
        data = value.encode("utf-8")
        return b"%d:%s" % (len(data), data)
    if isinstance(value, list):
        return b"l" + b"".join(encode(item) for item in value) + b"e"
    return b"d" + b"".join(encode(key) + encode(value[key]) for key in sorted(value)) + b"e"


def decode(data, at=0):
    """One bencoded value from `data` at `at`: (value, next position); raises IndexError if it isn't all there."""
    kind = data[at:at + 1]
    if kind == b"i":
        end = data.index(b"e", at)
        return int(data[at + 1:end]), end + 1
    if kind in (b"l", b"d"):
        at += 1
        items = []
        while data[at:at + 1] != b"e":
            if at >= len(data):
                raise IndexError("cut")
            item, at = decode(data, at)
            items.append(item)
        if kind == b"l":
            return items, at + 1
        return dict(zip(items[::2], items[1::2])), at + 1
    colon = data.index(b":", at)
    length = int(data[at:colon])
    if colon + 1 + length > len(data):
        raise IndexError("cut")
    return data[colon + 1:colon + 1 + length].decode("utf-8"), colon + 1 + length


class Client:
    """An nREPL client over loopback TCP."""

    def __init__(self, port):
        """Connects to 127.0.0.1:`port`."""
        self.sock = socket.create_connection(("127.0.0.1", port), timeout=10)
        self.buffer = b""
        self.next_id = 0

    def request(self, message):
        """Sends a request and returns its messages, up to the one whose status has `done`."""
        self.next_id += 1
        message = dict(message, id=str(self.next_id))
        self.sock.sendall(encode(message))
        answers = []
        while True:
            try:
                value, used = decode(self.buffer)
            except (IndexError, ValueError):
                chunk = self.sock.recv(65536)
                if not chunk:
                    sys.exit(f"the server closed during {message['op']}")
                self.buffer += chunk
                continue
            self.buffer = self.buffer[used:]
            if value.get("id") == message["id"]:
                answers.append(value)
                if "done" in value.get("status", []):
                    return answers

    def close(self):
        """Disconnects."""
        self.sock.close()


def main():
    """Runs the smoke test; exits non-zero with a reason on the first failure."""
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    build = os.path.abspath(sys.argv[1])
    work = tempfile.mkdtemp(prefix="afr-nr-")  # short: socket paths are limited to about 100 bytes
    app = os.path.join(work, "app")
    os.makedirs(app)
    with open(os.path.join(app, "app.lisp"), "w", encoding="utf-8") as manifest:
        manifest.write(MANIFEST)
    with open(os.path.join(app, "index.screen.lisp"), "w", encoding="utf-8") as entry:
        entry.write(ENTRY)
    sockets, data = os.path.join(work, "s"), os.path.join(work, "data")
    port_file = os.path.join(app, ".nrepl-port")
    # Ends by itself after that long, as when the person closes the window: the host cleans up as it returns
    env = dict(os.environ, AFFRESCOUI_SOCKET_DIR=sockets, AFFRESCOUI_SCREENSHOT=os.path.join(work, "end.ppm"),
               AFFRESCOUI_SCREENSHOT_AFTER=str(RUN_MS))
    host = subprocess.Popen([os.path.join(build, "affrescoui"), "run", app, "--nrepl", "--socket", sockets, "--data",
                             data], env=env, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True)
    try:
        deadline = time.monotonic() + 20
        while not os.path.exists(port_file):
            if time.monotonic() > deadline or host.poll() is not None:
                sys.exit("the host never wrote .nrepl-port")
            time.sleep(0.1)
        time.sleep(0.1)  # renamed into place whole, but give the reader a moment all the same
        with open(port_file, encoding="utf-8") as text:
            port = int(text.read())
        line = host.stdout.readline()
        assert line.startswith(f"nREPL server started on port {port} on host 127.0.0.1"), line
        client = Client(port)

        described = client.request({"op": "describe"})[0]
        assert set(described["ops"]) == OPERATIONS, sorted(described["ops"])
        assert "affrescoui" in described["versions"] and "clojure" not in described["versions"], described
        session = client.request({"op": "clone"})[0]["new-session"]
        assert session, "clone gave no session"
        answers = client.request({"op": "eval", "code": "*ns*", "session": session})
        values = [answer["value"] for answer in answers if "value" in answer]
        assert values == ["user"], answers
        client.close()
        if host.wait(timeout=RUN_MS / 1000 + 20) != 0:
            sys.exit(f"the host ended with {host.returncode}")
    finally:
        if host.poll() is None:
            host.terminate()
            host.wait(timeout=10)
    if os.path.exists(port_file):
        sys.exit("the host left .nrepl-port behind")
    shutil.rmtree(work, ignore_errors=True)
    print("nrepl smoke: port file, describe, clone, *ns*, and the file gone")


if __name__ == "__main__":
    main()
