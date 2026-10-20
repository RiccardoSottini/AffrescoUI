"""What a change touched: the lines each file gained or changed since a base commit, read from git."""

import re
import subprocess

# A hunk header of `git diff -U0`: "@@ -old[,count] +new[,count] @@"
HUNK = re.compile(r"^@@ -\d+(?:,\d+)? \+(\d+)(?:,(\d+))? @@")

# An escape in a path git writes in quotes: a byte in octal, or a C escape
ESCAPE = re.compile(r'\\([0-7]{3}|[abtnvfr"\\])')
ESCAPES = {"a": "\a", "b": "\b", "t": "\t", "n": "\n", "v": "\v", "f": "\f", "r": "\r", '"': '"', "\\": "\\"}


def diff_path(text, prefix):
    """The path a "--- " or "+++ " line names, without its prefix ("a/" or "b/"), or None ("/dev/null"). git writes a
    path in quotes, with escapes, when it has a byte that isn't printable ASCII (any letter beyond it, a tab, a quote):
    such a file must be judged too, not skipped. A path with a space ends in a tab (a tab of its own is escaped)."""
    text = text[:-1] if text.endswith("\t") else text
    if len(text) > 1 and text.startswith('"') and text.endswith('"'):
        escaped = ESCAPE.sub(lambda match: chr(int(match[1], 8)) if len(match[1]) == 3 else ESCAPES[match[1]],
                             text[1:-1])
        text = escaped.encode("latin-1").decode("utf-8", errors="replace")  # the octal escapes are UTF-8 bytes
    return text[len(prefix):] if text.startswith(prefix) else None


class ChangedFile:
    """A file the change added or modified: its path now, its path at the base (None if new), and its changed lines."""

    def __init__(self, path, base_path):
        self.path = path
        self.base_path = base_path
        self.ranges = []  # (first, last) line numbers in the file now, both included

    def touches(self, first, last):
        """Whether any changed line falls between first and last (both included)."""
        return any(start <= last and end >= first for start, end in self.ranges)


def changed_files(root, base, paths, suffixes):
    """The files under `paths` ending in one of `suffixes` that differ from `base` in the working tree, so uncommitted
    changes count too. Lines that were only removed leave no range: nothing in the file now is new there."""
    output = subprocess.run(
        ["git", "-c", "core.quotePath=true", "diff", "-U0", "--no-color", "--no-ext-diff", "--find-renames", "--diff-filter=AMR",
         "--src-prefix=a/", "--dst-prefix=b/", base, "--", *paths],
        cwd=root, check=True, capture_output=True, encoding="utf-8", errors="replace").stdout
    files, base_path, current = [], None, None
    for line in output.splitlines():
        if line.startswith("--- "):
            base_path = diff_path(line[4:], "a/")  # None for "--- /dev/null", a new file
        elif line.startswith("+++ "):
            path = diff_path(line[4:], "b/")
            current = ChangedFile(path, base_path) if path and path.endswith(suffixes) else None
            if current:
                files.append(current)
        elif current and (match := HUNK.match(line)):
            first, count = int(match[1]), int(match[2] if match[2] is not None else 1)
            if count:
                current.ranges.append((first, first + count - 1))
    return [file for file in files if file.ranges]


def file_at(root, commit, path):
    """The text of a file at a commit, or None if it isn't there. Bytes that aren't UTF-8 are replaced, as when the
    working tree is read, so one odd byte can't stop the gate."""
    result = subprocess.run(["git", "show", f"{commit}:{path}"], cwd=root, capture_output=True, encoding="utf-8",
                            errors="replace")
    return result.stdout if result.returncode == 0 else None
