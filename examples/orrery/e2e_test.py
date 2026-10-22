#!/usr/bin/env python3
"""End-to-end agent loop for Orrery, the planetarium example.

Starts Orrery's host (affrescoui_orrery, with its 3D view) on a private socket folder, on a copy of the screen without
its bodies strip, and drives it the way an agent would through the MCP server: it reads the screen, proves that
Sinopia's slide sorter fits Orrery through Orrery's adapter, and installs it, which is a revision the agent may only
propose. The agent gets a picture of the proposal and plays the strip's scenarios on it, the Studio approves it, and
the strip is on main. Then it plays Orrery's own scenarios headless on the screen as shipped.

Usage: e2e_test.py BUILD_DIR
"""

import base64
import json
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
SCREEN = os.path.join(HERE, "orrery.screen.json")
ADAPTER = os.path.join(HERE, "orrery-sorter.adapter.json")
FEATURE = os.path.join(ROOT, "examples", "sinopia", "features", "slide-sorter")
SCENARIOS = [os.path.join(HERE, "orrery.scenarios.json"), os.path.join(HERE, "bodies-strip.scenarios.json")]


def connect(path, timeout=20.0):
    """Connects to a host socket, retrying while the host starts; returns a line reader/writer."""
    deadline = time.monotonic() + timeout
    while True:
        try:
            s = socket.socket(socket.AF_UNIX)
            s.connect(path)
            stream = s.makefile("rwb")
            stream.readline()  # hello
            return s, stream
        except OSError:
            s.close()
            if time.monotonic() > deadline:
                sys.exit(f"the host never opened {os.path.basename(path)}")
            time.sleep(0.1)


class Mcp:
    """The MCP server over stdio, as Claude Code runs it."""

    def __init__(self, socket_dir):
        self.proc = subprocess.Popen([sys.executable, os.path.join(ROOT, "mcp", "affrescoui_mcp.py"),
                                      "--socket", socket_dir], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                     stderr=subprocess.DEVNULL, text=True, encoding="utf-8")
        self.next_id = 0
        self.call_raw("initialize", {"protocolVersion": "2025-06-18", "capabilities": {},
                                     "clientInfo": {"name": "e2e", "version": "1"}})

    def call_raw(self, method, params):
        """Sends one JSON-RPC request and returns its result."""
        self.next_id += 1
        self.proc.stdin.write(json.dumps({"jsonrpc": "2.0", "id": self.next_id, "method": method,
                                          "params": params}) + "\n")
        self.proc.stdin.flush()
        while True:
            line = self.proc.stdout.readline()
            if not line:
                sys.exit(f"the MCP server closed during {method}")
            message = json.loads(line)
            if message.get("id") == self.next_id:
                if "error" in message:
                    sys.exit(f"{method}: {message['error']}")
                return message["result"]

    def tool(self, name, arguments):
        """Calls a tool and returns its content list; fails the test on a tool error."""
        result = self.call_raw("tools/call", {"name": name, "arguments": arguments})
        if result.get("isError"):
            sys.exit(f"{name}: {result['content']}")
        return result["content"]

    def close(self):
        """Stops the server."""
        self.proc.stdin.close()
        self.proc.wait(timeout=10)


def copy_without_strip(work):
    """Writes Orrery's screen without its installed part, as it was before the install, next to a link to the
    examples' shared fonts; returns its path."""
    with open(SCREEN, encoding="utf-8") as f:
        document = json.load(f)
    document.pop("features", None)
    folder = os.path.join(work, "orrery")
    os.makedirs(folder)
    os.symlink(os.path.join(ROOT, "examples", "shared"), os.path.join(work, "shared"))
    screen = os.path.join(folder, "orrery.screen.json")
    with open(screen, "w", encoding="utf-8") as f:
        json.dump(document, f, indent="\t")
    for scenarios in SCENARIOS:
        shutil.copy(scenarios, folder)
    return screen


