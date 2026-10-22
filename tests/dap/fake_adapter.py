#!/usr/bin/env python3
"""A deterministic debug adapter for the `dap` service's tests and Forge's debug scenarios.

It speaks the Debug Adapter Protocol over stdin and stdout and "runs" a pretend program instead of a real one: `main`
goes through lines 1 to 10 of the launched file, calls `helper` (lines 12 to 14) on line 3, and raises on line 8.
Every answer is fixed, so a test can name the `seq`, the lines and the values it expects.

Launch arguments it understands, beside `program`:
  "stopOnEntry": true        stop on line 1 before running
  "console": "integratedTerminal"
                             ask the client to start the program in its terminal (a `runInTerminal` reverse request)
                             and answer the launch once the client answered that
  "crash": N                 exit with code N right after the launch is answered
  "flood": N                 write N output events after the launch

`evaluate` of "hang" is never answered (to test timeouts and cancel); `cancel` of it is.
"""

import json
import os
import sys

MAIN_LINES = range(1, 11)
HELPER_LINES = range(12, 15)
CALL_LINE = 3
RAISE_LINE = 8
LIBRARY = "/opt/fake-runtime/lib/runtime.py"

state = {
    "seq": 0,
    "program": "",
    "breakpoints": {},
    "filters": [],
    "line": 0,
    "helper": False,
    "locals": {"count": "3", "name": "'forge'"},
    "items": 2500,
    "pending": {},
    "hanging": None,
    "after_launch": None,
}


def write(message):
    """Writes one message with the next seq, framed by Content-Length."""
    state["seq"] += 1
    message["seq"] = state["seq"]
    body = json.dumps(message).encode("utf-8")
    sys.stdout.buffer.write(b"Content-Length: %d\r\n\r\n" % len(body) + body)
    sys.stdout.buffer.flush()


def respond(request, body=None, error=None):
    """Answers a request."""
    message = {"type": "response", "request_seq": request["seq"], "command": request["command"],
               "success": error is None}
    if error is not None:
        message["message"] = error
    if body is not None:
        message["body"] = body
    write(message)


def event(name, body=None):
    """Sends an event."""
    message = {"type": "event", "event": name}
    if body is not None:
        message["body"] = body
    write(message)


def read():
    """Reads one message, or None at the end of the input."""
    length = None
    while True:
        line = sys.stdin.buffer.readline()
        if not line:
            return None
        line = line.strip()
        if not line:
            break
        name, _, value = line.partition(b":")
        if name.strip().lower() == b"content-length":
            length = int(value.strip())
    return json.loads(sys.stdin.buffer.read(length))


def source():
    """The launched file, as a DAP Source."""
    return {"name": os.path.basename(state["program"]), "path": state["program"]}


def stop(reason, line, **extra):
    """Stops on a line, in main or helper."""
    state["line"] = line
    state["helper"] = line in HELPER_LINES
    body = {"reason": reason, "threadId": 1, "allThreadsStopped": True}
    body.update(extra)
    event("stopped", body)


def run_from(line):
    """Runs from a line to the next breakpoint, the exception, or the end."""
    for at in list(MAIN_LINES)[line - 1:]:
        if at in state["breakpoints"]:
            return stop("breakpoint", at, hitBreakpointIds=[state["breakpoints"][at]])
        if at == RAISE_LINE and "raised" in state["filters"]:
            return stop("exception", at, description="ValueError: bad value", text="ValueError")
    event("output", {"category": "stdout", "output": "done\n"})
    event("exited", {"exitCode": 0})
    event("terminated")


def set_breakpoints(request):
    """Verifies breakpoints on the main lines (and helper's); the others aren't."""
    state["breakpoints"] = {}
    answers = []
    for index, wanted in enumerate(request["arguments"].get("breakpoints", [])):
        line = wanted["line"]
        identity = index + 1
        if line in MAIN_LINES or line in HELPER_LINES:
            state["breakpoints"][line] = identity
            answers.append({"id": identity, "verified": True, "line": line, "source": source()})
        else:
            answers.append({"id": identity, "verified": False, "line": line, "message": "no code on this line"})
    respond(request, {"breakpoints": answers})


def stack_trace(request):
    """The frames: helper (if in it), main, and a library frame outside the folder."""
    frames = []
    if state["helper"]:
        frames.append({"id": 2, "name": "helper", "line": state["line"], "column": 1, "source": source()})
    frames.append({"id": 1, "name": "main", "line": CALL_LINE if state["helper"] else state["line"], "column": 1,
                   "source": source()})
    frames.append({"id": 3, "name": "<module>", "line": 40, "column": 1, "presentationHint": "subtle",
                   "source": {"name": "runtime.py", "path": LIBRARY}})
    respond(request, {"stackFrames": frames, "totalFrames": len(frames)})


def variables(request):
    """Locals (1001, 1002), globals (2000) and the paged children of `items` (3000)."""
    arguments = request["arguments"]
    reference = arguments["variablesReference"]
    if reference in (1001, 1002):
        answer = [{"name": name, "value": value, "variablesReference": 0}
                  for name, value in sorted(state["locals"].items())]
        answer.append({"name": "items", "value": "list[%d]" % state["items"], "variablesReference": 3000,
                       "indexedVariables": state["items"]})
    elif reference == 2000:
        answer = [{"name": "__name__", "value": "'__main__'", "variablesReference": 0}]
    elif reference == 3000:
        start = arguments.get("start", 0)
        count = arguments.get("count", state["items"])
        answer = [{"name": "[%d]" % index, "value": str(index * index), "variablesReference": 0}
                  for index in range(start, min(start + count, state["items"]))]
    else:
        return respond(request, error="no such variables")
    respond(request, {"variables": answer})


