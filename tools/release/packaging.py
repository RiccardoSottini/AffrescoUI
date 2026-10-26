#!/usr/bin/env python3
"""Fills in the package recipes of packaging/ (Homebrew, winget, AppImage, vcpkg, Conan) for one release: its
version, and the hashes of its files from SHA256SUMS. The recipes in packaging/ are templates and stay so; the
filled copies go to --out. Nothing is published or submitted.

    python3 tools/release/packaging.py --version 0.2.0 --sums dist/SHA256SUMS [--out dist/packages]
    python3 tools/release/packaging.py --check

The templates say @VERSION@, @RELEASE_DATE@ (SOURCE_DATE_EPOCH, or today), @SOURCE_SHA256@ and @SOURCE_SHA512@ (the
source tarball, affrescoui-<version>-source.tar.gz) and @WINDOWS_SHA256@ (affrescoui-<version>-windows-x86_64.zip).
The SHA-512, which vcpkg asks for, is computed from the file next to SHA256SUMS, after checking its SHA-256. A recipe
folder whose files aren't all in SHA256SUMS is skipped, with a note.

--check makes sure the recipes stay in step with the library: the targets packaging/conan/test_package checks are the
ones cmake/AffrescoUIInstall.cmake exports, and every token in packaging/ is one this script fills. Only the standard
library is needed.
"""

import argparse
import datetime
import hashlib
import os
import re
import shutil
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
PACKAGING = os.path.join(ROOT, "packaging")
INSTALL = os.path.join(ROOT, "cmake", "AffrescoUIInstall.cmake")
TEST_PACKAGE = os.path.join(PACKAGING, "conan", "test_package", "CMakeLists.txt")
TOKENS = ("VERSION", "RELEASE_DATE", "SOURCE_SHA256", "SOURCE_SHA512", "WINDOWS_SHA256")
TOKEN = re.compile(r"@([A-Z0-9_]+)@")
FILES = {"SOURCE": "affrescoui-{version}-source.tar.gz", "WINDOWS": "affrescoui-{version}-windows-x86_64.zip"}


def read_sums(path):
    """SHA256SUMS as a dictionary from a file's name to its SHA-256 (lower case)."""
    sums = {}
    with open(path, encoding="utf-8") as file:
        for line in file:
            found = re.match(r"([0-9a-fA-F]{64})\s+\*?(\S+)\s*$", line)
            if found:
                sums[os.path.basename(found.group(2))] = found.group(1).lower()
    return sums


def file_hash(path, algorithm):
    """A file's hash, in lower-case hexadecimal."""
    digest = hashlib.new(algorithm)
    with open(path, "rb") as file:
        for block in iter(lambda: file.read(1 << 16), b""):
            digest.update(block)
    return digest.hexdigest()


def values(version, sums_path):
    """The value of every token that can be filled for this release, as a dictionary."""
    sums = read_sums(sums_path)
    epoch = os.environ.get("SOURCE_DATE_EPOCH")
    date = datetime.datetime.fromtimestamp(int(epoch), datetime.timezone.utc) if epoch else datetime.datetime.now()
    filled = {"VERSION": version, "RELEASE_DATE": date.strftime("%Y-%m-%d")}
    for key, pattern in FILES.items():
        name = pattern.format(version=version)
        if name not in sums:
            continue
        filled[key + "_SHA256"] = sums[name]
        beside = os.path.join(os.path.dirname(os.path.abspath(sums_path)), name)
        if os.path.isfile(beside):
            if file_hash(beside, "sha256") != sums[name]:
                sys.exit(f"packaging.py: {name} doesn't match its line in {sums_path}")
            filled[key + "_SHA512"] = file_hash(beside, "sha512")
    return filled


