#!/usr/bin/env python3
"""Smoke test: every application opens the way a person starts it, and stays open.

Usage:
    python3 tests/smoke/open_apps.py [--build DIR] [--only NAME] [--seconds N] [--keep DIR]

Where tests/smoke/first_frames.py draws each screen's first frame offscreen, this starts each program as a person does:
in a window, from the repository root and from another folder, with no arguments where the README says so, with the
default socket and with `--socket DIR`, with a fresh data folder (`HOME` an empty folder) and with a history left by an
older version of the screen (a history the screen no longer matches, and one that is damaged). Each run lasts
`AFFRESCOUI_SCREENSHOT_AFTER` (5 s) and must then exit 0 by itself, print no error, and leave a frame that is drawn
(at least MIN_COLOURS colours). A service without a window (`affrescoui_server`) must still be running after that long,
a command without one (`affrescoui_bottega`) must have exited 0.
A program that isn't built (an option turned off) is skipped
with a note. On Linux it runs under Xvfb (.github/workflows/ci.yml). Only the Python standard library is needed.
"""

import concurrent.futures
import glob
import os
import re
import shutil
import socket
import subprocess
import sys
import tempfile
import time

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
MIN_COLOURS = 16  # a drawn window has text and controls: a blank or single-colour frame has far fewer
GRACE_SECONDS = 60  # beyond its run, how long a program may take to start, save its frame and exit
# A line that a program prints on stderr (where warnings and errors go; information goes to stdout) and that says
# something went wrong. Informational lines (the screenshot saved, where the host listens) don't match.
ERROR = re.compile(r"error|fail|isn't|can't|cannot|couldn't|unable|denied|invalid|abort|exception|warning|not found",
                   re.IGNORECASE)
SAVED = re.compile(r"screenshot saved|\b0 failed\b")  # the frame saved, a summary of runs that all passed

# Each program as its README starts it: its name, its arguments (paths relative to the repository root) and its kind:
# WINDOW (draws a frame and exits after the run), SERVICE (no window, still running after the run) or COMMAND (no
# window, does its work and exits 0). The host also opens every examples/*/*.screen.json and every app, by its
# app.json (added in programs()).
WINDOW, SERVICE, COMMAND = "window", "service", "command"
PROGRAMS = [
    ("host", "affrescoui_launcher", [], WINDOW),
    ("forge", "affrescoui_forge", [], WINDOW),
    ("tempera", "affrescoui_tempera", [], WINDOW),
    ("sinopia", "affrescoui_sinopia", [], WINDOW),
    ("bottega", "affrescoui_bottega", [], COMMAND),  # plays Bottega's scenarios headless
    ("surfaces", "affrescoui_surfaces", [], WINDOW),
    ("embed", "affrescoui_embed", [], WINDOW),
    ("embed-texture", "affrescoui_embed", ["--texture"], WINDOW),
    ("server", "affrescoui_server", ["--folder", "{data}"], SERVICE),
]
# What a program's README runs first, once, in the same folders (its output isn't checked)
FIRST = {"server": ["--folder", "{data}", "--add-token", "ada"]}


def programs(build):
    """Every program to open, (name, executable, arguments, kind), with the host once per example screen and once per
    app."""
    found = []
    for name, executable, arguments, kind in PROGRAMS:
        path = os.path.join(build, executable)
        if os.path.exists(path):
            found.append((name, path, arguments, kind))
        else:
            print(f"skipped {name}: {executable} isn't built")
    host = os.path.join(build, "affrescoui_launcher")
    for screen in sorted(glob.glob(os.path.join(ROOT, "examples", "*", "*.screen.json"))):
        name = "host " + os.path.relpath(screen, os.path.join(ROOT, "examples"))
        found.append((name, host, [os.path.relpath(screen, ROOT)], WINDOW))
    for app in sorted(glob.glob(os.path.join(ROOT, "examples", "*", "app.json"))):
        name = "host " + os.path.relpath(app, os.path.join(ROOT, "examples"))
        found.append((name, host, [os.path.relpath(app, ROOT)], WINDOW))
    return found


