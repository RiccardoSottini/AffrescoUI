"""Unit tests for the quality tools: `tools/quality/run.sh test`, or with lizard installed:
python3 -m unittest discover -s tools/quality
"""

import os
import subprocess
import tempfile
import unittest

import changes
import coverage
import functions
import tidy

LIMITS = {"length": 10, "complexity": 3}


def function_text(name, branches, parameters="int value"):
    """A C++ function with `branches` ifs (complexity branches + 1), one line each."""
    lines = [f"int {name}({parameters}) {{", "\tint total = 0;"]
    lines += [f"\tif(value == {i}) total += {i};" for i in range(branches)]
    return "\n".join(lines + ["\treturn total;", "}", ""])


def changed(path, ranges, base_path="same"):
    """A ChangedFile with the given changed line ranges."""
    file = changes.ChangedFile(path, path if base_path == "same" else base_path)
    file.ranges = ranges
    return file


def run_gate(head, base, ranges):
    """The function gate on one file, a.cpp, from its base and head texts."""
    file = changed("a.cpp", ranges, "same" if base is not None else None)
    return functions.gate([file], lambda path: head, lambda path: base, LIMITS)


class FunctionGateTest(unittest.TestCase):
    """functions.gate: what fails and what is only noted."""

    def test_a_new_function_over_the_limits_fails(self):
        failures, notes = run_gate(function_text("probe", 12), None, [(1, 3)])
        self.assertEqual(len(failures), 1)
        self.assertIn("probe is 16 lines (limit 10) and complexity 13 (limit 3): new", failures[0][1])
        self.assertEqual(notes, [])

    def test_a_small_function_passes(self):
        self.assertEqual(run_gate(function_text("small", 1), None, [(1, 5)]), ([], []))

    def test_an_untouched_function_over_the_limits_passes(self):
        text = function_text("small", 1) + function_text("big", 12)
        self.assertEqual(run_gate(text, text, [(1, 2)]), ([], []))

    def test_an_old_function_that_did_not_grow_is_only_noted(self):
        base = function_text("big", 12)
        head = base.replace("int total = 0;", "int total = 0; // a comment")
        failures, notes = run_gate(head, base, [(2, 2)])
        self.assertEqual(failures, [])
        self.assertIn("already over before, not worse", notes[0][1])

    def test_an_old_function_that_grew_fails(self):
        failures, _ = run_gate(function_text("big", 13), function_text("big", 12), [(15, 15)])
        self.assertIn("big is 17 lines (limit 10) and complexity 14 (limit 3): was 16 lines and complexity 13",
                      failures[0][1])

    def test_a_function_whose_parameters_changed_is_matched_by_name(self):
        head = function_text("big", 12, "int value, int more")
        failures, notes = run_gate(head, function_text("big", 12), [(1, 1)])
        self.assertEqual((len(failures), len(notes)), (0, 1))

    def test_a_new_overload_of_an_old_function_is_new(self):
        base = function_text("big", 12)
        head = base + function_text("big", 12, "long value")
        failures, notes = run_gate(head, base, [(17, 32)])
        self.assertEqual(len(notes), 0)
        self.assertIn("big is 16 lines (limit 10) and complexity 13 (limit 3): new", failures[0][1])


class TidyTest(unittest.TestCase):
    """tidy.parse and tidy.counts."""

    def test_a_finding_is_parsed_relative_to_the_root(self):
        root = os.path.realpath(tempfile.gettempdir())
        line = f"{root}/modules/vcs/src/A.cpp:12:3: warning: do not do this [bugprone-x,cert-y]"
        finding = tidy.parse(line, root, ["modules"])
        self.assertEqual(finding, {"file": "modules/vcs/src/A.cpp", "line": 12, "column": 3, "check": "bugprone-x",
                                   "message": "do not do this", "module": "vcs"})

    def test_lines_outside_the_sources_are_ignored(self):
        root = os.path.realpath(tempfile.gettempdir())
        self.assertIsNone(tidy.parse(f"{root}/build/_deps/j.hpp:1:1: warning: x [misc-y]", root, ["modules"]))
        self.assertIsNone(tidy.parse("12 warnings generated.", root, ["modules"]))

    def test_a_clang_tidy_that_crashes_is_a_failure(self):
        with tempfile.TemporaryDirectory() as root:
            crashing = os.path.join(root, "clang-tidy")
            with open(crashing, "w") as file:
                file.write("#!/bin/sh\nkill -SEGV $$\n")  # dies by a signal, printing nothing
            os.chmod(crashing, 0o755)
            findings, failures = tidy.run(crashing, root, root, ["modules/A.cpp"], ["modules"], 1, {})
        self.assertEqual(findings, [])
        self.assertEqual(failures, [("modules/A.cpp", "clang-tidy exited with status -11")])

    def test_counts_are_sorted_most_first(self):
        findings = [{"module": "vcs"}, {"module": "format"}, {"module": "format"}]
        self.assertEqual(list(tidy.counts(findings, "module").items()), [("format", 2), ("vcs", 1)])


