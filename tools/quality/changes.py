"""What a change touched: the lines each file gained or changed since a base commit, read from git."""

import re
import subprocess

# A hunk header of `git diff -U0`: "@@ -old[,count] +new[,count] @@"
HUNK = re.compile(r"^@@ -\d+(?:,\d+)? \+(\d+)(?:,(\d+))? @@")


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
        ["git", "diff", "-U0", "--no-color", "--no-ext-diff", "--find-renames", "--diff-filter=AMR",
         "--src-prefix=a/", "--dst-prefix=b/", base, "--", *paths],
        cwd=root, check=True, capture_output=True, encoding="utf-8", errors="replace").stdout
    files, base_path, current = [], None, None
    for line in output.splitlines():
        if line.startswith("--- "):
            base_path = line[6:] if line.startswith("--- a/") else None  # "--- /dev/null" for a new file
        elif line.startswith("+++ b/"):
            current = ChangedFile(line[6:], base_path) if line.endswith(suffixes) else None
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
