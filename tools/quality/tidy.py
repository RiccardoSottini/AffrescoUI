"""clang-tidy: runs it on the project's files in parallel, with the repository's .clang-tidy, and collects what it finds.

The gate does what clang-tidy-diff.py does (--line-filter with the changed lines), here rather than through that
script because the gate also needs the changed lines for the function checks, runs changed headers too, and fails
when a changed file can't be parsed: a file clang-tidy can't read would otherwise pass without being checked.
"""

import json
import os
import re
import subprocess
from concurrent.futures import ThreadPoolExecutor

# "path:line:column: warning: message [check-name,alias-name]"
FINDING = re.compile(r"^(?P<file>.+?):(?P<line>\d+):(?P<column>\d+): (?P<severity>warning|error): "
                     r"(?P<message>.*) \[(?P<checks>[\w.,-]+)\]$")


def version(clang_tidy):
    """The clang-tidy version, e.g. "18.1.3"."""
    output = subprocess.run([clang_tidy, "--version"], check=True, capture_output=True, text=True).stdout
    match = re.search(r"version (\d+[\w.]*)", output)
    return match[1] if match else output.strip()


def module_of(path):
    """The part of the repository a file belongs to: "format" for modules/format/..., else its top folder."""
    parts = path.split("/")
    return parts[1] if parts[0] == "modules" and len(parts) > 2 else parts[0]


def translation_units(build, root, sources):
    """The project's .cpp files in the compilation database, relative to root (not external/ or fetched code)."""
    with open(os.path.join(build, "compile_commands.json")) as file:
        entries = json.load(file)
    units = set()
    for entry in entries:
        path = relative(os.path.join(entry["directory"], entry["file"]), root)
        if path and path.endswith(".cpp") and path.split("/")[0] in sources:
            units.add(path)
    return sorted(units)


def relative(path, root):
    """A path relative to root, with forward slashes, or None if it is outside root."""
    path = os.path.relpath(os.path.realpath(path), os.path.realpath(root))
    return None if path.startswith("..") else path.replace(os.sep, "/")


def run(clang_tidy, build, root, files, sources, jobs, changed_lines=None):
    """Runs clang-tidy on each file and returns (findings, failures). Findings are dicts (file, line, column, check,
    message, module), once each even when a header is reached from several files, and only in `sources`.
    `changed_lines` ({path: [(first, last)]}) keeps only findings on those lines. Failures are (file, output) for files
    clang-tidy couldn't parse or crashed on."""
    command = [clang_tidy, "-p", build, "--quiet"]
    if changed_lines is not None:
        command.append("--line-filter=" + json.dumps(
            [{"name": path, "lines": [list(lines) for lines in ranges]} for path, ranges in changed_lines.items()]))

    def check(path):
        return path, subprocess.run([*command, path], cwd=root, capture_output=True, text=True)

    findings, failures = {}, []
    with ThreadPoolExecutor(max_workers=jobs) as pool:
        for path, result in pool.map(check, files):
            for line in result.stdout.splitlines():
                finding = parse(line, root, sources)
                if finding:
                    findings[(finding["file"], finding["line"], finding["column"], finding["check"])] = finding
            # clang-tidy exits with 0 after warnings, so any other status means the file wasn't fully checked: a
            # compile error, or a crash that printed no "error:" at all
            if result.returncode != 0:
                output = (result.stdout + result.stderr).strip()
                failures.append((path, output or f"clang-tidy exited with status {result.returncode}"))
    return sorted(findings.values(), key=lambda f: (f["file"], f["line"], f["column"])), failures


def parse(line, root, sources):
    """One finding from a line of clang-tidy's output, or None if the line isn't a finding in our sources."""
    match = FINDING.match(line)
    if not match:
        return None
    path = relative(os.path.join(root, match["file"]), root)
    if not path or path.split("/")[0] not in sources:
        return None
    return {"file": path, "line": int(match["line"]), "column": int(match["column"]),
            "check": match["checks"].split(",")[0], "message": match["message"], "module": module_of(path)}


def counts(findings, key):
    """How many findings there are per value of `key` (e.g. "module" or "check"), most first."""
    totals = {}
    for finding in findings:
        totals[finding[key]] = totals.get(finding[key], 0) + 1
    return dict(sorted(totals.items(), key=lambda item: (-item[1], item[0])))
