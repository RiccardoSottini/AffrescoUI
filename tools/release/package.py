#!/usr/bin/env python3
"""Packs a build's programs into the archive a release offers for one platform:

    affrescoui-<version>-<platform>/
        affrescoui, affrescoui_launcher                        (.exe on Windows)
        assets/<folder>/, assets/shared/                   every app's folder (an examples/ folder with an app.json)
                                                           and their fonts, as in examples/ without sources (the
                                                           programs look here when the source tree they were built
                                                           from isn't there), and SCREEN_FORMAT.md for the host's agent
        LICENSE, NOTICE, THIRD_PARTY_NOTICES.md, README.txt

    python3 tools/release/package.py --build build --platform macos-universal [--out dist] [--app DIR] [--signed]

It writes affrescoui-<version>-<platform>.tar.gz (.zip for a Windows platform) in --out and prints its name. The
version is the library's (check_version.py). Only the standard library is needed.

--app DIR (macOS) also writes the Launcher as an application bundle, DIR/AffrescoUI.app, unsigned:

    AffrescoUI.app/Contents/
        Info.plist                                         the bundle's id, name, version, icon, minimum macOS
        MacOS/affrescoui_launcher
        Resources/AffrescoUI.icns, Resources/assets/       the icon (brand/), and the assets as in the archive
        Resources/LICENSE, NOTICE, THIRD_PARTY_NOTICES.md

tools/release/macos.sh signs it and makes the disk image. --signed says in README.txt that the programs are signed
(the release workflow passes it when it signed them, before packing).
"""

import argparse
import os
import plistlib
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
PROGRAMS = ("affrescoui", "affrescoui_launcher")
NOTICES = ("LICENSE", "NOTICE", "THIRD_PARTY_NOTICES.md")
README = """AffrescoUI {version} for {platform}

  ./affrescoui_launcher                 the Launcher (assets/launcher): every app, opened with a click, and new ones
  ./affrescoui_launcher <screen.json>   one template, e.g. assets/flow/index.screen.json (the Kanban board)
  ./affrescoui help                 the command-line tool: check, render, fmt, ids, run, export, import

The MCP server for AI agents is in this release (the .whl file): pipx install ./affrescoui_mcp-<version>-py3-none-any.whl, then
  claude mcp add affrescoui -- affrescoui-mcp

{signing}
Every file of the release has a Sigstore signature (<file>.sigstore.json) and a line in SHA256SUMS; SECURITY.md,
"How releases are signed", says how to check them.
Documentation: https://github.com/RiccardoSottini/AffrescoUI
"""
SIGNING = {
    ("macos", True): "Both programs are signed with a Developer ID and notarized by Apple. A program outside an app can't\n"
                     "carry its notarization ticket (only apps and disk images can be stapled), so the first time one runs,\n"
                     "macOS asks Apple about it over the network. For an app to double-click that opens offline too, use\n"
                     "AffrescoUI-{version}-macos-universal.dmg.",
    ("macos", False): "These programs aren't signed. On macOS, a downloaded program is quarantined until you allow it:\n"
                      "xattr -dr com.apple.quarantine <this folder>.",
    ("windows", True): "Both programs are signed (Authenticode; the publisher is SignPath Foundation).",
    ("windows", False): "These programs aren't signed, so Windows SmartScreen may warn before they run.",
    ("linux", True): "",
    ("linux", False): "",
}
APP_NAME = "AffrescoUI"
BUNDLE_ID = "io.github.riccardosottini.affrescoui"
ICON = os.path.join(ROOT, "brand", "AffrescoUI.icns")


def program(build, name, windows):
    """A built program's file: in the build folder, or its Release folder (multi-configuration generators)."""
    file = name + (".exe" if windows else "")
    for folder in (build, os.path.join(build, "Release")):
        if os.path.isfile(os.path.join(folder, file)):
            return os.path.join(folder, file)
    sys.exit(f"package.py: {file} is not in {build}: build first")


def copy_assets(assets):
    """Every app's folder (an app.json in it: each opens in the stock host) and examples/shared, as the programs find
    them in examples/, without the C++ sources and the tests' scripts."""
    examples = os.path.join(ROOT, "examples")
    folders = sorted(each for each in os.listdir(examples) if os.path.isfile(os.path.join(examples, each, "app.json")))
    for each in folders + ["shared"]:
        shutil.copytree(os.path.join(examples, each), os.path.join(assets, each),
                        ignore=shutil.ignore_patterns("*.cpp", "*.hpp", "*.py"))
    # The host's own agent reads the format guide by topic (modules/intelligence, ScreenQueries.hpp)
    shutil.copyfile(os.path.join(ROOT, "SCREEN_FORMAT.md"), os.path.join(assets, "SCREEN_FORMAT.md"))


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


