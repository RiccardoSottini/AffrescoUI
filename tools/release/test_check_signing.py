#!/usr/bin/env python3
"""Tests of check_signing.py: which platforms a release run signs, and a platform's missing secrets.

    python3 -m unittest discover -s tools/release -p "test_check_signing.py"
"""

import os
import subprocess
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import check_signing  # noqa: E402


class Decide(unittest.TestCase):
    """check_signing.decide."""

    def test_a_tag_signs_both(self):
        sign, lines = check_signing.decide("push", "tag", set())
        self.assertEqual(sign, {"macos": True, "windows": True})
        self.assertIn("release environment", lines[0])
        self.assertIn("Sigstore bundle for every file", lines[-1])

    def test_a_manual_run_is_unsigned(self):
        sign, lines = check_signing.decide("workflow_dispatch", "branch", set())
        self.assertEqual(sign, {"macos": False, "windows": False})
        self.assertIn("only a v* tag is signed", lines[0])
        self.assertIn("not signed", lines[-1])

    def test_a_pull_request_is_never_signed(self):
        sign, lines = check_signing.decide("pull_request", "branch", set())
        self.assertEqual(sign, {"macos": False, "windows": False})
        self.assertIn("never signed", lines[0])

    def test_a_tag_may_release_a_platform_unsigned_when_the_variable_says_so(self):
        sign, lines = check_signing.decide("push", "tag", {"windows"})
        self.assertEqual(sign, {"macos": True, "windows": False})
        self.assertIn("allowed by RELEASE_UNSIGNED_PLATFORMS", lines[1])


class Secrets(unittest.TestCase):
    """check_signing.missing, and --secrets."""

    def test_missing_names_only_the_absent_ones(self):
        present = set(check_signing.SIGNING_INPUTS["macos"]) - {"APPLE_API_KEY_ID"}
        self.assertEqual(check_signing.missing("macos", present), ["APPLE_API_KEY_ID"])
        self.assertEqual(check_signing.missing("windows", set()), list(check_signing.SIGNING_INPUTS["windows"]))

    def run_check(self, platform, present):
        """Runs --secrets with the given secrets marked as set; returns the process."""
        environment = {key: value for key, value in os.environ.items() if key not in ("GITHUB_OUTPUT",
                                                                                      "GITHUB_STEP_SUMMARY")}
        for name in check_signing.SIGNING_INPUTS[platform]:
            environment[name] = "true" if name in present else "false"
        return subprocess.run([sys.executable, os.path.join(HERE, "check_signing.py"), "--secrets", platform],
                              env=environment, capture_output=True, text=True, check=False)

    def test_a_platform_without_its_secrets_fails_and_names_them(self):
        result = self.run_check("windows", {"SIGNPATH_API_TOKEN"})
        self.assertEqual(result.returncode, 1)
        self.assertIn("SIGNPATH_ORGANIZATION_ID", result.stderr)
        self.assertNotIn("SIGNPATH_API_TOKEN,", result.stderr)

    def test_a_platform_with_every_secret_passes(self):
        result = self.run_check("macos", set(check_signing.SIGNING_INPUTS["macos"]))
        self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == "__main__":
    unittest.main()