def evaluate(request):
    """Names of locals, sums of integers, and "hang"; anything else isn't defined."""
    expression = request["arguments"]["expression"].strip()
    if expression == "hang":
        state["hanging"] = request["seq"]
        return None
    if expression in state["locals"]:
        return respond(request, {"result": state["locals"][expression], "variablesReference": 0})
    parts = expression.split("+")
    if all(part.strip().isdigit() for part in parts):
        return respond(request, {"result": str(sum(int(part) for part in parts)), "variablesReference": 0})
    return respond(request, error="name '%s' is not defined" % expression)


def launch(request):
    """Remembers the program; asks for a terminal first when the console is the integrated one."""
    arguments = request["arguments"]
    state["program"] = arguments.get("program", "main.py")
    state["after_launch"] = arguments
    if arguments.get("console") == "integratedTerminal":
        write({"type": "request", "command": "runInTerminal",
               "arguments": {"kind": "integrated", "title": "fake", "cwd": arguments.get("cwd", "."),
                             "args": ["fake-run", state["program"]]}})
        state["pending"][state["seq"]] = request
        return
    finish_launch(request)


def finish_launch(request):
    """Answers the launch, then crashes or floods if asked to."""
    respond(request)
    arguments = state["after_launch"]
    event("output", {"category": "console", "output": "fake adapter: launching %s\n" % state["program"]})
    for index in range(arguments.get("flood", 0)):
        event("output", {"category": "stdout", "output": "line %d\n" % index})
    if "crash" in arguments:
        sys.exit(arguments["crash"])


def configuration_done(request):
    """Starts the program."""
    respond(request)
    if state["after_launch"].get("stopOnEntry"):
        stop("entry", 1)
    else:
        run_from(1)


def step(request, reason):
    """next, stepIn and stepOut, from the current line."""
    respond(request)
    line = state["line"]
    if request["command"] == "stepIn" and line == CALL_LINE and not state["helper"]:
        return stop(reason, HELPER_LINES[0])
    if state["helper"] and (request["command"] == "stepOut" or line == HELPER_LINES[-1]):
        return stop(reason, CALL_LINE + 1)
    if line + 1 in MAIN_LINES or line + 1 in HELPER_LINES:
        return stop(reason, line + 1)
    run_from(MAIN_LINES[-1] + 1)


def simple(request):
    """The requests with fixed answers; False if it isn't one."""
    command = request["command"]
    arguments = request.get("arguments") or {}
    answers = {
        "threads": {"threads": [{"id": 1, "name": "main"}]},
        "scopes": {"scopes": [
            {"name": "Locals", "variablesReference": 1000 + arguments.get("frameId", 1), "expensive": False},
            {"name": "Globals", "variablesReference": 2000, "expensive": True}]},
        "completions": {"targets": [{"label": name, "type": "variable"} for name in sorted(state["locals"])
                                    if name.startswith(arguments.get("text", ""))]},
        "exceptionInfo": {"exceptionId": "ValueError", "description": "bad value", "breakMode": "always"},
    }
    if command in answers:
        respond(request, answers[command])
    elif command == "setExceptionBreakpoints":
        state["filters"] = arguments.get("filters", [])
        respond(request, {"breakpoints": [{"verified": True} for _ in state["filters"]]})
    elif command == "setVariable":
        state["locals"][arguments["name"]] = arguments["value"]
        respond(request, {"value": arguments["value"]})
    elif command == "continue":
        respond(request, {"allThreadsContinued": True})
        run_from(state["line"] + 1)
    elif command == "pause":
        respond(request)
        stop("pause", state["line"] or 1)
    elif command == "restart":
        respond(request)
        run_from(1)
    elif command == "cancel":
        if state["hanging"] == arguments.get("requestId"):
            respond({"seq": state["hanging"], "command": "evaluate"}, error="cancelled")
            state["hanging"] = None
        respond(request)
    else:
        return False
    return True


def main():
    """Answers requests until disconnected or the input ends."""
    handlers = {
        "initialize": lambda request: (respond(request, {
            "supportsConfigurationDoneRequest": True, "supportsConditionalBreakpoints": True,
            "supportsHitConditionalBreakpoints": True, "supportsLogPoints": True, "supportsSetVariable": True,
            "supportsCompletionsRequest": True, "supportsCancelRequest": True, "supportsRestartRequest": True,
            "supportsExceptionInfoRequest": True, "supportsEvaluateForHovers": True,
            "exceptionBreakpointFilters": [{"filter": "raised", "label": "Raised Exceptions"},
                                           {"filter": "uncaught", "label": "Uncaught Exceptions", "default": True}]}),
            event("initialized")),
        "launch": launch,
        "setBreakpoints": set_breakpoints,
        "configurationDone": configuration_done,
        "stackTrace": stack_trace,
        "variables": variables,
        "evaluate": evaluate,
        "next": lambda request: step(request, "step"),
        "stepIn": lambda request: step(request, "step"),
        "stepOut": lambda request: step(request, "step"),
    }
    while True:
        message = read()
        if message is None:
            return
        if message["type"] == "response":
            launched = state["pending"].pop(message["request_seq"], None)
            if launched is not None:
                if message["success"]:
                    event("process", {"name": state["program"], "systemProcessId": (message.get("body") or {}).get("processId")})
                finish_launch(launched)
            continue
        command = message["command"]
        if command == "disconnect":
            respond(message)
            return
        if command in handlers:
            handlers[command](message)
        elif not simple(message):
            respond(message, error="unknown request '%s'" % command)


if __name__ == "__main__":
    main()
