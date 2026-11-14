#!/usr/bin/env python3
"""The conformance run (README.md): runs `affrescoui_conformance` over the Clojure test suite's files and sums them.

  run.py --check        the test: fails if the runner crashed or hung on a file, if a file that read before no longer
                        reads, or if a file passes fewer assertions than expected.json says; more passes, and says to
                        run --update
  run.py --update       rewrites expected.json and modules/lisp/CONFORMANCE.md
  run.py --report FILE  writes the whole result, one object per file, for a person

Each takes --runner BIN (the program) and --suite DIR (the suite's `test` folder), which default to where a build in
`build/` configured with -DAFFRESCOUI_CONFORMANCE=ON puts them.

Both files it writes are the same byte for byte from one run to the next.
"""

import argparse
import json
import pathlib
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent.parent
EXPECTED = HERE / "expected.json"
CONFORMANCE = ROOT / "modules" / "lisp" / "CONFORMANCE.md"
SHIM = HERE / "shim.lisp"
RUNNER = ROOT / "build" / "tests" / "conformance" / "affrescoui_conformance"
SUITE = ROOT / "build" / "_deps" / "clojure_test_suite-src" / "test"

# The suite's commit and the day it was the main branch's: as in CMakeLists.txt (README.md says how to bump both)
SUITE_COMMIT = "a9f32fd7bf0cf3ef04eb62db31a731bc7dcb6dc6"
SUITE_DATE = "2026-11-14"
SUITE_URL = "https://github.com/jank-lang/clojure-test-suite"

# How long the runner may take: over the whole suite, and over one file when a file is run alone
WHOLE_SECONDS = 120
FILE_SECONDS = 30

# The reasons an assertion is out of scope, in order, and the row of reference/differences.md each links to
REASONS = {
    "numbers": "numbers-are-jsons",
    "characters": "no-characters",
    "regex": "no-regular-expressions",
    "vars": "no-metadata-vars-protocols-records-or-multimethods",
    "mutation": "nothing-changes-a-value",
    "laziness": "nothing-is-lazy",
    "host": "no-host-interop-output-clock-or-randomness",
    "metadata": "no-metadata-vars-protocols-records-or-multimethods",
    "keys": "a-maps-keys-are-text",
    "namespaces": "one-flat-space-of-names-per-program",
}


def suite_files(suite):
    """The suite's test files, by their name under DIR, in a fixed order."""
    root = pathlib.Path(suite)
    return sorted(str(path.relative_to(root)) for path in root.glob("clojure/**/*.cljc"))


def run_runner(runner, suite, names, seconds):
    """Runs the runner on files; returns the objects it printed, by file, and whether it ended as it should."""
    command = [runner, "--shim", str(SHIM), "--suite", suite] + [str(pathlib.Path(suite) / name) for name in names]
    try:
        done = subprocess.run(command, capture_output=True, text=True, timeout=seconds, check=False)
    except subprocess.TimeoutExpired as expired:
        output = expired.stdout or ""
        if isinstance(output, bytes):
            output = output.decode("utf-8", "replace")
        return parse(output), False
    return parse(done.stdout), done.returncode == 0


def parse(output):
    """The objects in the runner's output, by file; a line cut short by a crash is left out."""
    results = {}
    for line in output.splitlines():
        try:
            result = json.loads(line)
        except json.JSONDecodeError:
            continue
        if isinstance(result, dict) and "file" in result:
            results[result["file"]] = result
    return results


def run_suite(runner, suite):
    """Every file's object. A file the runner crashed or hung on is run again alone, and is {"crashed": true} if it
    does so again."""
    names = suite_files(suite)
    results, ended = run_runner(runner, suite, names, WHOLE_SECONDS)
    if not ended or len(results) != len(names):
        for name in names:
            if name in results:
                continue
            alone, ended_alone = run_runner(runner, suite, [name], FILE_SECONDS)
            results[name] = alone.get(name) if ended_alone and name in alone else {"file": name, "crashed": True}
    return [results[name] for name in names]


def totals(results):
    """The sums over the files."""
    sums = {"files": len(results), "read": 0, "tested": 0, "defined": 0, "written": 0, "passed": 0, "failed": 0,
            "errors": 0, "outOfScope": 0, "notRun": 0, "exhausted": 0, "crashed": 0, "out": {}}
    for result in results:
        if result.get("crashed"):
            sums["crashed"] += 1
            continue
        if result.get("exhausted"):
            sums["exhausted"] += 1
        if result.get("tested"):
            sums["tested"] += 1
        for key in ("read", "defined"):
            sums[key] += 1 if result.get(key) else 0
        for key in ("written", "passed", "failed", "errors", "outOfScope", "notRun"):
            sums[key] += result.get(key, 0)
        for reason, count in result.get("out", {}).items():
            sums["out"][reason] = sums["out"].get(reason, 0) + count
    return sums


def expected_of(results):
    """What expected.json holds: per file, whether it read and how many assertions passed."""
    files = {}
    for result in results:
        files[result["file"]] = {"read": bool(result.get("read")), "passed": result.get("passed", 0)}
    return {"suite": SUITE_COMMIT, "files": files}


def check(results):
    """The test: the problems against expected.json, and whether any file does better."""
    expected = json.loads(EXPECTED.read_text(encoding="utf-8"))["files"] if EXPECTED.exists() else {}
    problems = []
    better = False
    for result in results:
        name = result["file"]
        if result.get("crashed"):
            problems.append(f"{name}: the runner crashed or hung on it")
            continue
        before = expected.get(name, {"read": False, "passed": 0})
        if before["read"] and not result.get("read"):
            problems.append(f"{name}: it read before, and doesn't now: {result.get('error', '')}")
        passed = result.get("passed", 0)
        if passed < before["passed"]:
            problems.append(f"{name}: {passed} assertions pass, {before['passed']} did")
        if passed > before["passed"] or (result.get("read") and not before["read"]):
            better = True
    return problems, better


