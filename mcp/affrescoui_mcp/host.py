"""The host at the other end of agent.sock: where it listens, connecting to it, and the messages it sends."""

import collections
import itertools
import json
import os
import socket
import stat
import subprocess
import sys
import threading
import time

TIMEOUT = 10  # seconds to wait for the host to answer
MAX_EVENTS = 1000  # news kept for get_events: an agent that never asks would otherwise keep all of it
MAX_RUNS = 64  # runs that waited whose later outcome is kept, the newest (as many as the host keeps)
NO_UNIX_SOCKETS = ("this Python has no Unix domain sockets (socket.AF_UNIX), which the host listens on: on Windows "
                   "no released CPython has them yet, see mcp/README.md")


def printable(text):
    """Text on one line: a backslash and the characters that aren't printable (a line break, a control character)
    escaped, as the host's log does, so text people or peers wrote can't start a line of its own nor send escape
    sequences to a terminal."""
    return "".join(c if c.isprintable() else c.encode("unicode_escape").decode("ascii")
                   for c in str(text).replace("\\", "\\\\"))


def log(text):
    """Writes a line to stderr, the server's log. Its text can come from people (a request), so it is printable()."""
    print(f"[affrescoui-mcp] {printable(text)}", file=sys.stderr, flush=True)


def default_socket_dir():
    """Where the host listens unless --socket says otherwise: one rule for the host and this server."""
    if os.environ.get("AFFRESCOUI_SOCKET_DIR"):
        return os.environ["AFFRESCOUI_SOCKET_DIR"]
    if os.name == "nt":  # a folder in the user's profile, which its ACL keeps private to the user
        local = os.environ.get("LOCALAPPDATA") or os.path.join(os.path.expanduser("~"), "AppData", "Local")
        return os.path.join(local, "affrescoui", "sockets")
    return os.path.join("/tmp", f"affrescoui-{os.getuid()}")


def check_socket_dir(path):
    """Refuses a socket directory another user could control (its path in /tmp is easy to guess and take first)."""
    try:
        info = os.lstat(path)
    except FileNotFoundError:
        return
    # Windows has no uid (st_uid is 0): its default folder is private by its ACL, so only the kind is checked there
    owned = os.name == "nt" or info.st_uid == os.getuid()
    if not stat.S_ISDIR(info.st_mode) or not owned:
        raise RuntimeError(f"{path} is not a directory owned by this user: remove it or pass --socket DIR")
    # The host makes it 0700; a folder others may enter or write lets them swap the socket for their own
    if os.name != "nt" and info.st_mode & 0o077:
        raise RuntimeError(f"{path} is open to other users (mode {stat.S_IMODE(info.st_mode):o}): chmod 700 it or "
                           "pass --socket DIR")


def connect_unix(path):
    """A stream socket connected to the host at path, or a RuntimeError saying why there is none."""
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    try:
        sock.connect(path)
    except OSError:
        sock.close()
        raise RuntimeError(f"no AffrescoUI host is listening at {path}. Start one: ./build/affrescoui_launcher")
    return sock


