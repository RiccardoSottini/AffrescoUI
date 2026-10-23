#!/usr/bin/env python3
"""Quality gates and reports for AffrescoUI's C++ (tools/quality/README.md). run.sh sets up the tools and calls this.

Usage:
    quality.py gate --base COMMIT --build DIR --clang-tidy PATH [--jobs N]
    quality.py report --build DIR --clang-tidy PATH --output quality-report.json [--coverage coverage.json] [--jobs N]
    quality.py coverage --export llvm-cov-export.json --output coverage.json

gate fails (exit 1) on a clang-tidy finding in the changed lines, or on a function the change made too long or too
complex (thresholds.json). report and coverage never fail on what they find. Each command prints Markdown, and adds
it to the GitHub job summary when $GITHUB_STEP_SUMMARY is set; gate also prints GitHub annotations.
"""

import argparse
import json
import os
import subprocess
import sys
import tempfile

import changes
import coverage
import functions
import tidy

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
CPP = (".cpp", ".hpp")


THRESHOLDS = "tools/quality/thresholds.json"
TIDY_CONFIG = ".clang-tidy"
COVERAGE_BASELINE = "tools/quality/coverage-baseline.json"


def load_thresholds():
    """thresholds.json: the limits the gate enforces, the aims it reports, and the folders it looks at."""
    with open(os.path.join(ROOT, THRESHOLDS)) as file:
        return json.load(file)


def rule_file(root, base, path):
    """A file of rules (thresholds.json, .clang-tidy, the coverage floor) as it is at the base commit, so a change
    can't loosen the rules that judge it. Without a base, or when the base doesn't have the file yet (the change
    adds it), the working tree's. None if neither has it."""
    text = changes.file_at(root, base, path) if base else None
    if text is None and os.path.isfile(os.path.join(root, path)):
        with open(os.path.join(root, path), encoding="utf-8") as file:
            text = file.read()
    return text


def publish(text):
    """Prints Markdown, and appends it to the GitHub job summary when there is one."""
    print(text)
    summary = os.environ.get("GITHUB_STEP_SUMMARY")
    if summary:
        with open(summary, "a") as file:
            file.write(text + "\n")


def annotate(path, line, title, message):
    """A GitHub annotation on a line of the pull request (only shown in GitHub Actions)."""
    if os.environ.get("GITHUB_ACTIONS") == "true":
        message = message.replace("%", "%25").replace("\r", "%0D").replace("\n", "%0A")
        print(f"::error file={path},line={line},title={title}::{message}")


def read_file(path):
    """A file of the working tree, by its path relative to the repository."""
    with open(os.path.join(ROOT, path), encoding="utf-8", errors="replace") as file:
        return file.read()


def source_files(sources):
    """The C++ files git tracks under the source folders, relative to the repository."""
    output = subprocess.run(["git", "ls-files", "--", *sources], cwd=ROOT, check=True, capture_output=True,
                            text=True).stdout
    return [path for path in output.splitlines() if path.endswith(CPP)]


