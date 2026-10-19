"""Function length and cyclomatic complexity, measured with lizard (https://github.com/terryyin/lizard).

Length is the number of lines from a function's first line to its closing brace, comments and blank lines included,
which is what a reader scrolls through. Complexity is McCabe's cyclomatic complexity: one, plus one for every
if, loop, case, catch, && and || (and ?:).
"""

import lizard

METRICS = ("length", "complexity")
UNITS = {"length": "{} lines", "complexity": "complexity {}"}


def measure(path, text):
    """The functions in one C++ file, as dicts: file, name, signature, start, end, length, complexity."""
    information = lizard.analyze_file.analyze_source_code(path, text)
    return [{"file": path, "name": function.name, "signature": function.long_name, "start": function.start_line,
             "end": function.end_line, "length": function.length, "complexity": function.cyclomatic_complexity}
            for function in information.function_list]


def over(function, limits):
    """The metrics on which a function is over its limit, e.g. ["length"]."""
    return [metric for metric in METRICS if function[metric] > limits[metric]]


def describe(function, metrics, limits):
    """A one-line description of a function's excess, e.g. "Runtime::build is 92 lines (limit 80)"."""
    parts = [f"{UNITS[metric].format(function[metric])} (limit {limits[metric]})" for metric in metrics]
    return f"{function['name']} is " + " and ".join(parts)


def unique_names(functions):
    """The names that only one function of a file has: an overload shares its name with another."""
    names = [function["name"] for function in functions]
    return {name for name in names if names.count(name) == 1}


def previous_versions(functions):
    """The functions of a file at the base, by signature, and by name where the name is unique in the file, so a
    function whose parameters changed is still compared with what it was. An overload has its own signature."""
    unique = unique_names(functions)
    versions = {function["name"]: function for function in functions if function["name"] in unique}
    versions.update((function["signature"], function) for function in functions)
    return versions


def gate(changed, read_head, read_base, limits):
    """Checks the functions a change touched. A function fails if it is over a limit and is new, or has grown on that
    metric: a function that was already over and didn't get worse is only noted, so editing old code isn't blocked.
    Returns (failures, notes), lists of (function, message)."""
    failures, notes = [], []
    for file in changed:
        base_text = read_base(file.base_path) if file.base_path else None
        before = previous_versions(measure(file.path, base_text) if base_text is not None else [])
        after = measure(file.path, read_head(file.path))
        # Matched by name only where the name is unique now too: a new overload is new, not the old function changed
        unique = unique_names(after)
        for function in after:
            metrics = over(function, limits)
            if not metrics or not file.touches(function["start"], function["end"]):
                continue
            previous = before.get(function["signature"],
                                  before.get(function["name"]) if function["name"] in unique else None)
            worse = [metric for metric in metrics if previous is None or function[metric] > previous[metric]]
            if worse:
                status = "new" if previous is None else "was " + " and ".join(
                    UNITS[metric].format(previous[metric]) for metric in worse)
                failures.append((function, f"{describe(function, worse, limits)}: {status}"))
            else:
                notes.append((function, f"{describe(function, metrics, limits)}: already over before, not worse"))
    return failures, notes


def summary(functions, limits, aim, top):
    """The whole-repository part of the report: counts over the limits and the aims, the longest and the most
    complex functions."""
    def entry(function):
        return {key: function[key] for key in ("file", "name", "start", "length", "complexity")}

    return {
        "count": len(functions),
        "over_limit": {metric: sum(function[metric] > limits[metric] for function in functions) for metric in METRICS},
        "over_aim": {metric: sum(function[metric] > aim[metric] for function in functions) for metric in METRICS},
        "longest": [entry(function) for function in sorted(functions, key=lambda f: (-f["length"], f["file"]))[:top]],
        "most_complex": [entry(function)
                         for function in sorted(functions, key=lambda f: (-f["complexity"], f["file"]))[:top]],
    }