class Host:
    """The running affrescoui_launcher, at the other end of agent.sock. Connects (again) when needed."""

    def __init__(self, socket_dir, launch, on_request):
        """A host listening in `socket_dir`; `launch` (a program and its arguments, a list, or None) starts one; `on_request` gets each person's
        request the host relays."""
        self.path = os.path.join(socket_dir, "agent.sock")
        self.launch = launch
        self.on_request = on_request
        self.sock = None
        self.lock = threading.Lock()
        self.connecting = threading.Lock()  # held while connecting (and launching), apart from self.lock
        self.ids = itertools.count(1)
        self.waiting = {}  # request id -> [threading.Event, reply]
        self.events = collections.deque(maxlen=MAX_EVENTS)  # what happened since the agent last asked, the latest
        self.dropped = 0  # older news the bound let go since then
        self.rules = ("default", [])  # the mode and grants last reported
        # Runs that waited, by number: [threading.Event, outcome, abandoned]. The host answers a run that waits twice
        # with the request's id; the second answer comes after request() let its waiter go, so it lands here
        self.runs = collections.OrderedDict()
        self.committed = False  # a commit came since get_events last asked: the commands may have changed

    def ensure(self):
        """Connects to the host unless connected, starting it first with --launch when none listens; a RuntimeError
        says why there is no host. Calls that find no connection at once wait for one of them to make it: each
        would otherwise connect (and launch a host) of its own, and all but the last connection would stay open,
        each with its reader, for nothing."""
        with self.connecting:
            with self.lock:
                if self.sock:
                    return
            # Before --launch: a host this Python can't reach would be started again on every call
            if not hasattr(socket, "AF_UNIX"):  # CPython on Windows, so far (python/cpython#77589)
                raise RuntimeError(NO_UNIX_SOCKETS)
            if not os.path.exists(self.path) and self.launch:
                self.start()
            check_socket_dir(os.path.dirname(self.path))
            sock = connect_unix(self.path)
            with self.lock:
                self.sock = sock
            threading.Thread(target=self.read, args=(sock,), daemon=True).start()
            log(f"connected to {self.path}")

    def start(self):
        """Starts the --launch command and waits up to 5 seconds for its socket to appear."""
        log(f"no host running: starting {self.launch[0]}")
        # An argument vector, no shell. Standard output is this server's channel to the client, so the host gets none;
        # it keeps standard error. Its own session lets the host outlive this server, as a host the person started.
        try:
            subprocess.Popen(self.launch, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, start_new_session=True)
        except OSError as failure:  # reported like a host that isn't there, not ending the server
            raise RuntimeError(f"couldn't start {self.launch[0]}: {failure}")
        for _ in range(50):
            if os.path.exists(self.path):
                break
            time.sleep(0.1)

    def read(self, sock):
        """Reads the host's messages until it closes the connection. Whatever stops the reading closes it too, so the
        next call connects again instead of waiting for answers nobody reads."""
        try:
            for line in sock.makefile("r", encoding="utf-8", errors="replace"):
                try:
                    message = json.loads(line)
                except (ValueError, RecursionError):  # as Server.serve
                    continue
                if isinstance(message, dict):
                    self.dispatch(message)
        finally:
            with self.lock:
                if self.sock is sock:
                    self.sock = None
            sock.close()
            log("the host closed the connection")

    def dispatch(self, message):
        """One message from the host: the answer someone waits for, a person's request, or news for get_events."""
        kind = message.get("type")
        with self.lock:
            pending = self.waiting.get(message.get("id")) if isinstance(message.get("id"), str) else None
            # A waiter takes one answer: an outcome that follows before request() lets it go is a run's, below
            if pending and kind != "request" and not pending[0].is_set():
                if kind == "waiting":  # made before the waiter wakes, so an outcome that follows at once has a place
                    self.keep_run(message.get("run"))
                pending[1] = message
                pending[0].set()
                return
            if kind in ("ran", "rejected") and self.settle_run(message):
                return
        if kind in ("request", "cancel"):  # a person's request, or their cancelling one
            self.on_request(message)
            return
        event = self.news(kind, message)
        if event is not None:
            self.note(event)

    def news(self, kind, message):
        """What a message nobody waits for tells get_events, or None if it tells nothing."""
        if kind == "event":
            return {"event": message.get("name"), "data": message.get("data")}
        if kind in ("applied", "rejected") and "proposal" in message:  # a reviewer decided
            return {"proposal": message["proposal"], "outcome": "merged" if kind == "applied" else "rejected",
                    "revision": message.get("revision"), "detail": message.get("error")}
        if kind in ("ran", "rejected") and isinstance(message.get("run"), int):  # a run's outcome nobody waits for
            return run_news(kind, message)
        if kind == "committed":
            self.committed = True
            return {"committed": message.get("revision"), "commit": message.get("short"),
                    "author": message.get("author"), "committer": message.get("committer"),
                    "message": message.get("message")}
        if kind == "review":
            return self.review_news(message)
        if kind == "error":
            return {"error": message.get("message")}
        return None

    def review_news(self, message):
        """The review's mode and "don't ask again" grants, only when the reviewer changed them since the last report
        (the host reports them on every connection). Called on the reading thread alone, as is dispatch."""
        rules = (message.get("mode"), [g.get("rule") for g in message.get("grants") or [] if isinstance(g, dict)])
        if rules == self.rules:
            return None
        self.rules = rules
        return {"mode": rules[0], "grants": rules[1]}

    def keep_run(self, run):
        """Makes a place for the later outcome of run `run`, the oldest let go once MAX_RUNS are kept. Called with
        self.lock held."""
        if not isinstance(run, int) or isinstance(run, bool):
            return
        self.runs[run] = [threading.Event(), None, False]
        self.runs.move_to_end(run)
        while len(self.runs) > MAX_RUNS:
            self.runs.popitem(last=False)

    def settle_run(self, message):
        """Gives a run's later outcome to the run_command call that waits for it: True if one does. Called with
        self.lock held."""
        place = self.runs.get(message.get("run")) if isinstance(message.get("run"), int) else None
        if place is None or place[2] or place[0].is_set():
            return False
        place[1] = message
        place[0].set()
        return True

    def await_run(self, run, seconds):
        """The later outcome of run `run` ("ran" or "rejected"), if it comes within `seconds`; else None, and an
        outcome that comes after is news for get_events."""
        with self.lock:
            place = self.runs.get(run)
        if place is None:
            return None
        place[0].wait(seconds)
        with self.lock:  # an outcome that came as the wait ended is still this call's
            self.runs.pop(run, None)
            if place[1] is None:
                place[2] = True
            return place[1]

    def note(self, event):
        """Keeps news for the next get_events, the oldest let go once MAX_EVENTS wait."""
        with self.lock:
            self.dropped += len(self.events) == MAX_EVENTS
            self.events.append(event)

    def send(self, message):
        """Sends one message to the host, connecting (or starting it) first if needed."""
        self.ensure()
        with self.lock:
            if not self.sock:
                raise RuntimeError("lost the connection to the host")
            self.sock.sendall((json.dumps(message) + "\n").encode("utf-8"))

    def request(self, message):
        """Sends a message and waits for the answer carrying the same id."""
        request_id = f"mcp-{next(self.ids)}"
        pending = [threading.Event(), None]
        with self.lock:
            self.waiting[request_id] = pending
        try:
            self.send({**message, "id": request_id})
            if not pending[0].wait(TIMEOUT):
                raise RuntimeError("the host did not answer")
            return pending[1]
        finally:
            with self.lock:
                self.waiting.pop(request_id, None)

    def take_events(self):
        """The news since the last call, oldest first; there is none left after it."""
        with self.lock:
            events, dropped = list(self.events), self.dropped
            self.events.clear()
            self.dropped = 0
            if self.committed:  # a commit can add, change or remove commands: list them again before relying on them
                events.append({"commands": "may have changed"})
                self.committed = False
        if dropped:
            log(f"{dropped} older events were dropped: get_events keeps the latest {MAX_EVENTS}")
        return events


def run_news(kind, message):
    """What get_events says of a run's outcome nobody waited for: its number, command and outcome, and the error or
    how many changes it made."""
    if kind == "rejected":
        detail = message.get("error")
    elif isinstance(message.get("changed"), list):
        detail = len(message["changed"])
    else:
        detail = message.get("changes")
    return {"run": message["run"], "command": message.get("command"), "outcome": kind, "detail": detail}
