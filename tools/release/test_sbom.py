#!/usr/bin/env python3
"""Tests of sbom.py, on small trees made in a temporary folder: a correct tree passes, and a hash that doesn't match,
a missing pin, notices that drifted and a licence that isn't permissive each fail; the SBOM is valid SPDX 2.3.

    python3 -m unittest discover -s tools/release -p "test_sbom.py"
"""

import hashlib
import json
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sbom  # noqa: E402

ARCHIVE_HASH = "a" * 64
REQUIREMENT_HASH = "b" * 64
VENDORED = b"/* a vendored header */\n"


def manifest():
    """The manifest of the small tree: one download, one vendored file and one Python requirement."""
    common = {"purpose": "a test", "license": "MIT", "licenseNote": "MIT", "usedBy": ["core"], "scope": "runtime"}
    return [
        dict(common, name="zlibby", version="1.2.3", kind="downloaded", url="https://example.org/zlibby-1.2.3.tar.gz",
             sha256=ARCHIVE_HASH, pinnedIn="CMakeLists.txt"),
        dict(common, name="header", version="1.0", kind="vendored", url="https://example.org/header",
             files=[{"path": "external/header/header.h", "sha256": hashlib.sha256(VENDORED).hexdigest()}]),
        dict(common, name="wheel", version="2.0", kind="python", url="https://example.org/wheel/2.0/",
             sha256=REQUIREMENT_HASH, pinnedIn="tools/release/requirements.txt", scope="build"),
    ]


class SbomTest(unittest.TestCase):
    """sbom.py's checks and documents, on a tree of its own."""

    def setUp(self):
        """Writes a tree the manifest describes exactly."""
        self.folder = tempfile.TemporaryDirectory()
        self.root = self.folder.name
        self.write("CMakeLists.txt", "project(AffrescoUI VERSION 1.2.3)\nFetchContent_Declare(zlibby\n"
                   "  URL https://example.org/zlibby-1.2.3.tar.gz\n  URL_HASH SHA256=" + ARCHIVE_HASH + ")\n")
        self.write("external/header/header.h", VENDORED.decode())
        self.write("tools/release/requirements.txt", "wheel==2.0 \\\n    --hash=sha256:" + REQUIREMENT_HASH + "\n")
        self.save(manifest())

    def tearDown(self):
        """Removes the tree."""
        self.folder.cleanup()

    def write(self, path, text):
        """Writes a file of the tree."""
        full = os.path.join(self.root, path)
        os.makedirs(os.path.dirname(full), exist_ok=True)
        with open(full, "w", encoding="utf-8", newline="\n") as file:
            file.write(text)

    def save(self, dependencies):
        """Writes the manifest and the notices that match it."""
        self.write(sbom.MANIFEST, json.dumps({"schemaVersion": 1, "dependencies": dependencies}))
        self.write(sbom.NOTICES, sbom.notices(dependencies))

    def assertProblem(self, words):
        """Asserts that the check finds a problem containing the given words."""
        problems = sbom.check(self.root)
        self.assertTrue(any(words in problem for problem in problems), problems)

    def test_a_correct_tree_passes(self):
        """The tree as made passes."""
        self.assertEqual(sbom.check(self.root), [])

    def test_a_changed_vendored_file_is_a_hash_mismatch(self):
        """A vendored file that no longer matches its hash fails."""
        self.write("external/header/header.h", "/* changed */\n")
        self.assertProblem("hash mismatch")

    def test_a_download_with_another_hash_is_a_hash_mismatch(self):
        """A manifest hash that CMakeLists.txt doesn't pin fails."""
        dependencies = manifest()
        dependencies[0]["sha256"] = "c" * 64
        self.save(dependencies)
        self.assertProblem("hash mismatch")

    def test_a_download_without_a_hash_is_a_missing_pin(self):
        """A FetchContent_Declare without URL_HASH fails."""
        self.write("cmake/More.cmake", "FetchContent_Declare(other\n  URL https://example.org/other.tar.gz)\n")
        self.assertProblem("missing pin")

    def test_an_entry_without_a_hash_is_a_missing_pin(self):
        """A manifest entry without a SHA-256 fails."""
        dependencies = manifest()
        del dependencies[2]["sha256"]
        self.save(dependencies)
        self.assertProblem("missing pin")

    def test_an_unlisted_vendored_file_fails(self):
        """A file under external/ that the manifest doesn't list fails."""
        self.write("external/other/other.h", "/* other */\n")
        self.assertProblem("vendored but not in")

    def test_a_changed_requirement_fails(self):
        """A requirement pinned at another version fails."""
        self.write("tools/release/requirements.txt", "wheel==2.1 \\\n    --hash=sha256:" + REQUIREMENT_HASH + "\n")
        self.assertProblem("version 2.0")

    def test_drifted_notices_fail(self):
        """Notices edited by hand fail."""
        self.write(sbom.NOTICES, sbom.notices(manifest()) + "an edit\n")
        self.assertProblem("drifted")

    def test_a_copyleft_licence_is_flagged(self):
        """A licence that isn't permissive fails, and passes once reviewed."""
        dependencies = manifest()
        dependencies[1]["license"] = "GPL-3.0-only"
        self.save(dependencies)
        self.assertProblem("not a permissive licence")
        dependencies[1]["licenseReview"] = "used only to build"
        self.save(dependencies)
        self.assertEqual(sbom.check(self.root), [])

    def test_licence_expressions(self):
        """OR needs one permissive side, AND both, WITH a known exception."""
        self.assertTrue(sbom.licence_permissive("FTL OR GPL-2.0-or-later"))
        self.assertFalse(sbom.licence_permissive("MIT AND GPL-2.0-only"))
        self.assertTrue(sbom.licence_permissive("(BSD-3-Clause WITH PCRE2-exception) AND MIT"))
        self.assertFalse(sbom.licence_permissive("MPL-2.0"))
        with self.assertRaises(ValueError):
            sbom.licence_permissive("MIT OR")

    def test_the_sbom_is_spdx(self):
        """The SBOM is SPDX 2.3 JSON, describes the project, and has every component with its checksums."""
        document = sbom.sbom(self.root, sbom.creation_time())
        self.assertEqual(document["spdxVersion"], "SPDX-2.3")
        self.assertEqual(document["dataLicense"], "CC0-1.0")
        names = {package["name"]: package for package in document["packages"]}
        self.assertEqual(set(names), {"AffrescoUI", "zlibby", "header", "wheel"})
        self.assertEqual(names["AffrescoUI"]["versionInfo"], "1.2.3")
        self.assertEqual(names["zlibby"]["checksums"][0]["checksumValue"], ARCHIVE_HASH)
        self.assertEqual(document["files"][0]["fileName"], "./external/header/header.h")
        ids = {element["SPDXID"] for element in document["packages"] + document["files"]} | {"SPDXRef-DOCUMENT"}
        for relationship in document["relationships"]:
            self.assertIn(relationship["spdxElementId"], ids)
            self.assertIn(relationship["relatedSpdxElement"], ids)
        json.dumps(document)


if __name__ == "__main__":
    unittest.main()
