#!/usr/bin/env python3
"""A deterministic stand-in for an AI agent that installs Sinopia's slide sorter into Tempera through the MCP server
(VERSION_CONTROL.md, "Installing across apps"), as tests/repair/fake_agent.py stands in for a repairing one.

It waits for the `studio` peer's request, reads what Tempera offers and what the sorter needs (get_contributions),
checks the composition without an adapter (it fails, naming the unmet needs), writes the adapter from Tempera's
exports, checks again, installs it (a proposal for a reviewer) and replies. It prints one JSON line: {"proposal": N}.

Usage: fake_agent.py SOCKET_DIR    (AFFRESCOUI_CLI: affrescoui, which proofs play Tempera's scenarios with)
"""

import json
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
TEMPERA = "examples/tempera/index.screen.json"
SORTER = "examples/sinopia/features/slide-sorter"


class Mcp:
    """The MCP server over stdio, as an agent's client runs it."""

    def __init__(self, socket_dir):
        self.proc = subprocess.Popen([sys.executable, os.path.join(ROOT, "mcp", "affrescoui_mcp.py"),
                                      "--socket", socket_dir], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                     stderr=subprocess.DEVNULL, text=True, encoding="utf-8")
        self.next_id = 0
        self.call("initialize", {"protocolVersion": "2025-06-18", "capabilities": {},
                                 "clientInfo": {"name": "tempera-sorter-agent", "version": "1"}})

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

    def tool(self, name, arguments, failing=False):
        """Calls a tool and returns its text; exits unless it failed exactly when expected."""
        result = self.call("tools/call", {"name": name, "arguments": arguments})
        text = "\n".join(block.get("text", "") for block in result["content"])
        if bool(result.get("isError")) != failing:
            sys.exit(f"{name}: {text}")
        return text


def adapter_for(offers):
    """The adapter an agent writes after reading Tempera's exports: its top-level layers are the pages."""
    for export in ("doc.layers", "doc.active", "layers.move", "layers.select", "layers.duplicate"):
        if export not in offers:
            sys.exit(f"Tempera doesn't offer {export}:\n{offers}")
    return {"format": "affrescoui-adapter", "version": 1, "feature": "slideSorter", "host": "app:tempera",
            "note": "Tempera's top-level layers as the sorter's pages; its actions are the layer actions",
            "bind": {"pages": {"value": "{{map(filter(app.doc.layers, it.depth == 0), "
                                        "{id: it.id, name: it.name, image: it.thumbnail})}}"},
                     "current": {"export": "doc.active"},
                     "reorder": {"action": "layers.move", "with": {"args": {"id": "{{id}}", "onto": "{{onto}}"}}},
                     "select": {"action": "layers.select", "with": {"args": {"id": "{{id}}"}}},
                     "duplicate": {"action": "layers.duplicate", "with": {"args": {"id": "{{id}}"}}}}}


def main():
    """Answers one install request."""
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    mcp = Mcp(sys.argv[1])
    mcp.tool("get_screen", {"outline": True, "depth": 0})  # connects, so the studio peer's request reaches it
    print(json.dumps({"ready": True}), flush=True)
    request = json.loads(mcp.tool("wait_for_request", {"timeout_seconds": 60}))
    if not request.get("request_id") or "slideSorter" not in request.get("text", ""):
        sys.exit(f"unexpected request: {request}")
    offers = mcp.tool("get_contributions", {"file": TEMPERA, "running": True})
    if "slideSorter" not in mcp.tool("get_contributions", {"file": "examples/sinopia/index.screen.json"}):
        sys.exit("Sinopia offers no slideSorter")
    bare = mcp.tool("check_composition", {"screen": TEMPERA, "feature": SORTER}, failing=True)
    if "unmet need pages" not in bare:
        sys.exit(f"the proof without an adapter should name the pages:\n{bare}")
    adapter = adapter_for(offers)
    mcp.tool("check_composition", {"screen": TEMPERA, "feature": SORTER, "adapter": adapter})
    installed = mcp.tool("install_feature", {"screen": TEMPERA, "feature": SORTER, "adapter": adapter})
    words = installed.splitlines()[-1].split()
    if words[:3] != ["proposed", "as", "proposal"]:
        sys.exit(f"the install should wait for review:\n{installed}")
    mcp.tool("reply", {"request_id": request["request_id"],
                       "text": f"Proposed the slide sorter in the panels dock with an adapter:\n{installed}"})
    print(json.dumps({"proposal": int(words[3])}), flush=True)
    mcp.proc.stdin.close()
    mcp.proc.wait(timeout=10)


if __name__ == "__main__":
    main()
