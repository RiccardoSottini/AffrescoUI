#!/usr/bin/env python3
"""A deterministic stand-in for an AI agent that repairs the "Blocked column" change set (VERSION_CONTROL.md,
"Self-repairing change sets").

It reads the host's messages, one JSON object per line, on stdin, remembers the board's columns (state.lanes.launch) from "hello", and
answers a "request" of kind "repair" with one revision: a "Blocked" column before the last one, whatever that one is
now called. tests/unit/change_set_repair.cpp runs it through a ProcessChannel.
"""

import json
import sys


def repair(request, columns):
    """The answer to a repair request: the Blocked column, before the last column."""
    return {
        "type": "repair",
        "request": request["id"],
        "revision": {
            "message": "Repair: Blocked before the last column",
            "patch": [{
                "op": "add",
                "path": "/state/lanes/launch/%d" % max(len(columns) - 1, 0),
                "value": {"name": "Blocked", "color": "#dc2626"},
            }],
        },
    }


def main():
    """Answers repair requests until stdin closes."""
    columns = []
    for line in sys.stdin:
        try:
            message = json.loads(line)
        except ValueError:
            continue
        if message.get("type") == "hello":
            columns = message.get("state", {}).get("lanes", {}).get("launch", [])
        elif message.get("type") == "request" and message.get("kind") == "repair":
            print(json.dumps(repair(message, columns)), flush=True)


if __name__ == "__main__":
    main()
