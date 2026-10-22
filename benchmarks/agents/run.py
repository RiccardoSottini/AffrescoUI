#!/usr/bin/env python3
"""The agent benchmark (benchmarks/agents/README.md): each task's request given to a client that changes a running
screen through the MCP server, then the task's scenarios played on the result and its untouched nodes compared.

    python3 benchmarks/agents/run.py --client oracle                    # the reference revisions: all must pass
    python3 benchmarks/agents/run.py --client claude-code --model M     # Claude Code, headless, with an MCP config
    python3 benchmarks/agents/run.py --only kanban-                     # the tasks whose ids start so
    python3 benchmarks/agents/run.py --markdown results/*.json          # the "Agents" table of BENCHMARKS.md

Per task: a host (`affrescoui run`, mode acceptEdits) on a socket folder of its own, a reviewer on its studio.sock that
accepts every proposal (the changes rules send to review, e.g. to actions), the client connected through
mcp/affrescoui_mcp.py, then `affrescoui test --document` on the screen as the host has it. A task passes when its
scenarios pass, every node it lists as unchanged is unchanged and, for the oracle, its scenarios failed before (so they
test the request). Recorded: success, tokens in and out, tool calls, revisions and time, into one JSON per run. API
keys are the client's business, from the environment: nothing of the environment is written, and the client's output
is read for its counts only.
"""

import argparse
import datetime
import glob
import importlib.util
import json
import os
import platform
import shutil
import socket
import statistics
import subprocess
import sys
import tempfile
import threading
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
MCP = os.path.join(ROOT, "mcp", "affrescoui_mcp.py")
LEVELS = ("edit", "feature", "forge")
DENIED = "Bash,Edit,Write,NotebookEdit,Read,Glob,Grep,WebFetch,WebSearch,Task"  # the screen is changed through MCP


def load_mcp():
    """The MCP server's module, imported without running it (for locate and compact)."""
    spec = importlib.util.spec_from_file_location("affrescoui_mcp", MCP)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def load_tasks(only):
    """The tasks, by id, those whose ids start with one of only's prefixes (comma-separated) if given."""
    tasks = []
    for path in sorted(glob.glob(os.path.join(HERE, "tasks", "*.json"))):
        with open(path, encoding="utf-8") as file:
            task = json.load(file)
        if task["level"] not in LEVELS:
            sys.exit(f"run.py: {path}: level {task['level']!r} is not one of {', '.join(LEVELS)}")
        if not only or task["id"].startswith(tuple(only.split(","))):
            tasks.append(task)
    return tasks


class Lines:
    """Newline-delimited JSON over a socket or a pipe: send, and wait for the answer carrying an id."""

    def __init__(self, reader, writer):
        self.reader, self.writer, self.next = reader, writer, 0
        self.lock = threading.Lock()

    def request(self, message, deadline=30):
        """Sends a message with a new id; its answer, the other messages read meanwhile dropped."""
        with self.lock:
            self.next += 1
            self.writer.write((json.dumps({**message, "id": self.next}) + "\n").encode())
            self.writer.flush()
            end = time.monotonic() + deadline
            while time.monotonic() < end:
                line = self.reader.readline()
                if not line:
                    raise RuntimeError("the connection closed")
                answer = json.loads(line)
                if answer.get("id") == self.next:
                    return answer
            raise RuntimeError("no answer")