def minimum_macos(build):
    """The oldest macOS the build runs on: its CMAKE_OSX_DEPLOYMENT_TARGET, else the program's own minos."""
    with open(os.path.join(build, "CMakeCache.txt"), encoding="utf-8") as file:
        found = re.search(r"^CMAKE_OSX_DEPLOYMENT_TARGET:\w+=(\S+)$", file.read(), re.MULTILINE)
    if found:
        return found.group(1)
    loads = subprocess.run(["otool", "-l", program(build, "affrescoui_launcher", False)], check=True,
                           capture_output=True, text=True).stdout
    versions = re.findall(r"^\s*minos (\S+)$", loads, re.MULTILINE)
    if not versions:
        sys.exit("package.py: no deployment target in the build, and no minos in affrescoui_launcher")
    return max(versions, key=lambda each: tuple(int(part) for part in each.split(".")))


def make_app(build, out, version):
    """Writes the Launcher's application bundle, out/AffrescoUI.app (unsigned), and returns its path. The program
    finds its assets in Contents/Resources/assets (apps/shared/Assets.hpp)."""
    app = os.path.join(out, APP_NAME + ".app")
    shutil.rmtree(app, ignore_errors=True)
    contents = os.path.join(app, "Contents")
    resources = os.path.join(contents, "Resources")
    os.makedirs(os.path.join(contents, "MacOS"))
    os.makedirs(resources)
    shutil.copy2(program(build, "affrescoui_launcher", False), os.path.join(contents, "MacOS"))
    copy_assets(os.path.join(resources, "assets"))
    shutil.copyfile(ICON, os.path.join(resources, APP_NAME + ".icns"))
    for each in NOTICES:
        shutil.copyfile(os.path.join(ROOT, each), os.path.join(resources, each))
    info = {
        "CFBundleDevelopmentRegion": "en",
        "CFBundleDisplayName": APP_NAME,
        "CFBundleExecutable": "affrescoui_launcher",
        "CFBundleIconFile": APP_NAME,
        "CFBundleIdentifier": BUNDLE_ID,
        "CFBundleInfoDictionaryVersion": "6.0",
        "CFBundleName": APP_NAME,
        "CFBundlePackageType": "APPL",
        "CFBundleShortVersionString": version,
        "CFBundleVersion": version,
        "LSApplicationCategoryType": "public.app-category.developer-tools",
        "LSMinimumSystemVersion": minimum_macos(build),
        "NSHighResolutionCapable": True,
        "NSHumanReadableCopyright": "Copyright 2017-2026 Riccardo Sottini. Apache License 2.0.",
        "NSSupportsAutomaticGraphicsSwitching": True,  # OpenGL: stay on the integrated GPU unless asked
    }
    with open(os.path.join(contents, "Info.plist"), "wb") as file:
        plistlib.dump(info, file)
    with open(os.path.join(contents, "PkgInfo"), "w", encoding="ascii") as file:
        file.write("APPL????")
    return app


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--build", required=True, help="the build folder")
    parser.add_argument("--platform", required=True, help="e.g. macos-universal, linux-x86_64, windows-x86_64")
    parser.add_argument("--out", default=os.path.join(ROOT, "dist"), help="where the archive goes")
    parser.add_argument("--app", help="macOS: also write AffrescoUI.app (unsigned) in this folder")
    parser.add_argument("--signed", action="store_true", help="the programs were signed: say so in README.txt")
    args = parser.parse_args()
    system = args.platform.split("-")[0]
    windows = system == "windows"
    if args.app and system != "macos":
        parser.error("--app is for a macOS platform")
    version = subprocess.run([sys.executable, os.path.join(ROOT, "tools", "release", "check_version.py")],
                             check=True, capture_output=True, text=True).stdout.strip()
    name = f"affrescoui-{version}-{args.platform}"
    signing = SIGNING.get((system, args.signed), "").format(version=version)

    with tempfile.TemporaryDirectory(prefix="affrescoui-package-") as stage:
        folder = os.path.join(stage, name)
        os.makedirs(folder)
        for each in PROGRAMS:
            shutil.copy2(program(args.build, each, windows), folder)
        copy_assets(os.path.join(folder, "assets"))
        for each in NOTICES:
            shutil.copyfile(os.path.join(ROOT, each), os.path.join(folder, each))
        with open(os.path.join(folder, "README.txt"), "w", encoding="utf-8", newline="\r\n" if windows else "\n") as f:
            f.write(README.format(version=version, platform=args.platform, signing=signing + "\n" if signing else ""))
        os.makedirs(args.out, exist_ok=True)
        print(os.path.basename(archive(stage, name, os.path.abspath(args.out), windows)))
    if args.app:
        os.makedirs(args.app, exist_ok=True)
        print(make_app(args.build, os.path.abspath(args.app), version))


if __name__ == "__main__":
    main()
