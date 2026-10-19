"""Coverage of the unit tests per module, from clang's source-based coverage (`llvm-cov export -summary-only`).

coverage.json keeps the per-module and per-file numbers in a stable form, so a later baseline can compare with it.
"""

import os

from tidy import module_of, relative

KINDS = ("lines", "functions", "regions", "branches")


def numbers(summary):
    """The covered, total and percent figures of one llvm-cov summary, for each kind."""
    return {kind: {"covered": summary[kind]["covered"], "count": summary[kind]["count"],
                   "percent": round(summary[kind]["percent"], 2)} for kind in KINDS if kind in summary}


def add(totals, summary):
    """Adds one file's covered and total figures to a module's totals."""
    for kind in KINDS:
        if kind in summary:
            total = totals.setdefault(kind, {"covered": 0, "count": 0})
            total["covered"] += summary[kind]["covered"]
            total["count"] += summary[kind]["count"]


def sources(root, module):
    """A module's .cpp files, relative to root."""
    found = []
    for folder, _, names in os.walk(os.path.join(root, "modules", module, "src")):
        found += [relative(os.path.join(folder, name), root) for name in names if name.endswith(".cpp")]
    return sorted(found)


def summarize(export, root):
    """Per module and per file coverage from llvm-cov's export. The figures cover only the files in the test binary:
    the linker leaves out the files of a library that nothing calls. So each module also lists which of its .cpp
    files were measured and which weren't, and a module with none is listed too, never silently dropped."""
    files, modules = {}, {}
    for name in sorted(os.listdir(os.path.join(root, "modules"))):
        if os.path.isdir(os.path.join(root, "modules", name)):  # not a stray file such as macOS's .DS_Store
            modules[name] = {"sources": sources(root, name), "measured": [], "figures": {}}
    for entry in export["data"][0]["files"]:
        path = relative(entry["filename"], root)
        if not path or module_of(path) not in modules:
            continue
        files[path] = numbers(entry["summary"])
        module = modules[module_of(path)]
        add(module["figures"], entry["summary"])
        if path in module["sources"]:
            module["measured"].append(path)
    for module in modules.values():
        module["unmeasured"] = [path for path in module.pop("sources") if path not in module["measured"]]
        for figure in module["figures"].values():
            figure["percent"] = round(100.0 * figure["covered"] / figure["count"], 2) if figure["count"] else 0.0
    return {"tool": "llvm-cov", "tests": "affrescoui_unit_tests", "total": numbers(export["data"][0]["totals"]),
            "modules": modules, "files": dict(sorted(files.items()))}


def markdown(coverage):
    """The job summary's coverage table."""
    lines = ["### Unit test coverage", "",
             "Of the files in the test binary. A .cpp file that nothing in the tests calls isn't linked, so it isn't "
             "measured: the last column counts them.", "",
             "| Module | Lines | Functions | Branches | .cpp files measured |", "|---|---:|---:|---:|---:|"]

    def cell(figures, kind):
        figure = figures.get(kind)
        return f"{figure['percent']:.1f}% ({figure['covered']}/{figure['count']})" if figure else "–"

    for name, module in coverage["modules"].items():
        figures, measured, total = module["figures"], len(module["measured"]), len(module["measured"]) + len(
            module["unmeasured"])
        lines.append(f"| {name} | {cell(figures, 'lines')} | {cell(figures, 'functions')} | "
                     f"{cell(figures, 'branches')} | {f'{measured} of {total}' if total else 'headers only'} |")
    total = coverage["total"]
    lines.append(f"| **all** | {cell(total, 'lines')} | {cell(total, 'functions')} | {cell(total, 'branches')} | |")
    return "\n".join(lines) + "\n"
