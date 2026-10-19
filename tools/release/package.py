#!/usr/bin/env python3
"""Packs a build's programs into the archive a release offers for one platform:

    affrescoui-<version>-<platform>/
        affrescoui, affrescoui_host, affrescoui_studio     (.exe on Windows)
        assets/screens/, assets/fonts/                     the templates and their fonts (the programs look here
                                                           when the source tree they were built from isn't there)
        LICENSE, NOTICE, THIRD_PARTY_NOTICES.md, README.txt

    python3 tools/release/package.py --build build --platform macos-universal [--out dist]

It writes affrescoui-<version>-<platform>.tar.gz (.zip for a Windows platform) in --out and prints its name. The
version is the library's (check_version.py). Only the standard library is needed.
"""

import argparse
import os
import shutil
import subprocess
import sys
import tarfile
import tempfile
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
PROGRAMS = ("affrescoui", "affrescoui_host", "affrescoui_studio")
NOTICES = ("LICENSE", "NOTICE", "THIRD_PARTY_NOTICES.md")
README = """AffrescoUI {version} for {platform}

  ./affrescoui_host                 the Kanban template; other templates are in assets/screens
  ./affrescoui_studio               the Studio: ask an agent for changes, review them
  ./affrescoui help                 the command-line tool: check, render, fmt, ids, run, export, import

The MCP server for AI agents is on PyPI: pipx install affrescoui-mcp, then
  claude mcp add affrescoui -- affrescoui-mcp

On macOS, a downloaded program is quarantined until you allow it: xattr -dr com.apple.quarantine <this folder>.
Documentation: https://github.com/RiccardoSottini/AffrescoUI
"""


def program(build, name, windows):
    """A built program's file: in the build folder, or its Release folder (multi-configuration generators)."""
    file = name + (".exe" if windows else "")
    for folder in (build, os.path.join(build, "Release")):
        if os.path.isfile(os.path.join(folder, file)):
            return os.path.join(folder, file)
    sys.exit(f"package.py: {file} is not in {build}: build first")


def archive(stage, name, out, windows):
    """The staged folder as a .zip (Windows) or a .tar.gz that keeps the programs executable."""
    if windows:
        path = os.path.join(out, name + ".zip")
        with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as file:
            for folder, _, files in os.walk(os.path.join(stage, name)):
                for each in sorted(files):
                    full = os.path.join(folder, each)
                    file.write(full, os.path.relpath(full, stage))
    else:
        path = os.path.join(out, name + ".tar.gz")
        with tarfile.open(path, "w:gz") as file:
            file.add(os.path.join(stage, name), arcname=name)
    return path


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--build", required=True, help="the build folder")
    parser.add_argument("--platform", required=True, help="e.g. macos-universal, linux-x86_64, windows-x86_64")
    parser.add_argument("--out", default=os.path.join(ROOT, "dist"), help="where the archive goes")
    args = parser.parse_args()
    windows = args.platform.startswith("windows")
    version = subprocess.run([sys.executable, os.path.join(ROOT, "tools", "release", "check_version.py")],
                             check=True, capture_output=True, text=True).stdout.strip()
    name = f"affrescoui-{version}-{args.platform}"

    with tempfile.TemporaryDirectory(prefix="affrescoui-package-") as stage:
        folder = os.path.join(stage, name)
        os.makedirs(folder)
        for each in PROGRAMS:
            shutil.copy2(program(args.build, each, windows), folder)
        shutil.copytree(os.path.join(ROOT, "examples", "assets"), os.path.join(folder, "assets"))
        for each in NOTICES:
            shutil.copyfile(os.path.join(ROOT, each), os.path.join(folder, each))
        with open(os.path.join(folder, "README.txt"), "w", encoding="utf-8", newline="\r\n" if windows else "\n") as f:
            f.write(README.format(version=version, platform=args.platform))
        os.makedirs(args.out, exist_ok=True)
        print(os.path.basename(archive(stage, name, os.path.abspath(args.out), windows)))


if __name__ == "__main__":
    main()