def colours_of_ppm(path):
    """How many distinct colours a binary PPM (P6) frame has, or None when it can't be read."""
    with open(path, "rb") as file:
        data = file.read()
    fields = re.match(rb"P6\s+(\d+)\s+(\d+)\s+(\d+)\s", data)
    if not fields:
        return None
    pixels = data[fields.end():]
    return len({pixels[i:i + 3] for i in range(0, len(pixels) - 2, 3 * 7)})  # every 7th pixel is enough to tell


def stale_history(home, build, screen):
    """Leaves in `home` the history an older version of `screen` (same file name, other content) left behind."""
    if not screen:
        return
    older = os.path.join(home, "older")
    os.makedirs(older)
    copy = os.path.join(older, os.path.basename(screen))
    with open(os.path.join(ROOT, screen), encoding="utf-8") as file:
        text = file.read()
    # An older version: the same file, one string changed, so main's head no longer matches the file
    with open(copy, "w", encoding="utf-8") as file:
        file.write(text.replace('"title"', '"title_"', 1) if '"title"' in text else text + "\n")
    environment = environment_for(home)
    environment["AFFRESCOUI_SCREENSHOT"] = os.path.join(older, "frame.ppm")
    environment["AFFRESCOUI_SCREENSHOT_AFTER"] = "500"
    socket = tempfile.mkdtemp(prefix="ao", dir="/tmp")
    try:
        subprocess.run([os.path.join(build, "affrescoui_launcher"), copy, "--socket", socket], env=environment, cwd=older,
                       capture_output=True, timeout=GRACE_SECONDS, check=False)
    finally:
        shutil.rmtree(socket, ignore_errors=True)


def damaged_history(home):
    """Damages every refs.json under `home`'s data folder, as a crash mid-write of an older version might."""
    for refs in glob.glob(os.path.join(home, ".affrescoui", "*.history", "refs.json")):
        with open(refs, "w", encoding="utf-8") as file:
            file.write('{"version": 0, "main": "0000"')


def environment_for(home):
    """The environment a person's shell gives a program, with `home` as its home and no data folder of our own."""
    environment = {k: v for k, v in os.environ.items() if not k.startswith("AFFRESCOUI_")}
    environment["HOME"] = home
    return environment


def run(program, build, way, seconds, keep):
    """Opens one program one way; returns (name, way, problems)."""
    name, executable, arguments, kind = program
    home = tempfile.mkdtemp(prefix="open-apps-")
    socket = tempfile.mkdtemp(prefix="ao", dir="/tmp")  # short: a socket path has a length limit
    elsewhere = tempfile.mkdtemp(prefix="elsewhere-")
    problems = []
    try:
        screen = arguments[0] if arguments and arguments[0].endswith(".screen.json") else None
        if way in ("older history", "damaged history"):
            stale_history(home, build, screen)
            if way == "damaged history":
                damaged_history(home)
        cwd = elsewhere if way == "from another folder" else ROOT
        def placed(given):
            given = [os.path.join(ROOT, a) if a.endswith(".json") else a for a in given]
            return [a.replace("{data}", os.path.join(home, "served")) for a in given]
        absolute = placed(arguments)
        if name in FIRST:
            subprocess.run([executable] + placed(FIRST[name]), cwd=cwd, env=environment_for(home), capture_output=True,
                           timeout=GRACE_SECONDS, check=False)
        if way != "default socket" and executable.endswith(("launcher", "forge", "modello", "tempera", "sinopia")):
            absolute += ["--socket", socket]
        if way != "default socket" and executable.endswith("server"):
            absolute += ["--listen", f"127.0.0.1:{free_port()}"]  # runs side by side; the default way keeps 7420
        frame = os.path.join(home, "frame.ppm")
        environment = environment_for(home)
        if kind == WINDOW:
            environment["AFFRESCOUI_SCREENSHOT"] = frame
            environment["AFFRESCOUI_SCREENSHOT_AFTER"] = str(seconds * 1000)
        problems = watch([executable] + absolute, cwd, environment, kind, seconds)
        if kind == WINDOW and not problems:
            problems = look(frame, name, way, keep)
    finally:
        for folder in (home, socket, elsewhere):
            shutil.rmtree(folder, ignore_errors=True)
    return name, way, problems