def gate(arguments):
    """The gate on what changed since --base: new tidy findings and new or worse over-long or complex functions. The
    rules (thresholds.json and .clang-tidy) are the base's: a change is judged by the rules it started from."""
    thresholds = json.loads(rule_file(ROOT, arguments.base, THRESHOLDS))
    limits, sources = thresholds["function"], thresholds["sources"]
    changed = changes.changed_files(ROOT, arguments.base, sources, CPP)
    lines = {file.path: file.ranges for file in changed}
    base = subprocess.run(["git", "rev-parse", "--short", arguments.base], cwd=ROOT, check=True, capture_output=True,
                          text=True).stdout.strip()

    # A .cpp file this platform doesn't build (DesktopWin32.cpp on Linux) has no compile command, so clang-tidy can't
    # parse it: its tidy findings are left to a platform that builds it, and only its function sizes are judged here
    unbuilt = tidy.unbuilt(lines, tidy.translation_units(arguments.build, ROOT, sources))
    tidied = sorted(path for path in lines if path not in unbuilt)
    with tempfile.TemporaryDirectory() as folder:
        config, text = None, rule_file(ROOT, arguments.base, TIDY_CONFIG)
        if text is not None:
            config = os.path.join(folder, "clang-tidy.yaml")
            with open(config, "w", encoding="utf-8") as file:
                file.write(text)
        findings, failures = tidy.run(arguments.clang_tidy, arguments.build, ROOT, tidied, sources,
                                      arguments.jobs, lines, config)
    too_big, notes = functions.gate(changed, read_file, lambda path: changes.file_at(ROOT, arguments.base, path),
                                    limits, changes.removed_files(ROOT, arguments.base, sources, CPP))

    report = [f"### Quality gate: the changes since {base}", "",
              f"{len(changed)} C++ files changed. clang-tidy {tidy.version(arguments.clang_tidy)}, "
              f"function limits: {limits['length']} lines, complexity {limits['complexity']}. The rules "
              f"(`{TIDY_CONFIG}`, `{THRESHOLDS}`) are {base}'s.", ""]
    if unbuilt:
        report += ["Not built on this platform, so not given to clang-tidy here: "
                   + ", ".join(f"`{path}`" for path in unbuilt) + ".", ""]
    report += section("clang-tidy findings in the changed lines", [
        f"`{f['file']}:{f['line']}` {f['message']} [`{f['check']}`]" for f in findings])
    report += section("Files clang-tidy couldn't parse (so they weren't checked)", [
        f"`{path}`\n\n```\n{output[-3000:]}\n```" for path, output in failures])
    report += section("Functions over the limits", [f"`{f['file']}:{f['start']}` {message}" for f, message in too_big])
    if notes:
        report += ["Already over the limits before this change, and not worse (not a failure):", ""]
        report += [f"- `{f['file']}:{f['start']}` {message}" for f, message in notes] + [""]

    for finding in findings:
        annotate(finding["file"], finding["line"], f"clang-tidy: {finding['check']}", finding["message"])
    for function, message in too_big:
        annotate(function["file"], function["start"], "Function size", message)
    for path, output in failures:
        annotate(path, 1, "clang-tidy couldn't parse this file", output[-1000:])

    failed = bool(findings or too_big or failures)
    report.append("**Failed.**" if failed else "**Passed:** no new findings, no new over-long or complex functions.")
    publish("\n".join(report))
    return 1 if failed else 0


def section(title, items):
    """A Markdown section listing items, or saying there are none."""
    if not items:
        return [f"{title}: none.", ""]
    return [f"**{title}: {len(items)}**", ""] + [f"- {item}" for item in items] + [""]


def report(arguments):
    """The whole-repository report: function sizes and tidy findings per module, plus coverage if given."""
    thresholds = load_thresholds()
    sources, top = thresholds["sources"], thresholds["top"]
    measured = []
    for path in source_files(sources):
        measured += functions.measure(path, read_file(path))
    sizes = functions.summary(measured, thresholds["function"], thresholds["aim"], top)

    units = tidy.translation_units(arguments.build, ROOT, sources)
    findings, failures = tidy.run(arguments.clang_tidy, arguments.build, ROOT, units, sources, arguments.jobs)
    commit = subprocess.run(["git", "rev-parse", "HEAD"], cwd=ROOT, check=True, capture_output=True,
                            text=True).stdout.strip()
    result = {
        "commit": commit,
        "tools": {"clang-tidy": tidy.version(arguments.clang_tidy), "lizard": functions.lizard.version},
        "thresholds": {"function": thresholds["function"], "aim": thresholds["aim"]},
        "functions": sizes,
        "tidy": {"translation_units": len(units), "unparsed": [path for path, _ in failures], "total": len(findings),
                 "by_module": tidy.counts(findings, "module"), "by_check": tidy.counts(findings, "check"),
                 "by_file": tidy.counts(findings, "file"), "findings": findings},
    }
    if arguments.coverage:
        with open(arguments.coverage) as file:
            result["coverage"] = {name: figures for name, figures in json.load(file)["modules"].items()}
    with open(arguments.output, "w") as file:
        json.dump(result, file, indent=1)
    publish(report_markdown(result, top))
    return 0