def templates():
    """The recipe folders of packaging/ and their files: a dictionary from a folder's name to its files' paths,
    relative to packaging/. The README is not a recipe."""
    recipes = {}
    for folder, children, names in os.walk(PACKAGING):
        children.sort()
        for name in sorted(names):
            path = os.path.relpath(os.path.join(folder, name), PACKAGING).replace(os.sep, "/")
            if "/" in path:
                recipes.setdefault(path.split("/")[0], []).append(path)
    return recipes


def fill(version, sums_path, out):
    """Writes the filled recipes to out, folder by folder. Returns the names of the folders it wrote."""
    filled = values(version, sums_path)
    written = []
    for recipe, paths in templates().items():
        texts = {}
        for path in paths:
            with open(os.path.join(PACKAGING, path), encoding="utf-8") as file:
                texts[path] = file.read()
        missing = sorted({t for text in texts.values() for t in TOKEN.findall(text)} - filled.keys())
        if missing:
            print(f"packaging.py: skipped {recipe}: no {', '.join(missing)} (is its file in SHA256SUMS, and next to it?)")
            continue
        shutil.rmtree(os.path.join(out, recipe), ignore_errors=True)
        for path, text in texts.items():
            target = os.path.join(out, path)
            os.makedirs(os.path.dirname(target), exist_ok=True)
            with open(target, "w", encoding="utf-8", newline="\n") as file:
                file.write(TOKEN.sub(lambda found: filled[found.group(1)], text))
            shutil.copymode(os.path.join(PACKAGING, path), target)
        written.append(recipe)
    return written


def exported_targets():
    """The targets find_package(AffrescoUI) defines, from cmake/AffrescoUIInstall.cmake, as affrescoui::<name>."""
    with open(INSTALL, encoding="utf-8") as file:
        found = re.search(r"install\(TARGETS(.*?)EXPORT AffrescoUITargets", file.read(), re.DOTALL)
    if not found:
        sys.exit("packaging.py: no install(TARGETS ... EXPORT AffrescoUITargets) in cmake/AffrescoUIInstall.cmake")
    names = found.group(1).split()
    return {"affrescoui::" + (name[len("affrescoui_"):] if name.startswith("affrescoui_") else name) for name in names}


def check():
    """The problems that keep the recipes from matching the library: a list of messages."""
    problems = []
    with open(TEST_PACKAGE, encoding="utf-8") as file:
        tested = set(re.findall(r"affrescoui::[a-z0-9_]+", file.read()))
    exported = exported_targets()
    if tested != exported:
        problems.append(f"packaging/conan/test_package checks {sorted(tested - exported)} that the install doesn't "
                        f"export, and misses {sorted(exported - tested)}")
    for paths in templates().values():
        for path in paths:
            with open(os.path.join(PACKAGING, path), encoding="utf-8") as file:
                unknown = set(TOKEN.findall(file.read())) - set(TOKENS)
            problems += [f"packaging/{path}: @{token}@ is not a token packaging.py fills" for token in sorted(unknown)]
    return problems


def main():
    """Fills the recipes in, or checks them (--check)."""
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--version", help="the release's version, e.g. 0.2.0")
    parser.add_argument("--sums", help="the release's SHA256SUMS")
    parser.add_argument("--out", default=os.path.join(ROOT, "dist", "packages"), help="where the filled recipes go")
    parser.add_argument("--check", action="store_true", help="check the recipes against the library instead")
    args = parser.parse_args()
    if args.check:
        problems = check()
        for problem in problems:
            print(f"packaging.py: {problem}", file=sys.stderr)
        if problems:
            sys.exit(1)
        print("packaging.py: the recipes match the library's targets")
        return
    if not args.version or not args.sums:
        parser.error("say --version and --sums, or --check")
    if not re.fullmatch(r"\d+\.\d+\.\d+", args.version):
        sys.exit(f"packaging.py: {args.version} is not a version like 0.2.0")
    written = fill(args.version, args.sums, os.path.abspath(args.out))
    print(f"packaging.py: wrote {', '.join(written) or 'nothing'} to {args.out}")


if __name__ == "__main__":
    main()
