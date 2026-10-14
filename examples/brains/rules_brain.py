#!/usr/bin/env python3
"""A stand-in "brain" for the chat demo: turns chat messages into screen revisions with simple keyword rules.

It speaks the live-screen protocol (SCREEN_FORMAT.md, "Live screens") over stdin/stdout, one JSON object per line,
so an LLM-backed brain can replace it later without any change to the app:

    app  -> brain   {"type": "hello", "revision": 0, "document": {...}, "state": {...}}
    app  -> brain   {"type": "event", "name": "chat", "data": {"text": "dark mode"}, "revision": 0}
    brain -> app    {"type": "revision", "parent": 0, "author": "rules", "message": "Dark theme", "patch": [...]}
    app  -> brain   {"type": "applied", "revision": 1}, {"type": "proposed", "proposal": 1} (the user will review it),
                    or {"type": "rejected", "error": "..."}; later {"type": "applied"/"rejected", "proposal": 1}

Try typing: "dark mode", "light mode", "wider sidebar", "narrower sidebar", "bigger text", "smaller text",
"add a counter", "remove the counter", "undo".
"""

import json
import sys

DARK = {
    "background": "#0b1220",
    "surface": "#111827",
    "divider": "#1f2937",
    "text": "#e5e7eb",
    "mutedText": "#6b7280",
    "input": "#1f2937",
    "inputFocused": "#312e81",
}

COUNTER = {
    "id": "counter",
    "type": "label",
    "text": "Clicked {{state.clicks}} times",
    "anchors": {"right": 24, "top": 14},
    "width": 180,
    "height": 36,
    "align": "center",
    "color": "@accent",
    "textColor": "#ffffff",
    "cursor": "hand",
    "hover": {"color": "@accentHover"},
    "pressed": {"color": "@accentPressed"},
    "onClick": [{"increment": "state.clicks"}],
}

revision = 0
light = {}  # the original colors, from the hello message


def send(message):
    print(json.dumps(message), flush=True)


def log(text):
    print(f"[rules brain] {text}", file=sys.stderr, flush=True)


def revise(message, patch):
    send({"type": "revision", "parent": revision, "author": "rules", "message": message, "patch": patch})


def colors(values, message):
    revise(message, [{"op": "replace", "path": f"/colors/{name}", "value": value} for name, value in values.items()])


def on_chat(text):
    t = text.lower()
    if "undo" in t:
        send({"type": "undo"})
    elif "dark" in t:
        colors(DARK, "Dark theme")
    elif "light" in t:
        colors({name: light[name] for name in DARK if name in light}, "Light theme")
    elif "wider" in t:
        revise("Wider sidebar", [{"op": "replace", "path": "/state/sidebarWidth", "value": 400}])
    elif "narrow" in t:
        revise("Narrower sidebar", [{"op": "replace", "path": "/state/sidebarWidth", "value": 200}])
    elif "bigger" in t or "larger" in t:
        revise("Bigger text", [{"op": "replace", "path": "/fonts/default/size", "value": 19}])
    elif "smaller" in t:
        revise("Smaller text", [{"op": "replace", "path": "/fonts/default/size", "value": 14}])
    elif "remove" in t and "counter" in t:
        revise("Removed the counter", [{"op": "remove", "path": "#counter"}])
    elif "counter" in t:
        # New layout *and* new behaviour, while the app runs: a button whose click handler lives in the patch
        revise("Added a click counter", [
            {"op": "add", "path": "/state/clicks", "value": 0},
            {"op": "add", "path": "#header/children/-", "value": COUNTER},
        ])
    else:
        log(f"no rule for {text!r}")


def main():
    global revision, light
    while True:
        line = sys.stdin.readline()
        if not line:
            return  # the app closed the channel
        message = json.loads(line)
        kind = message.get("type")

        if kind == "hello":
            revision = message["revision"]
            light = dict(message["document"].get("colors", {}))
            log(f"connected at revision {revision}")
        elif kind == "event" and message.get("name") == "chat":
            on_chat(message["data"]["text"])
        elif kind == "proposed":
            log(f"waiting for the user to review proposal {message['proposal']}")
        elif kind in ("applied", "rejected"):
            revision = message["revision"]
            if "proposal" in message:
                log(f"the user {'accepted' if kind == 'applied' else 'rejected'} proposal {message['proposal']}")
            elif kind == "rejected":
                log(f"revision rejected: {message['error']}")


if __name__ == "__main__":
    main()
