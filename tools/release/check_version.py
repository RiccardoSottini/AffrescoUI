#!/usr/bin/env python3
"""Checks that the version is the same everywhere a release states it: the library (CMakeLists.txt, project
VERSION), the MCP package (mcp/pyproject.toml) and the MCP server (serverInfo in mcp/affrescoui_mcp.py); and, for a
tag, that the tag is "v" and that version. Prints the version.

    python3 tools/release/check_version.py [--tag v0.2.0]
"""

import argparse
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
PLACES = {  # file -> the pattern whose first group is the version
    "CMakeLists.txt": r"project\(AffrescoUI VERSION (\d+\.\d+\.\d+)",
    "mcp/pyproject.toml": r'(?m)^version = "([^"]+)"',
    "mcp/affrescoui_mcp.py": r'"serverInfo": \{"name": "affrescoui", "version": "([^"]+)"\}',
}


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--tag", help="the release tag, e.g. v0.2.0")
    args = parser.parse_args()

    versions = {}
    for path, pattern in PLACES.items():
        with open(os.path.join(ROOT, path), encoding="utf-8") as file:
            found = re.search(pattern, file.read())
        if not found:
            sys.exit(f"check_version.py: no version found in {path}")
        versions[path] = found.group(1)
    if len(set(versions.values())) != 1:
        sys.exit("check_version.py: the versions differ: " + ", ".join(f"{p} {v}" for p, v in versions.items()))
    version = next(iter(versions.values()))
    if args.tag and args.tag != "v" + version:
        sys.exit(f"check_version.py: the tag {args.tag} is not v{version}, the version in {', '.join(PLACES)}")
    print(version)


if __name__ == "__main__":
    main()
