#!/usr/bin/env python3
"""End-to-end: add Sinopia's slide sorter to Tempera.

Starts Tempera, connects as a `studio` peer (the person's client) and asks an agent to install the sorter in a
request. A scripted fake agent (fake_agent.py) reads the contributions through the MCP server, writes the adapter,
checks the composition and submits the install; the `studio` peer accepts the proposal. Then the live document plays
the sorter docked and reordering Tempera's layers, the install shows in history and blame, and undoing it removes it.

Usage: e2e_test.py BUILD_DIR
"""

import json
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
TEMPERA = os.path.join(os.path.dirname(HERE), "tempera")

LIVE_SCENARIOS = {"format": "affrescoui-scenarios", "version": 2, "screens": TEMPERA, "screen": "index.screen.json",
                  "scenarios": [{"name": "bottega-sorter-live", "steps": [
                      {"expect": {"node": "#slideSorter-view", "exists": True}},
                      {"expect": {"node": "#slideSorter-title", "text": "Pages (3)"}},
                      {"drag": "#slideSorter-page-7", "to": "#slideSorter-page-5"},
                      {"expect": {"state": "doc.layers[1].name", "is": "Hills"}}]}]}


class StudioPeer:
    """A `studio` peer's connection to the host: one JSON object per line."""

    def __init__(self, path, timeout=20.0):
        deadline = time.monotonic() + timeout
        while True:
            self.socket = socket.socket(socket.AF_UNIX)
            try:
                self.socket.connect(path)
                break
            except OSError:
                self.socket.close()
                if time.monotonic() > deadline:
                    sys.exit(f"the host never opened {os.path.basename(path)}")
                time.sleep(0.1)
        self.socket.settimeout(120)
        self.stream = self.socket.makefile("rwb")
        self.stream.readline()  # hello
        self.next_id = 0

    def send(self, message):
        """Sends one message."""
        self.stream.write((json.dumps(message) + "\n").encode())
        self.stream.flush()

    def until(self, wanted):
        """Reads messages until one that wanted() accepts, and returns it."""
        while True:
            line = self.stream.readline()
            if not line:
                sys.exit("the host closed the studio peer's connection")
            message = json.loads(line)
            if wanted(message):
                return message

    def ask(self, message):
        """Sends a message with an id and returns the host's answer to it."""
        self.next_id += 1
        self.send({**message, "id": f"s{self.next_id}"})
        return self.until(lambda answer: answer.get("id") == f"s{self.next_id}")


def install(build, sockets, studio):
    """The `studio` peer asks, the fake agent installs, the `studio` peer accepts. Returns the agent's reply."""
    env = dict(os.environ, AFFRESCOUI_SOCKET_DIR=sockets, AFFRESCOUI_CLI=os.path.join(build, "affrescoui_bottega"))
    agent = subprocess.Popen([sys.executable, os.path.join(HERE, "fake_agent.py"), sockets], env=env,
                             stdout=subprocess.PIPE, text=True)
    if json.loads(agent.stdout.readline() or "{}").get("ready") is not True:
        sys.exit("the fake agent didn't start")
    studio.send({"type": "request", "id": "install-1",
                 "text": "Install slideSorter from app:sinopia into the panels dock: draft the adapter"})
    proposal = json.loads(agent.stdout.readline() or "{}").get("proposal")
    if agent.wait(timeout=300) != 0 or proposal is None:
        sys.exit("the fake agent failed")
    reply = studio.until(lambda message: message.get("type") == "reply")
    head = None
    while head is None:
        review = studio.ask({"type": "getDocument"})
        head = next((p["head"] for p in review.get("proposals", []) if p["id"] == proposal), None)
    applied = studio.ask({"type": "accept", "proposal": proposal, "head": head})
    assert applied.get("type") in ("applied", "done"), applied
    return reply


def check_live(build, work, studio):
    """The live document plays the sorter, history and blame show the install, and undo removes it."""
    live = studio.ask({"type": "getDocument"})
    assert "slideSorter" in live["document"].get("features", {}), "not installed"
    document, scenarios = os.path.join(work, "live.json"), os.path.join(work, "live.scenarios.json")
    with open(document, "w", encoding="utf-8") as file:
        json.dump(live["document"], file)  # its state as declared: the live one holds the feature's own
    with open(scenarios, "w", encoding="utf-8") as file:
        json.dump(LIVE_SCENARIOS, file)
    run = subprocess.run([os.path.join(build, "affrescoui_bottega"), "--document", document, scenarios],
                         capture_output=True, text=True)
    if run.returncode != 0:
        sys.exit(run.stdout + run.stderr)
    history = studio.ask({"type": "getHistory"})["entries"]
    assert any("Install slideSorter from app:sinopia" in entry.get("message", "") for entry in history), history
    blame = studio.ask({"type": "blame", "target": "/features/slideSorter"})
    assert blame.get("commit", {}).get("author") in ("agent", "studio"), blame
    undone = studio.ask({"type": "undo"})
    assert undone.get("type") in ("applied", "done"), undone
    assert "slideSorter" not in studio.ask({"type": "getDocument"})["document"].get("features", {}), "not undone"


def main():
    """Runs the demo; exits non-zero with a reason on the first failure."""
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    build = os.path.abspath(sys.argv[1])
    work = tempfile.mkdtemp(prefix="afr-bt-")  # short: socket paths are limited to about 100 bytes
    sockets = os.path.join(work, "s")
    host = subprocess.Popen([os.path.join(build, "affrescoui_tempera"), "--socket", sockets,
                             "--data", os.path.join(work, "data")],
                            env=dict(os.environ, AFFRESCOUI_SOCKET_DIR=sockets),
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        studio = StudioPeer(os.path.join(sockets, "studio.sock"))
        reply = install(build, sockets, studio)
        assert "slide sorter" in reply.get("text", ""), reply
        check_live(build, work, studio)
        studio.socket.close()
    finally:
        host.terminate()
        host.wait(timeout=10)
    shutil.rmtree(work, ignore_errors=True)
    print("bottega: the slide sorter installed into Tempera by an agent, reviewed, docked, reordering and undone")


if __name__ == "__main__":
    main()