def free_port():
    """Returns a TCP port on 127.0.0.1 that nothing listens on now."""
    with socket.socket() as probe:
        probe.bind(("127.0.0.1", 0))
        return probe.getsockname()[1]


def watch(command, cwd, environment, kind, seconds):
    """Runs a command; a window or a command must exit 0 by itself, a service must still run, and none may print a
    warning or an error. Returns problems."""
    process = subprocess.Popen(command, cwd=cwd, env=environment, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE,
                               text=True, errors="replace")
    problems = []
    try:
        _, output = process.communicate(timeout=seconds + (0 if kind == SERVICE else GRACE_SECONDS))
        if kind == SERVICE:
            problems.append(f"exited with {process.returncode} before {seconds} s")
        elif process.returncode != 0:
            problems.append(f"exited with {process.returncode}")
    except subprocess.TimeoutExpired:
        process.terminate()
        try:
            _, output = process.communicate(timeout=10)
        except subprocess.TimeoutExpired:
            process.kill()
            _, output = process.communicate()
        if kind != SERVICE:
            problems.append(f"still running {seconds + GRACE_SECONDS} s later")
    problems += [f"printed: {line.strip()}" for line in output.splitlines() if ERROR.search(line) and
                 not SAVED.search(line)]
    return problems


def look(frame, name, way, keep):
    """Checks that a run left its frame and that it is drawn; copies it into `keep` when given. Returns problems."""
    if not os.path.exists(frame):
        return ["left no frame"]
    colours = colours_of_ppm(frame)
    if keep:
        os.makedirs(keep, exist_ok=True)
        shutil.copy(frame, os.path.join(keep, re.sub(r"[^\w.-]+", "-", f"{name} {way}") + ".ppm"))
    if colours is None or colours < MIN_COLOURS:
        return [f"its frame is blank ({colours} colours)"]
    return []


def ways(program):
    """The ways a program is opened: every program from the root and from elsewhere, with the default socket and its
    own, fresh; the ones that keep a screen's history also with an older one and a damaged one."""
    name, _, arguments, kind = program
    found = ["from the root", "from another folder"]
    if kind == WINDOW and (not arguments or arguments[0].endswith(".screen.json")):
        found.append("default socket")
    if arguments and arguments[0].endswith(".screen.json"):
        found += ["older history", "damaged history"]
    return found


def main():
    """Opens every program every way; exits with 1 if one didn't open."""
    args, build, only, seconds, keep = sys.argv[1:], "build", None, 5, None
    while args:
        if len(args) < 2 or args[0] not in ("--build", "--only", "--seconds", "--keep"):
            sys.exit(__doc__)
        option, value, args = args[0], args[1], args[2:]
        if option == "--build":
            build = value
        elif option == "--only":
            only = value
        elif option == "--seconds":
            seconds = int(value)
        else:
            keep = os.path.abspath(value)
    build = os.path.abspath(build)
    if not os.path.exists(os.path.join(build, "affrescoui_launcher")):
        sys.exit(f"{os.path.join(build, 'affrescoui_launcher')} not found: build first (cmake --build {build})")

    started, failed = time.monotonic(), 0
    runs = [(p, w) for p in programs(build) if not only or only in p[0] for w in ways(p)]
    # The default socket is one folder for every program, so those runs go one at a time, after the others
    shared = [r for r in runs if r[1] == "default socket"]
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        results = list(pool.map(lambda r: run(r[0], build, r[1], seconds, keep), [r for r in runs if r not in shared]))
    results += [run(p, build, w, seconds, keep) for p, w in shared]
    for name, way, problems in results:
        failed += bool(problems)
        print(("FAILED  " if problems else "opened  ") + f"{name}, {way}" +
              "".join(f"\n        {problem}" for problem in problems))
    print(f"{failed} failed of {len(results)}, in {time.monotonic() - started:.0f} s")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
