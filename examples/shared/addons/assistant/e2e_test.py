#!/usr/bin/env python3
"""The assistant add-on end to end: the stock host, the MCP server as the agent, and a page where a timer stands in
for the person (e2e.screen.json).

The page asks before any agent is connected and is told no one is there (offline). Then the agent, through the MCP
server, waits for the person's request, proposes a change and replies; the person accepts the proposal card and the
change is on main. The agent's second proposal is rejected on its card, and main stays as it was.

Usage: e2e_test.py BUILD_DIR
"""

import json
import os
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
SCREEN = os.path.join(HERE, "e2e.screen.json")


class Mcp:
    """The MCP server over stdio, as Claude Code runs it."""

    def __init__(self, socket_dir, cli):
        """Starts it on `socket_dir`, with `cli` as its affrescoui tool: the build's under test, not one on the PATH."""
        self.proc = subprocess.Popen([sys.executable, os.path.join(ROOT, "mcp", "affrescoui_mcp.py"),
                                      "--socket", socket_dir], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                     stderr=subprocess.DEVNULL, text=True, encoding="utf-8",
                                     env=dict(os.environ, AFFRESCOUI_CLI=cli))
        self.next_id = 0
        self.call("initialize", {"protocolVersion": "2025-06-18", "capabilities": {},
                                 "clientInfo": {"name": "e2e", "version": "1"}})

    def call(self, method, params):
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
        """Calls a tool and returns its first text; fails the test on a tool error."""
        result = self.call("tools/call", {"name": name, "arguments": arguments})
        if result.get("isError"):
            sys.exit(f"{name}: {result['content']}")
        return result["content"][0]["text"]

    def screen(self):
        """The running document, with its live state."""
        return json.loads(self.tool("get_screen", {"full": True}))["document"]

    def until(self, what, test, timeout=20.0):
        """Reads the screen until `test(document)` holds; fails with `what` after the timeout."""
        deadline = time.monotonic() + timeout
        while True:
            document = self.screen()
            if test(document):
                return document
            if time.monotonic() > deadline:
                sys.exit(f"timed out waiting for {what}: {json.dumps(document.get('state'))[:2000]}")
            time.sleep(0.2)

    def close(self):
        """Stops the server."""
        self.proc.stdin.close()
        self.proc.wait(timeout=10)


def title(document):
    """The page title node."""
    return document["root"]["children"][0]


def request_and_propose(mcp, text, change, message):
    """Waits for the person's request `text`, proposes `change` for it and replies; returns the proposal's id."""
    request = json.loads(mcp.tool("wait_for_request", {"timeout_seconds": 30}))
    assert request.get("text") == text, request
    assert request.get("origin") == "app", request
    proposed = mcp.tool("submit_revision", {"message": message, "set": change})
    assert proposed.startswith("proposed as proposal "), proposed
    proposal = int(proposed.split()[3])
    reply = mcp.tool("reply", {"request_id": request["request_id"], "text": "Here it is."})
    assert reply == "sent to the app", reply
    return proposal


def cards(document):
    """The proposal cards in the conversation."""
    return [m for m in document["state"]["assistant"]["messages"] if m.get("kind") == "card"]


def main():
    """Runs the loop; exits non-zero with a reason on the first failure."""
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    build = os.path.abspath(sys.argv[1])
    work = tempfile.mkdtemp(prefix="afr-as-")  # short: socket paths are limited to about 100 bytes
    sockets, data = os.path.join(work, "s"), os.path.join(work, "data")
    env = dict(os.environ, AFFRESCOUI_SOCKET_DIR=sockets)
    # The page asks the agents, so the host would ask the person first: they said no before (apps/host's grants.json),
    # so the host attaches no agent of its own, and this one, over MCP, needs no grant
    os.makedirs(os.path.join(data, "apps", "e2e"))
    with open(os.path.join(data, "apps", "e2e", "grants.json"), "w", encoding="utf-8") as grants:
        json.dump({"format": "affrescoui-grants", "version": 1, "decisions": {"assistant": False}}, grants)
    host = subprocess.Popen([os.path.join(build, "affrescoui_launcher"), SCREEN, "--socket", sockets, "--data", data],
                            env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        deadline = time.monotonic() + 20
        while not os.path.exists(os.path.join(sockets, "agent.sock")):
            if time.monotonic() > deadline or host.poll() is not None:
                sys.exit("the host never opened agent.sock")
            time.sleep(0.1)
        mcp = Mcp(sockets, os.path.join(build, "affrescoui"))

        # 1. Asked with no agent connected, the add-on says so
        document = mcp.until("the offline answer", lambda d: len(d["state"]["assistant"]["messages"]) >= 2)
        offline = document["state"]["assistant"]["messages"][:2]
        assert offline[0]["mine"] and offline[0]["text"] == "Is anyone there?", offline
        assert offline[1]["error"] and not offline[1]["mine"], offline

        # 2. The person asks; the agent proposes and replies; the person accepts the card
        renamed = request_and_propose(mcp, "Rename the page to Board", {"#pageTitle/text": "Board"}, "Rename the page")
        document = mcp.until("the accepted card",
                             lambda d: d["state"]["assistant"]["decided"].get(str(renamed)) == "accepted")
        card = next(c for c in cards(document) if c["id"] == renamed)
        assert card["title"] == "Rename the page" and card["lines"], card
        assert title(document)["text"] == "Board", title(document)
        assert "merged" in mcp.tool("get_events", {}), "the agent wasn't told of the merge"

        # 3. The second proposal is rejected on its card, and main stays as it was
        recoloured = request_and_propose(mcp, "Make the title red", {"#pageTitle/textColor": "#cf222e"},
                                         "Red title")
        document = mcp.until("the rejected card",
                             lambda d: d["state"]["assistant"]["decided"].get(str(recoloured)) == "rejected")
        assert title(document)["textColor"] == "#1f2328", title(document)
        assert not json.loads(mcp.tool("get_screen", {"full": True})).get("proposals"), "a proposal is still open"
        mcp.close()
    finally:
        host.terminate()
        host.wait(timeout=10)
    print("assistant e2e: offline, accepted and rejected")


if __name__ == "__main__":
    main()
