#!/usr/bin/env python3
"""End-to-end agent loop for the game launcher example.

Starts the host on the launcher screen, drives it the way an agent would through the MCP server (read the screen,
propose a change), approves the proposal as a `studio` peer, and checks the picture the agent gets back. Then it plays
the launcher's own scenarios headless.

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
SCREEN = os.path.join(HERE, "index.screen.json")
SCENARIOS = [os.path.join(HERE, "index.scenarios.json"), os.path.join(HERE, "controls.scenarios.json")]


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

    def __init__(self, socket_dir, cli):
        """Starts it on `socket_dir`, with `cli` as its affrescoui tool: the build's under test, not one on the PATH."""
        self.proc = subprocess.Popen([sys.executable, os.path.join(ROOT, "mcp", "affrescoui_mcp.py"),
                                      "--socket", socket_dir], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                     stderr=subprocess.DEVNULL, text=True, encoding="utf-8",
                                     env=dict(os.environ, AFFRESCOUI_CLI=cli))
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


def main():
    """Runs the loop; exits non-zero with a reason on the first failure."""
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    build = os.path.abspath(sys.argv[1])
    work = tempfile.mkdtemp(prefix="afr-gl-")  # short: socket paths are limited to about 100 bytes
    sockets, data = os.path.join(work, "s"), os.path.join(work, "data")
    env = dict(os.environ, AFFRESCOUI_SOCKET_DIR=sockets)
    host = subprocess.Popen([os.path.join(build, "affrescoui_launcher"), SCREEN, "--socket", sockets, "--data", data,
                             "--grant", "assistant"],
                            env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        studio, studio_stream = connect(os.path.join(sockets, "studio.sock"))
        mcp = Mcp(sockets, os.path.join(build, "affrescoui"))

        # 1. The agent reads the screen
        text = mcp.tool("get_screen", {"at": "#details-title"})[0]["text"]
        assert "details-title" in text, text

        # 2. ... and proposes a change: agents need a reviewer's approval in this screen
        text = mcp.tool("submit_revision", {"message": "Dim the blurb",
                                            "set": {"#details-blurb/textColor": "@color.muted"}})[0]["text"]
        assert text.startswith("proposed as proposal "), text
        proposal = int(text.split()[3])

        # The studio peer is told of it, with the head it will approve
        studio.settimeout(20)
        head = None
        while head is None:
            message = json.loads(studio_stream.readline())
            for open_proposal in message.get("proposals", []) if message.get("type") == "review" else []:
                if open_proposal["id"] == proposal:
                    head = open_proposal["head"]

        # 3. The agent checks its work on the proposal before anyone reviews it
        picture = mcp.tool("render_screen", {"proposal": proposal})[0]
        png = base64.b64decode(picture["data"])
        assert picture["type"] == "image" and png.startswith(b"\x89PNG"), picture.get("type")

        # 4. The person approves it, as a studio peer
        studio_stream.write((json.dumps({"type": "accept", "proposal": proposal,
                                         "head": head}) + "\n").encode())
        studio_stream.flush()
        reply = {}
        while reply.get("type") != "applied":
            reply = json.loads(studio_stream.readline())
            assert reply.get("type") != "rejected", reply
        assert reply.get("proposal") == proposal, reply

        # 5. The change is on main now
        text = mcp.tool("get_screen", {"at": "#details-blurb"})[0]["text"]
        assert "@color.muted" in text, text
        mcp.close()
        studio.close()
    finally:
        host.terminate()
        host.wait(timeout=10)

    # 6. The launcher's own scenarios and the scenarios of its controls, headless
    shots = os.path.join(work, "shots")
    run = subprocess.run([os.path.join(build, "affrescoui"), "test", *SCENARIOS, "--out", shots],
                         capture_output=True, text=True)
    if run.returncode != 0:
        sys.exit(run.stdout + run.stderr)
    shutil.rmtree(work, ignore_errors=True)
    print("game launcher: agent loop and scenarios passed")


if __name__ == "__main__":
    main()