class ChangesTest(unittest.TestCase):
    """changes.changed_files on a scratch repository."""

    def git(self, *arguments):
        """Runs git in the scratch repository."""
        subprocess.run(["git", "-c", "user.name=t", "-c", "user.email=t@t", *arguments], cwd=self.root, check=True,
                       capture_output=True)

    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.root = self.directory.name
        os.makedirs(os.path.join(self.root, "modules"))
        self.git("init", "-q")
        self.write("modules/a.cpp", "one\ntwo\nthree\nfour\n")
        self.write("modules/b.cpp", "keep\n")
        self.git("add", ".")
        self.git("commit", "-q", "-m", "base")

    def tearDown(self):
        self.directory.cleanup()

    def write(self, path, text):
        """Writes a file of the scratch repository."""
        with open(os.path.join(self.root, path), "w") as file:
            file.write(text)

    def test_added_and_modified_lines_are_ranges_of_the_new_file(self):
        self.write("modules/a.cpp", "one\nTWO\nthree\nfour\nfive\nsix\n")
        self.write("modules/new.cpp", "x\n")
        self.write("modules/untracked.cpp", "x\n")
        self.git("add", "modules/new.cpp")  # git diff sees new files once they are added
        files = {file.path: file for file in changes.changed_files(self.root, "HEAD", ["modules"], (".cpp",))}
        self.assertEqual(files["modules/a.cpp"].ranges, [(2, 2), (5, 6)])
        self.assertEqual(files["modules/a.cpp"].base_path, "modules/a.cpp")
        self.assertEqual(files["modules/new.cpp"].base_path, None)
        self.assertNotIn("modules/b.cpp", files)
        self.assertNotIn("modules/untracked.cpp", files)

    def test_a_file_with_only_removed_lines_is_left_out(self):
        self.write("modules/a.cpp", "one\nfour\n")
        self.assertEqual(changes.changed_files(self.root, "HEAD", ["modules"], (".cpp",)), [])

    def test_a_renamed_file_keeps_its_base_path(self):
        self.git("mv", "modules/a.cpp", "modules/c.cpp")
        self.write("modules/c.cpp", "one\ntwo\nthree\nfour\nfive\n")
        files = changes.changed_files(self.root, "HEAD", ["modules"], (".cpp",))
        self.assertEqual([(file.path, file.base_path, file.ranges) for file in files],
                         [("modules/c.cpp", "modules/a.cpp", [(5, 5)])])
        self.assertEqual(changes.file_at(self.root, "HEAD", "modules/a.cpp"), "one\ntwo\nthree\nfour\n")
        self.assertIsNone(changes.file_at(self.root, "HEAD", "modules/c.cpp"))

    def test_bytes_that_are_not_utf8_are_replaced(self):
        with open(os.path.join(self.root, "modules/a.cpp"), "wb") as file:
            file.write(b"one\n\xe9\n")  # a Latin-1 byte
        self.git("commit", "-q", "-am", "latin-1")
        self.write("modules/a.cpp", "one\nchanged\n")
        files = changes.changed_files(self.root, "HEAD", ["modules"], (".cpp",))
        self.assertEqual([(file.path, file.ranges) for file in files], [("modules/a.cpp", [(2, 2)])])
        self.assertEqual(changes.file_at(self.root, "HEAD", "modules/a.cpp"), "one\n�\n")


class CoverageTest(unittest.TestCase):
    """coverage.summarize: per module figures, and which files were measured."""

    def test_modules_add_up_and_unmeasured_files_are_listed(self):
        with tempfile.TemporaryDirectory() as root:
            root = os.path.realpath(root)
            for path in ("modules/vcs/src/A.cpp", "modules/vcs/src/B.cpp", "modules/render/src/C.cpp"):
                os.makedirs(os.path.dirname(os.path.join(root, path)), exist_ok=True)
                open(os.path.join(root, path), "w").close()
            open(os.path.join(root, "modules/.DS_Store"), "w").close()  # a file, not a module

            def summary(covered, count):
                return {kind: {"covered": covered, "count": count, "percent": 100.0 * covered / count}
                        for kind in ("lines", "functions")}

            export = {"data": [{"totals": summary(3, 8), "files": [
                {"filename": f"{root}/modules/vcs/src/A.cpp", "summary": summary(1, 4)},
                {"filename": f"{root}/modules/vcs/include/A.hpp", "summary": summary(2, 4)}]}]}
            result = coverage.summarize(export, root)
        self.assertEqual(list(result["modules"]), ["render", "vcs"])
        vcs = result["modules"]["vcs"]
        self.assertEqual(vcs["figures"]["lines"], {"covered": 3, "count": 8, "percent": 37.5})
        self.assertEqual((vcs["measured"], vcs["unmeasured"]), (["modules/vcs/src/A.cpp"], ["modules/vcs/src/B.cpp"]))
        self.assertEqual(result["modules"]["render"]["figures"], {})
        self.assertIn("| render | – | – | – | 0 of 1 |", coverage.markdown(result))


if __name__ == "__main__":
    unittest.main()
