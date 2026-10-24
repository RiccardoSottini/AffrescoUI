#!/usr/bin/env python3
"""The host's own agent end to end, with no MCP: the stock host, the openai-compatible provider pointed at a tiny fake
Chat Completions server on the loopback, and the assistant add-on's test page.

The data folder's intelligence.json names the provider; the host starts with --ask, so the assistant opens and sends
the request. The fake server answers the agent's first call with a submit_revision tool call, the second with a reply.
The page's kept conversation then holds the person's message, the reply and the proposal's card (it waits for the
person's review: nothing was accepted), and the server saw the tools, the request and no key. The host keeps its keys
in memory (AFFRESCOUI_KEYS=memory), never touching the system's keychain.

Usage: e2e_test.py BUILD_DIR
"""

import glob
import http.server
import json
import os
import subprocess
import sys
import tempfile
import threading

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
SCREEN = os.path.join(ROOT, "tests", "render", "screens", "assistant.screen.json")
REQUEST = "Rename the page to Launch plan"


class Provider(http.server.BaseHTTPRequestHandler):
    """A fake Chat Completions server: a revision first, then a reply."""
    seen = []

    def do_POST(self):  # noqa: N802 (the handler's name is http.server's)
        """Answers /v1/chat/completions, keeping what it was sent."""
        body = json.loads(self.rfile.read(int(self.headers.get("Content-Length", "0"))))
        # Only whether a key came, never the header itself: a test's output must not hold a key
        Provider.seen.append({"path": self.path, "authorized": self.headers.get("Authorization") is not None,
                              "body": body})
        if len(Provider.seen) == 1:
            call = {"name": "submit_revision",
                    "arguments": json.dumps({"message": "Rename the page", "set": {"#pageTitle/text": "Launch plan"}})}
            message = {"role": "assistant", "content": None,
                       "tool_calls": [{"id": "call_1", "type": "function", "function": call}]}
            finish = "tool_calls"
        else:
            call = {"name": "reply", "arguments": json.dumps({"text": "Renamed it: it waits for your review."})}
            message = {"role": "assistant", "content": None,
                       "tool_calls": [{"id": "call_2", "type": "function", "function": call}]}
            finish = "tool_calls"
        answer = json.dumps({"choices": [{"message": message, "finish_reason": finish}]}).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(answer)))
        self.end_headers()
        self.wfile.write(answer)

    def log_message(self, *args):
        """Quiet."""


def fail(message):
    """Stops the test."""
    print("FAILED: " + message)
    sys.exit(1)


def main():
    build = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "build")
    host = os.path.join(build, "affrescoui_launcher.exe" if os.name == "nt" else "affrescoui_launcher")
    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Provider)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    port = server.server_address[1]
    with tempfile.TemporaryDirectory(prefix="affrescoui-intelligence-") as folder:
        data = os.path.join(folder, "data")
        sockets = os.path.join(folder, "s")
        os.makedirs(data, mode=0o700)
        os.makedirs(sockets, mode=0o700)
        with open(os.path.join(data, "intelligence.json"), "w", encoding="utf-8") as file:
            json.dump({"provider": "openai-compatible", "model": "fake-model",
                       "providers": {"openai-compatible": {"baseUrl": f"http://127.0.0.1:{port}/v1"}}}, file)
        # AFFRESCOUI_KEYS=memory: the host's keys stay in its memory, so the system's keychain is never read
        env = dict(os.environ, AFFRESCOUI_KEYS="memory", AFFRESCOUI_SOCKET_DIR=sockets, AFFRESCOUI_SCREENSHOT=os.path.join(folder, "shot.ppm"),
                   AFFRESCOUI_SCREENSHOT_AFTER="6000")
        run = subprocess.run([host, SCREEN, "--socket", sockets, "--data", data, "--ask", REQUEST], env=env,
                             capture_output=True, text=True, timeout=120)
        server.shutdown()
        if run.returncode != 0:
            fail(f"the host exited with {run.returncode}: {run.stderr[-2000:]}")
        states = glob.glob(os.path.join(data, "*.state.json"))
        if not states:
            fail("the conversation wasn't kept: " + run.stderr[-2000:])
        with open(states[0], encoding="utf-8") as file:
            kept = json.load(file)
        messages = kept["/assistant/messages"]  # the persisted paths, by JSON Pointer
        texts = [m.get("text") for m in messages]
        cards = [m for m in messages if m.get("kind") == "card"]
        if REQUEST not in texts:
            fail(f"the request isn't in the conversation: {texts}")
        if "Renamed it: it waits for your review." not in texts:
            fail(f"the reply isn't in the conversation: {texts}")
        if len(cards) != 1 or cards[0].get("title") != "Rename the page":
            fail(f"expected one proposal card: {messages}")
        if len(Provider.seen) != 2:
            fail(f"expected two requests to the provider, got {len(Provider.seen)}")
        first = Provider.seen[0]
        if first["path"] != "/v1/chat/completions" or first["authorized"]:
            fail(f"unexpected first request: {first['path']}, with a key: {first['authorized']}")
        names = [tool["function"]["name"] for tool in first["body"].get("tools", [])]
        if "submit_revision" not in names or "reply" not in names:
            fail(f"the tools weren't offered: {names}")
        if REQUEST not in json.dumps(first["body"]["messages"]):
            fail("the request wasn't sent to the provider")
        second = json.dumps(Provider.seen[1]["body"]["messages"])
        if "waits for the person's review" not in second:
            fail("the revision's outcome wasn't given back to the model")
    print("ok: the host's agent read the screen, proposed a revision for review and replied")


if __name__ == "__main__":
    main()