class Host:
    """`affrescoui run` on a screen, its socket and data folders in a private folder; a reviewer on its studio.sock."""

    def __init__(self, affrescoui, screen, folder):
        self.sockets = os.path.join(folder, "s")
        os.makedirs(self.sockets, mode=0o700)
        environment = {**os.environ, "AFFRESCOUI_SOCKET_DIR": self.sockets}
        self.process = subprocess.Popen([affrescoui, "run", screen, "--socket", self.sockets, "--data",
                                         os.path.join(folder, "d"), "--mode", "acceptEdits"], env=environment,
                                        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        end = time.monotonic() + 30
        while not os.path.exists(os.path.join(self.sockets, "studio.sock")):
            if time.monotonic() > end or self.process.poll() is not None:
                self.stop()
                raise RuntimeError(f"the host didn't start on {os.path.basename(screen)}")
            time.sleep(0.05)
        self.studio = socket.socket(socket.AF_UNIX)
        self.studio.connect(os.path.join(self.sockets, "studio.sock"))
        stream = self.studio.makefile("rwb")
        self.reviewer = Lines(stream, stream)
        self.accepted = 0
        self.stopping = threading.Event()
        self.thread = threading.Thread(target=self.review, daemon=True)
        self.thread.start()

    def document(self):
        """The host's answer to getDocument: the document, the live state, the head and the open proposals."""
        return self.reviewer.request({"type": "getDocument"})

    def review(self):
        """The reviewer: accepts each open proposal, as it is, until the host stops."""
        while not self.stopping.wait(0.1):
            try:
                for proposal in self.document().get("proposals") or []:
                    self.reviewer.request({"type": "accept", "proposal": proposal["id"],
                                           "head": proposal.get("head")})
                    self.accepted += 1
            except (OSError, RuntimeError, ValueError):
                return

    def settled(self, deadline=10):
        """The host's document once no proposal is open (the reviewer accepted them all), or as it is at the deadline."""
        end = time.monotonic() + deadline
        while True:
            reply = self.document()
            if not reply.get("proposals") or time.monotonic() > end:
                return reply
            time.sleep(0.1)

    def stop(self):
        """Stops the reviewer and the host."""
        if hasattr(self, "stopping"):
            self.stopping.set()
            self.studio.close()
        self.process.terminate()
        try:
            self.process.wait(10)
        except subprocess.TimeoutExpired:
            self.process.kill()


class McpClient:
    """mcp/affrescoui_mcp.py on the host's socket folder, spoken to as an MCP client does over stdio."""

    def __init__(self, sockets):
        self.process = subprocess.Popen([sys.executable, MCP, "--socket", sockets], stdin=subprocess.PIPE,
                                        stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
        self.lines = Lines(self.process.stdout, self.process.stdin)
        self.lines.request({"jsonrpc": "2.0", "method": "initialize",
                            "params": {"protocolVersion": "2025-06-18", "capabilities": {},
                                       "clientInfo": {"name": "oracle", "version": "1"}}})
        self.process.stdin.write(b'{"jsonrpc":"2.0","method":"notifications/initialized"}\n')
        self.process.stdin.flush()

    def call(self, name, arguments):
        """Calls a tool: its text and whether it is an error."""
        answer = self.lines.request({"jsonrpc": "2.0", "method": "tools/call",
                                     "params": {"name": name, "arguments": arguments}}, deadline=120)
        result = answer.get("result") or {}
        text = "".join(block.get("text", "") for block in result.get("content", []))
        return text, bool(result.get("isError")) or "error" in answer

    def close(self):
        """Ends the server."""
        self.process.stdin.close()
        self.process.wait(10)


def run_oracle(task, host, timeout):
    """The oracle: submits the task's reference revisions, as an agent would, and nothing else."""
    client = McpClient(host.sockets)
    try:
        for arguments in task["reference"]:
            text, failed = client.call("submit_revision", arguments)
            if failed or text.startswith(("rejected", "error")):
                raise RuntimeError(f"its reference revision was refused: {text[:300]}")
            host.settled()
    finally:
        client.close()
    return {"toolCalls": len(task["reference"]), "tokensIn": None, "tokensOut": None, "model": None}


def claude_prompt(task):
    """What Claude Code is asked: the request, and where to make it."""
    return ("A person asks for a change to the AffrescoUI screen that is running now. Make it with the affrescoui "
            "MCP tools (read the screen, then submit revisions), and stop when it is done.\n\nThe request: "
            + task["request"])


def run_claude(task, host, timeout, model):
    """Claude Code, headless (claude -p), with only the affrescoui MCP server: its counts from its stream's events."""
    config = os.path.join(os.path.dirname(host.sockets), "mcp.json")
    with open(config, "w", encoding="utf-8") as file:
        json.dump({"mcpServers": {"affrescoui": {"command": sys.executable,
                                                 "args": [MCP, "--socket", host.sockets]}}}, file)
    command = ["claude", "-p", claude_prompt(task), "--output-format", "stream-json", "--verbose", "--mcp-config",
               config, "--strict-mcp-config", "--allowedTools", "mcp__affrescoui", "--disallowedTools", DENIED]
    if model:
        command += ["--model", model]
    process = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True, timeout=timeout,
                             cwd=os.path.dirname(host.sockets))
    counts = {"toolCalls": 0, "tokensIn": None, "tokensOut": None, "model": model}
    for line in filter(str.strip, process.stdout.splitlines()):
        event = json.loads(line)
        if event.get("type") == "system" and event.get("model"):
            counts["model"] = event["model"]
        elif event.get("type") == "assistant":
            content = (event.get("message") or {}).get("content") or []
            counts["toolCalls"] += sum(1 for block in content if block.get("type") == "tool_use")
        elif event.get("type") == "result":
            usage = event.get("usage") or {}
            counts["tokensIn"] = sum(usage.get(key) or 0 for key in (
                "input_tokens", "cache_creation_input_tokens", "cache_read_input_tokens"))
            counts["tokensOut"] = usage.get("output_tokens")
    host.settled()
    return counts


def as_played(reply):
    """The screen as the host has it, the live state as its state (what get_screen shows an agent)."""
    document = dict(reply["document"])
    document["state"] = reply.get("state")
    return document


def play(affrescoui, task, document, folder, name):
    """Plays the task's scenarios on a document with `affrescoui test --document`: (passed, failures)."""
    screen = os.path.join(ROOT, task["screen"])
    scenarios = os.path.join(folder, name + ".scenarios.json")
    played = os.path.join(folder, name + ".screen.json")
    with open(scenarios, "w", encoding="utf-8") as file:
        json.dump({"format": "affrescoui-scenarios", "version": 1, "screens": os.path.dirname(screen),
                   "screen": os.path.basename(screen), "scenarios": task["scenarios"]}, file)
    with open(played, "w", encoding="utf-8") as file:
        json.dump(document, file)
    process = subprocess.run([affrescoui, "test", scenarios, "--json", "--document", played], stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE, text=True, timeout=300)
    reports = [json.loads(line) for line in process.stdout.splitlines() if line.startswith("{")]
    if not reports:
        return False, [process.stderr.strip()[:300] or f"affrescoui test exited with {process.returncode}"]
    failures = [f"{failure.get('scenario')}, step {failure.get('step')}: {failure.get('problem')}"
                for failure in reports[0].get("failures", [])]
    return reports[0].get("failed", 1) == 0 and process.returncode == 0, failures


def changed_nodes(mcp, task, before, after):
    """The addresses the task says must not change that did (or that went away)."""
    changed = []
    for address in task.get("unchanged", []):
        try:
            if mcp.locate(before, address)[1] != mcp.locate(after, address)[1]:
                changed.append(address)
        except ValueError:
            changed.append(address)
    return changed


def run_task(task, args, mcp):
    """One task from a fresh host: the client's counts, the checks and the time, as a result."""
    folder = tempfile.mkdtemp(prefix="aui-")  # short: a socket's path is at most 104 bytes
    affrescoui = os.path.join(args.build, "affrescoui")
    host = None
    result = {"id": task["id"], "level": task["level"], "success": False}
    try:
        host = Host(affrescoui, os.path.join(ROOT, task["screen"]), folder)
        start = host.document()
        failed_before = not play(affrescoui, task, as_played(start), folder, "before")[0]
        began = time.monotonic()
        run = run_oracle if args.client == "oracle" else run_claude
        counts = run(task, host, args.timeout, *([args.model] if args.client == "claude-code" else []))
        seconds = time.monotonic() - began
        end = host.settled()
        passed, failures = play(affrescoui, task, as_played(end), folder, "after")
        changed = changed_nodes(mcp, task, start["document"], end["document"])
        result.update(counts, seconds=round(seconds, 2), revisions=end["revision"] - start["revision"],
                      scenariosPassed=passed, failures=failures, changed=changed, failedBefore=failed_before)
        result["success"] = passed and not changed and (failed_before or args.client != "oracle")
    except (RuntimeError, OSError, ValueError, subprocess.TimeoutExpired) as failure:
        result["error"] = str(failure)[:300]
    finally:
        if host:
            host.stop()
        shutil.rmtree(folder, ignore_errors=True)
    return result


def client_version(client):
    """The client's version: Claude Code's own line, or the oracle's."""
    if client == "oracle":
        return "oracle 1"
    process = subprocess.run(["claude", "--version"], stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True)
    return process.stdout.strip()


def machine():
    """The machine, in a few words: its processor and system, nothing that names a person."""
    if sys.platform == "darwin":
        chip = subprocess.run(["sysctl", "-n", "machdep.cpu.brand_string"], stdout=subprocess.PIPE, text=True)
        return f"{chip.stdout.strip()}, macOS {platform.mac_ver()[0]}"
    return f"{platform.machine()}, {platform.system()} {platform.release()}"


def commit():
    """The commit measured, short, marked when the checkout has changes."""
    head = subprocess.run(["git", "-C", ROOT, "rev-parse", "--short", "HEAD"], stdout=subprocess.PIPE, text=True)
    dirty = subprocess.run(["git", "-C", ROOT, "status", "--porcelain", "--untracked-files=no"],
                           stdout=subprocess.PIPE, text=True)
    return head.stdout.strip() + (" (with changes)" if dirty.stdout.strip() else "")


def summary(results):
    """The run's totals: tasks, passed, success rate and the medians (of the tasks that have them)."""

    def median(key):
        values = [result[key] for result in results if result.get(key) is not None]
        return round(statistics.median(values), 3) if values else None

    passed = sum(result["success"] for result in results)
    by_level = {level: [r["success"] for r in results if r["level"] == level] for level in LEVELS}
    return {"tasks": len(results), "passed": passed, "successRate": round(passed / len(results), 3) if results else None,
            "byLevel": {level: f"{sum(found)}/{len(found)}" for level, found in by_level.items() if found},
            "medianTokensIn": median("tokensIn"), "medianTokensOut": median("tokensOut"),
            "medianToolCalls": median("toolCalls"), "medianSeconds": median("seconds")}


def markdown(paths):
    """The "Agents" table of BENCHMARKS.md, one row per results file; "not run yet" without any."""
    rows = ["| Date | Client | Model | Versions | Machine | Success | Median tokens in / out | Median tool calls |",
            "|---|---|---|---|---|---:|---:|---:|"]
    for path in sorted(paths):
        with open(path, encoding="utf-8") as file:
            run = json.load(file)
        totals = run["summary"]
        tokens = " / ".join("n/a" if totals[key] is None else f"{totals[key]:,.0f}"
                            for key in ("medianTokensIn", "medianTokensOut"))
        rows.append(f"| {run['date']} | {run['client']} | {run['model'] or 'none'} | {run['clientVersion']}, "
                    f"AffrescoUI {run['commit']} | {run['machine']} | {totals['passed']}/{totals['tasks']} "
                    f"({', '.join(f'{k} {v}' for k, v in totals['byLevel'].items())}) | {tokens} | "
                    f"{totals['medianToolCalls']} |")
    if len(rows) == 2:
        rows.append("| not run yet | | | | | | | |")
    return "\n".join(rows) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--client", choices=("oracle", "claude-code"), default="oracle")
    parser.add_argument("--model", help="claude-code: the model (default: Claude Code's)")
    parser.add_argument("--only", help="the tasks whose ids start with one of these (comma-separated)")
    parser.add_argument("--build", default=os.path.join(ROOT, "build"), help="the build with affrescoui")
    parser.add_argument("--timeout", type=int, default=900, help="seconds a client has for one task")
    parser.add_argument("--out", help="the results file (default: results/<client>-<date>.json here)")
    parser.add_argument("--markdown", nargs="*", metavar="RESULTS", help="print the Agents table of these results")
    args = parser.parse_args()
    if args.markdown is not None:
        return sys.stdout.write(markdown(args.markdown))

    mcp, results = load_mcp(), []
    for task in load_tasks(args.only):
        results.append(run_task(task, args, mcp))
        verdict = "passed" if results[-1]["success"] else "FAILED"
        print(f"{verdict}  {task['id']}", *results[-1].get("failures", []), results[-1].get("error", ""), flush=True)
    today = datetime.date.today().isoformat()
    run = {"format": "affrescoui-agent-benchmark", "version": 1, "client": args.client,
           "model": next((r["model"] for r in results if r.get("model")), args.model),
           "clientVersion": client_version(args.client), "commit": commit(), "machine": machine(), "date": today,
           "summary": summary(results), "tasks": results}
    out = args.out or os.path.join(HERE, "results", f"{args.client}-{today}.json")
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    with open(out, "w", encoding="utf-8") as file:
        file.write(json.dumps(run, indent=2) + "\n")
    print(f"{run['summary']['passed']}/{run['summary']['tasks']} passed; {out}")
    return 0 if run["summary"]["passed"] == run["summary"]["tasks"] else 1


if __name__ == "__main__":
    sys.exit(main())