def main():
    """Runs the loop; exits non-zero with a reason on the first failure."""
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    build = os.path.abspath(sys.argv[1])
    work = tempfile.mkdtemp(prefix="afr-or-")  # short: socket paths are limited to about 100 bytes
    sockets, data = os.path.join(work, "s"), os.path.join(work, "data")
    screen = copy_without_strip(work)
    strip = os.path.join(os.path.dirname(screen), os.path.basename(SCENARIOS[1]))
    env = dict(os.environ, AFFRESCOUI_SOCKET_DIR=sockets)
    host = subprocess.Popen([os.path.join(build, "affrescoui_orrery"), screen, "--socket", sockets, "--data", data],
                            env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        studio, studio_stream = connect(os.path.join(sockets, "studio.sock"))
        mcp = Mcp(sockets)

        # 1. The agent reads the screen: the dock the strip goes in, empty
        text = mcp.tool("get_screen", {"at": "#dock"})[0]["text"]
        assert "bodies.dock" in text, text
        text = mcp.tool("get_contributions", {"file": screen})[0]["text"]
        assert "bodies.dock" in text and "sky.bodies" in text, text

        # 2. It proves Sinopia's slide sorter fits, through Orrery's adapter
        with open(ADAPTER, encoding="utf-8") as f:
            adapter = json.load(f)
        text = mcp.tool("check_composition", {"screen": screen, "feature": FEATURE, "adapter": adapter})[0]["text"]
        assert "proven" in text and "not proven" not in text, text

        # 3. ... and installs it: agents need a reviewer's approval in this screen
        text = mcp.tool("install_feature", {"screen": screen, "feature": FEATURE, "adapter": adapter})[0]["text"]
        assert "proposed as proposal " in text, text
        proposal = int(text.split("proposed as proposal ")[1].split()[0].rstrip(".,:;"))

        # The Studio is told of it, with the head it will approve
        studio.settimeout(20)
        head = None
        while head is None:
            message = json.loads(studio_stream.readline())
            for open_proposal in message.get("proposals", []) if message.get("type") == "review" else []:
                if open_proposal["id"] == proposal:
                    head = open_proposal["head"]

        # 4. The agent checks its work on the proposal before anyone reviews it: a picture, and the strip's scenarios
        picture = mcp.tool("render_screen", {"proposal": proposal})[0]
        png = base64.b64decode(picture["data"])
        assert picture["type"] == "image" and png.startswith(b"\x89PNG"), picture.get("type")
        text = mcp.tool("run_scenarios", {"file": strip, "proposal": proposal})[0]["text"]
        assert "FAILED" not in text and "1 passed" in text, text

        # 5. The person approves it in the Studio
        studio_stream.write((json.dumps({"type": "accept", "proposal": proposal,
                                         "head": head}) + "\n").encode())
        studio_stream.flush()
        reply = {}
        while reply.get("type") != "applied":
            reply = json.loads(studio_stream.readline())
            assert reply.get("type") != "rejected", reply
        assert reply.get("proposal") == proposal, reply

        # 6. The strip is on main now, with its proof
        text = mcp.tool("get_screen", {"at": "/features/slideSorter"})[0]["text"]
        assert "app:sinopia" in text and "proof" in text, text
        text = mcp.tool("run_scenarios", {"file": strip})[0]["text"]
        assert "FAILED" not in text, text
        mcp.close()
        studio.close()
    finally:
        host.terminate()
        host.wait(timeout=10)

    # 7. Orrery's own scenarios, headless, on the screen as shipped (without the 3D view, which only the host has)
    shots = os.path.join(work, "shots")
    run = subprocess.run([os.path.join(build, "affrescoui"), "test", *SCENARIOS, "--out", shots],
                         capture_output=True, text=True)
    if run.returncode != 0:
        sys.exit(run.stdout + run.stderr)
    shutil.rmtree(work, ignore_errors=True)
    print("orrery: agent install, approval, picture and scenarios passed")


if __name__ == "__main__":
    main()