def link(reason):
    """A reason, linked to its row of differences.md."""
    return f"[{reason}](reference/differences.md#{REASONS[reason]})"


def function_of(result):
    """The function a file tests, as code, or its name when it tests none."""
    tested = result.get("tested", "")
    return f"`{tested}`" if tested else "—"


def conformance_page(results):
    """The text of CONFORMANCE.md."""
    sums = totals(results)
    lines = [
        "# Conformance",
        "",
        "<!-- Written by tests/conformance/run.py --update: don't edit by hand. -->",
        "",
        f"The language measured against the [cross-dialect Clojure test suite]({SUITE_URL}) (MPL-2.0), at commit "
        f"`{SUITE_COMMIT[:8]}` (its main branch on {SUITE_DATE}). The suite is fetched by its checksum to run, never "
        "copied here; how to run it is in [tests/conformance/README.md](../../tests/conformance/README.md), and "
        "what differs on purpose in [reference/differences.md](reference/differences.md).",
        "",
        "## Totals",
        "",
        "| | |",
        "|---|---|",
        f"| Files read | {sums['read']} of {sums['files']} |",
        f"| Functions defined | {sums['defined']} of the {sums['tested']} the suite tests |",
        f"| Assertions passed | {sums['passed']} |",
        f"| Assertions failed | {sums['failed']} |",
        f"| Assertions that raised an error | {sums['errors']} |",
        f"| Assertions out of scope | {sums['outOfScope']} |",
        f"| Assertions not run | {sums['notRun']} |",
        f"| Files past their budget | {sums['exhausted']} |",
        "",
        "Read them so: an assertion **passed** when the language gave the value Clojure gives; it **failed** when it "
        "gave another, or **raised an error** (a missing function, a wrong argument, a form the language refuses); it "
        "is **out of scope** when it uses something the language leaves out on purpose (counted without running it); "
        "it was **not run** when its function isn't defined, or the form around it stopped before it. Errors also count "
        "the top-level forms and tests that stopped.",
        "",
        "## Out of scope, by reason",
        "",
        "| Reason | Assertions |",
        "|---|---|",
    ]
    for reason in REASONS:
        if reason in sums["out"]:
            lines.append(f"| {link(reason)} | {sums['out'][reason]} |")
    lines += ["", "## Failing today", "",
              "The files with assertions that failed or raised an error, most first: the gaps, in the open.", "",
              "| File | Function | Failed | Errors | Passed |", "|---|---|---|---|---|"]
    failing = [r for r in results if not r.get("crashed") and r.get("failed", 0) + r.get("errors", 0) > 0]
    failing.sort(key=lambda r: (-(r.get("failed", 0) + r.get("errors", 0)), r["file"]))
    for result in failing:
        lines.append(f"| `{result['file']}` | {function_of(result)} | {result.get('failed', 0)} | "
                     f"{result.get('errors', 0)} | {result.get('passed', 0)} |")
    lines += ["", "## Every file", "", "| File | Function | Defined | Passed | Failed | Errors | Out of scope |",
              "|---|---|---|---|---|---|---|"]
    for result in results:
        if result.get("crashed"):
            lines.append(f"| `{result['file']}` | | crashed | | | | |")
        elif result.get("exhausted"):
            lines.append(f"| `{result['file']}` | {function_of(result)} | past its budget | | | | |")
        else:
            defined = "yes" if result.get("defined") else ("no" if result.get("tested") else "—")
            if not result.get("read"):
                defined = "doesn't read"
            lines.append(f"| `{result['file']}` | {function_of(result)} | {defined} | {result.get('passed', 0)} | "
                         f"{result.get('failed', 0)} | {result.get('errors', 0)} | {result.get('outOfScope', 0)} |")
    return "\n".join(lines) + "\n"


def summary(results):
    """One line of totals, for the test's output."""
    sums = totals(results)
    return (f"{sums['read']}/{sums['files']} files read, {sums['defined']}/{sums['tested']} functions defined; "
            f"{sums['passed']} passed, {sums['failed']} failed, {sums['errors']} errors, {sums['outOfScope']} out of "
            f"scope, {sums['notRun']} not run; {sums['exhausted']} past their budget, {sums['crashed']} crashed")


def main():
    """Runs as asked: see the module's text."""
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--runner", default=str(RUNNER), help="the affrescoui_conformance program")
    parser.add_argument("--suite", default=str(SUITE), help="the suite's test folder")
    action = parser.add_mutually_exclusive_group(required=True)
    action.add_argument("--check", action="store_true")
    action.add_argument("--update", action="store_true")
    action.add_argument("--report", metavar="FILE")
    args = parser.parse_args()

    results = run_suite(args.runner, args.suite)
    print(summary(results))
    if args.report:
        pathlib.Path(args.report).write_text("".join(json.dumps(r, sort_keys=True) + "\n" for r in results),
                                             encoding="utf-8")
        return 0
    if args.update:
        EXPECTED.write_text(json.dumps(expected_of(results), indent=1, sort_keys=True) + "\n", encoding="utf-8")
        CONFORMANCE.write_text(conformance_page(results), encoding="utf-8")
        return 0
    problems, better = check(results)
    for problem in problems:
        print(problem)
    if problems:
        return 1
    if better:
        print("more assertions pass than expected.json says: run --update")
    return 0


if __name__ == "__main__":
    sys.exit(main())
