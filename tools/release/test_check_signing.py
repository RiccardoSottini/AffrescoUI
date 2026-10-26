#!/usr/bin/env python3
"""Tests of check_signing.py: which platforms a release run signs, and when it must fail.

    python3 -m unittest discover -s tools/release -p "test_check_signing.py"
"""

import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import check_signing  # noqa: E402

EVERY = {name for names in check_signing.SECRETS.values() for name in names}
MACOS = set(check_signing.SECRETS["macos"])


class Decide(unittest.TestCase):
    """check_signing.decide."""

    def test_a_tag_with_every_secret_signs_both(self):
        sign, lines, failed = check_signing.decide("push", "tag", EVERY, set())
        self.assertEqual(sign, {"macos": True, "windows": True})
        self.assertFalse(failed)
        self.assertIn("Sigstore bundle for every file", lines[-1])

    def test_a_dry_run_without_secrets_is_unsigned_and_names_them(self):
        sign, lines, failed = check_signing.decide("workflow_dispatch", "branch", set(), set())
        self.assertEqual(sign, {"macos": False, "windows": False})
        self.assertFalse(failed)
        self.assertIn("secret MACOS_CERTIFICATE_P12", lines[0])
        self.assertIn("SIGNPATH_API_TOKEN", lines[1])

    def test_one_missing_secret_leaves_its_platform_unsigned(self):
        sign, lines, _ = check_signing.decide("workflow_dispatch", "branch", EVERY - {"APPLE_API_KEY_ID"}, set())
        self.assertEqual(sign, {"macos": False, "windows": True})
        self.assertIn("secret APPLE_API_KEY_ID missing", lines[0])
        self.assertNotIn("MACOS_CERTIFICATE_P12", lines[0])

    def test_a_pull_request_is_never_signed(self):
        sign, lines, failed = check_signing.decide("pull_request", "branch", EVERY, set())
        self.assertEqual(sign, {"macos": False, "windows": False})
        self.assertFalse(failed)
        self.assertIn("never signed", lines[0])

    def test_a_tag_without_secrets_fails(self):
        sign, lines, failed = check_signing.decide("push", "tag", MACOS, set())
        self.assertEqual(sign, {"macos": True, "windows": False})
        self.assertTrue(failed)
        self.assertIn("a tag must be signed", lines[1])

    def test_a_tag_may_release_a_platform_unsigned_when_the_variable_says_so(self):
        sign, lines, failed = check_signing.decide("push", "tag", MACOS, {"windows"})
        self.assertEqual(sign, {"macos": True, "windows": False})
        self.assertFalse(failed)
        self.assertIn("allowed by RELEASE_UNSIGNED_PLATFORMS", lines[1])


if __name__ == "__main__":
    unittest.main()