def report_markdown(result, top):
    """The job summary's whole-repository report."""
    sizes, findings, limit, aim = result["functions"], result["tidy"], result["thresholds"]["function"], \
        result["thresholds"]["aim"]
    lines = [f"### Quality report: the whole repository at {result['commit'][:7]}", "",
             f"clang-tidy {result['tools']['clang-tidy']} on {findings['translation_units']} files, lizard "
             f"{result['tools']['lizard']} on {sizes['count']} functions.", "",
             "| | Over the limit | Over the aim |", "|---|---:|---:|",
             f"| Length | {sizes['over_limit']['length']} (> {limit['length']} lines) | "
             f"{sizes['over_aim']['length']} (> {aim['length']}) |",
             f"| Complexity | {sizes['over_limit']['complexity']} (> {limit['complexity']}) | "
             f"{sizes['over_aim']['complexity']} (> {aim['complexity']}) |", "",
             f"#### clang-tidy findings: {findings['total']}", "", "| Module | Findings |", "|---|---:|"]
    lines += [f"| {name} | {count} |" for name, count in findings["by_module"].items()]
    lines += ["", "| Check | Findings |", "|---|---:|"]
    lines += [f"| `{name}` | {count} |" for name, count in list(findings["by_check"].items())[:top]]
    if findings["unparsed"]:
        lines += ["", "Files clang-tidy couldn't parse: " + ", ".join(f"`{p}`" for p in findings["unparsed"])]
    for title, key in ((f"The {top} longest functions", "longest"), (f"The {top} most complex functions",
                                                                      "most_complex")):
        lines += ["", f"#### {title}", "", "| Function | Where | Lines | Complexity |", "|---|---|---:|---:|"]
        lines += [f"| `{f['name']}` | `{f['file']}:{f['start']}` | {f['length']} | {f['complexity']} |"
                  for f in sizes[key]]
    return "\n".join(lines) + "\n"


def coverage_command(arguments):
    """coverage.json and the coverage table, from llvm-cov's export; fails when coverage is below the floor in
    coverage-baseline.json (the base's, with --base)."""
    with open(arguments.export) as file:
        result = coverage.summarize(json.load(file), ROOT)
    with open(arguments.output, "w") as file:
        json.dump(result, file, indent=1)
    baseline = rule_file(ROOT, arguments.base, COVERAGE_BASELINE)
    failures = coverage.below_floor(result, json.loads(baseline)) if baseline else []
    source = f"`{COVERAGE_BASELINE}`" + (f" at {arguments.base}" if arguments.base else "")
    publish(coverage.markdown(result) + "\n" + coverage.floor_markdown(failures, source))
    for place, kind, measured, floor in failures:
        annotate(place if "/" in place else COVERAGE_BASELINE, 1, "Coverage floor",
                 f"{place}: {kind} coverage {measured:.2f}%, below the floor of {floor}%")
    return 1 if failures else 0


def main():
    """Parses the command line and runs one command."""
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    commands = parser.add_subparsers(dest="command", required=True)
    for name in ("gate", "report"):
        command = commands.add_parser(name)
        command.add_argument("--build", required=True, help="a build directory with compile_commands.json")
        command.add_argument("--clang-tidy", required=True, help="the clang-tidy executable")
        command.add_argument("--jobs", type=int, default=4, help="clang-tidy runs in parallel")
    commands.choices["gate"].add_argument("--base", required=True, help="the commit the changes are compared with")
    commands.choices["report"].add_argument("--output", required=True, help="where to write quality-report.json")
    commands.choices["report"].add_argument("--coverage", help="coverage.json, to include in the report")
    command = commands.add_parser("coverage")
    command.add_argument("--export", required=True, help="the output of llvm-cov export -summary-only")
    command.add_argument("--output", required=True, help="where to write coverage.json")
    command.add_argument("--base", help="the commit whose coverage floor applies (default: the working tree's)")

    arguments = parser.parse_args()
    run = {"gate": gate, "report": report, "coverage": coverage_command}[arguments.command]
    return run(arguments)


if __name__ == "__main__":
    sys.exit(main())
