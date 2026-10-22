#!/usr/bin/env python3
"""Keeps the list of third-party components, cmake/dependencies.json, true, and writes what a release says about them:
an SPDX 2.3 software bill of materials and THIRD_PARTY_NOTICES.md.

    python3 tools/release/sbom.py --check              fails on a missing pin, a hash or version that doesn't match,
                                                       a licence that isn't permissive, or notices that drifted
    python3 tools/release/sbom.py --notices            writes THIRD_PARTY_NOTICES.md from the manifest
    python3 tools/release/sbom.py --sbom out.spdx.json writes the SBOM (SOURCE_DATE_EPOCH, if set, dates it)

What --check compares the manifest with: every FetchContent_Declare in the CMake files (each must have a URL_HASH,
listed in the manifest), the vendored files (external/ and every font in the tree), and the hashed requirements of
tools/release/requirements.txt. A licence that isn't permissive passes only with a "licenseReview" saying why it is
acceptable. Only the standard library is needed.
"""

import argparse
import datetime
import hashlib
import json
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
MANIFEST = "cmake/dependencies.json"
NOTICES = "THIRD_PARTY_NOTICES.md"
KINDS = ("downloaded", "vendored", "python")
SCOPES = ("runtime", "build", "test")
REQUIRED = ("name", "version", "purpose", "kind", "url", "license", "licenseNote", "usedBy", "scope")
SHA256 = re.compile(r"^[0-9a-f]{64}$")
# Licences that ask for no more than attribution and the licence text: SPDX identifiers, plus public domain
PERMISSIVE = {
    "0BSD", "Apache-2.0", "BSD-2-Clause", "BSD-3-Clause", "BSL-1.0", "CC-BY-4.0", "CC0-1.0", "FTL", "ISC", "MIT",
    "MIT-Modern-Variant", "OFL-1.1", "PSF-2.0", "Unlicense", "Zlib", "curl", "LicenseRef-Public-Domain",
}
EXCEPTIONS = {"PCRE2-exception", "LLVM-exception"}  # exceptions that only grant more
NOTICES_HEAD = """# Third-party notices

<!-- Written by tools/release/sbom.py --notices from cmake/dependencies.json: edit those, not this file. -->

AffrescoUI is licensed under the Apache License 2.0 (see `LICENSE` and `NOTICE`). It includes or downloads the
following components, which keep their own licenses:
"""
NOTICES_TAIL = """
Portions of this software are copyright © 2026 The FreeType Project (https://freetype.org). All rights reserved.

Versions published before this change (under the name sGLGUI) were licensed under the GNU LGPL 3.0; those copies keep that license.
"""


def read_manifest(root):
    """The manifest's dependencies, as a list of dictionaries."""
    with open(os.path.join(root, MANIFEST), encoding="utf-8") as file:
        return json.load(file)["dependencies"]


def read_text(root, path):
    """A file of the tree as text, or "" if it isn't there."""
    try:
        with open(os.path.join(root, path), encoding="utf-8") as file:
            return file.read()
    except OSError:
        return ""


def file_hash(path, algorithm="sha256"):
    """A file's hash, in lower-case hexadecimal."""
    digest = hashlib.new(algorithm)
    with open(path, "rb") as file:
        for block in iter(lambda: file.read(1 << 16), b""):
            digest.update(block)
    return digest.hexdigest()


def tree_files(root):
    """The files of the tree, as paths relative to it with forward slashes: what git tracks, or else every file
    outside build folders and hidden folders."""
    try:
        listed = subprocess.run(["git", "-C", root, "ls-files"], capture_output=True, text=True, check=True)
        if listed.stdout.strip():
            return [line for line in listed.stdout.splitlines() if os.path.isfile(os.path.join(root, line))]
    except (OSError, subprocess.CalledProcessError):
        pass
    found = []
    for folder, children, names in os.walk(root):
        children[:] = [c for c in children if not c.startswith((".", "build", "_deps", "out"))]
        found += [os.path.relpath(os.path.join(folder, n), root).replace(os.sep, "/") for n in names]
    return found


def licence_tokens(expression):
    """An SPDX licence expression cut into identifiers, operators and parentheses."""
    return re.findall(r"\(|\)|[A-Za-z0-9.+:-]+", expression)


def licence_permissive(expression):
    """Whether an SPDX licence expression is permissive: an OR needs one permissive side, an AND both, and a WITH
    a permissive licence and a known exception. Raises ValueError on an expression that doesn't parse."""
    tokens = licence_tokens(expression)
    position = 0

    def term():
        nonlocal position
        if position >= len(tokens):
            raise ValueError(f"incomplete licence expression: {expression}")
        token = tokens[position]
        position += 1
        if token == "(":
            value = either()
            if position >= len(tokens) or tokens[position] != ")":
                raise ValueError(f"unbalanced licence expression: {expression}")
            position += 1
            return value
        if token in ("AND", "OR", "WITH", ")"):
            raise ValueError(f"unexpected {token} in licence expression: {expression}")
        value = token in PERMISSIVE
        if position < len(tokens) and tokens[position] == "WITH":
            if position + 1 >= len(tokens):
                raise ValueError(f"WITH without an exception: {expression}")
            value = value and tokens[position + 1] in EXCEPTIONS
            position += 2
        return value

    def both():
        nonlocal position
        value = term()
        while position < len(tokens) and tokens[position] == "AND":
            position += 1
            value = term() and value
        return value

    def either():
        nonlocal position
        value = both()
        while position < len(tokens) and tokens[position] == "OR":
            position += 1
            value = both() or value
        return value

    result = either()
    if position != len(tokens):
        raise ValueError(f"unexpected {tokens[position]} in licence expression: {expression}")
    return result


def check_entries(dependencies):
    """Problems with the manifest's own entries: missing fields, unknown kinds, malformed hashes, repeated names and
    licences that aren't permissive without a review. Returns a list of messages."""
    problems = []
    seen = set()
    for entry in dependencies:
        name = entry.get("name", "?")
        problems += [f"{name}: no {field}" for field in REQUIRED if not entry.get(field)]
        if name in seen:
            problems.append(f"{name}: listed twice")
        seen.add(name)
        if entry.get("kind") not in KINDS:
            problems.append(f"{name}: kind is not one of {', '.join(KINDS)}")
        if entry.get("scope") not in SCOPES:
            problems.append(f"{name}: scope is not one of {', '.join(SCOPES)}")
        files = entry.get("files", [])
        hashes = [f.get("sha256") for f in files] if entry.get("kind") == "vendored" else [entry.get("sha256")]
        if entry.get("kind") == "vendored" and not files:
            problems.append(f"{name}: a vendored component lists no files")
        if entry.get("kind") in ("downloaded", "python") and not entry.get("pinnedIn"):
            problems.append(f"{name}: no pinnedIn")
        problems += [f"{name}: missing pin (no SHA-256)" for h in hashes if not h or not SHA256.match(h)]
        problems += check_licence(entry)
    return problems


def check_licence(entry):
    """The licence problems of one entry: an expression that doesn't parse, or one that isn't permissive and has no
    "licenseReview". Returns a list of messages."""
    name = entry.get("name", "?")
    try:
        permissive = licence_permissive(entry.get("license", ""))
    except ValueError as error:
        return [f"{name}: {error}"]
    if permissive:
        return []
    if entry.get("licenseReview"):
        print(f"sbom.py: note: {name} is {entry['license']}, not permissive: {entry['licenseReview']}")
        return []
    return [f"{name}: {entry['license']} is not a permissive licence (add a licenseReview if it is acceptable)"]


def declared_downloads(root, files):
    """The FetchContent_Declare blocks of the CMake files, as (file, name, SHA-256 or None) tuples."""
    found = []
    for path in files:
        if not (path.endswith(".cmake") or os.path.basename(path) == "CMakeLists.txt"):
            continue
        for block in re.finditer(r"FetchContent_Declare\(\s*(\w+)(.*?)\)", read_text(root, path), re.DOTALL):
            pinned = re.search(r"URL_HASH\s+SHA256=([0-9a-fA-F]{64})", block.group(2))
            found.append((path, block.group(1), pinned.group(1).lower() if pinned else None))
    return found


def check_downloads(root, dependencies, files):
    """Problems with what CMake downloads: a download with no hash or not in the manifest, and a manifest entry whose
    hash or version isn't where it says it is pinned. Returns a list of messages."""
    problems = []
    listed = {e["sha256"]: e for e in dependencies if e.get("kind") == "downloaded"}
    for path, name, pinned in declared_downloads(root, files):
        if not pinned:
            problems.append(f"{path}: {name} is downloaded without a URL_HASH SHA256 (missing pin)")
        elif pinned not in listed:
            problems.append(f"{path}: {name} ({pinned[:12]}...) is not in {MANIFEST}")
    for entry in dependencies:
        if entry.get("kind") not in ("downloaded", "vendored") or not entry.get("pinnedIn"):
            continue
        text = read_text(root, entry["pinnedIn"])
        hashes = [entry["sha256"]] if entry["kind"] == "downloaded" else [f["sha256"] for f in entry["files"]]
        for value in hashes:
            if value not in text:
                problems.append(f"{entry['name']}: hash mismatch: {value[:12]}... is not in {entry['pinnedIn']}")
        if entry["kind"] == "downloaded" and entry["version"] not in text + entry["url"]:
            problems.append(f"{entry['name']}: version {entry['version']} is not in {entry['pinnedIn']}")
    return problems


def check_vendored(root, dependencies, files):
    """Problems with vendored files: a hash that doesn't match the file, a listed file that is gone, and a font or a
    file under external/ the manifest doesn't list. Returns a list of messages."""
    problems = []
    listed = set()
    for entry in dependencies:
        for item in entry.get("files", []):
            listed.add(item["path"])
            path = os.path.join(root, item["path"])
            if not os.path.isfile(path):
                problems.append(f"{entry['name']}: {item['path']} is missing")
            elif file_hash(path) != item["sha256"]:
                problems.append(f"{entry['name']}: hash mismatch: {item['path']} has {file_hash(path)}")
    for path in files:
        third_party = path.startswith("external/") and os.path.basename(path) != "README.md"
        font = path.lower().endswith((".ttf", ".otf", ".ttc", ".woff", ".woff2"))
        if (third_party or font) and path not in listed:
            problems.append(f"{path}: vendored but not in {MANIFEST}")
    return problems


def check_python(root, dependencies):
    """Problems with the hashed Python requirements: a requirement not in the manifest, one whose version or hash
    differs, and a manifest entry no requirements file pins. Returns a list of messages."""
    problems = []
    entries = {(e["pinnedIn"], e["name"].lower()): e for e in dependencies if e.get("kind") == "python"}
    pinned = set()
    for path in sorted({e["pinnedIn"] for e in entries.values()} | {"tools/release/requirements.txt"}):
        for line in read_text(root, path).replace("\\\n", " ").splitlines():  # joins continued lines
            found = re.match(r"\s*([A-Za-z0-9_.-]+)==(\S+)(.*)", line)
            if not found:
                continue
            name, version, rest = found.group(1).lower(), found.group(2), found.group(3)
            hashes = re.findall(r"--hash=sha256:([0-9a-f]{64})", rest)
            entry = entries.get((path, name))
            pinned.add((path, name))
            if not entry:
                problems.append(f"{path}: {name} is not in {MANIFEST}")
            elif entry["version"] != version:
                problems.append(f"{name}: version {entry['version']} in {MANIFEST}, {version} in {path}")
            elif entry["sha256"] not in hashes:
                problems.append(f"{name}: hash mismatch with {path}")
    problems += [f"{name}: not pinned in {path}" for (path, name) in entries if (path, name) not in pinned]
    return problems


def where(entry):
    """The "Where" cell of a component in the notices."""
    if entry["kind"] == "vendored":
        folders = sorted({os.path.dirname(f["path"]) + "/" for f in entry["files"]})
        paths = [f["path"] for f in entry["files"]] if len(entry["files"]) == 1 else folders
        return ", ".join(f"`{p}`" for p in paths) + " (vendored, SHA-256 checked by sbom.py)"
    if entry["kind"] == "python":
        return f"pinned by hash in `{entry['pinnedIn']}`; used to build, not shipped"
    if entry["pinnedIn"].startswith(".github/"):
        return f"downloaded by a workflow, SHA-256 checked (`{entry['pinnedIn']}`)"
    return f"downloaded by CMake, SHA-256 checked (`{entry['pinnedIn']}`)"


def notices(dependencies):
    """THIRD_PARTY_NOTICES.md as it should be: one row per component, in the manifest's order."""
    escape = lambda text: text.replace("|", "\\|")
    rows = ["| Component | Where | Used for | License |", "|---|---|---|---|"]
    for entry in dependencies:
        component = f"{entry['name']} {entry['version']} ({entry['purpose']})"
        used = f"{entry['scope']}: " + ", ".join(entry["usedBy"])
        licence = f"{entry['licenseNote']}; SPDX: `{entry['license']}`"
        rows.append(f"| {escape(component)} | {escape(where(entry))} | {escape(used)} | {escape(licence)} |")
    return NOTICES_HEAD + "\n" + "\n".join(rows) + "\n" + NOTICES_TAIL


def check_notices(root, dependencies):
    """A problem if THIRD_PARTY_NOTICES.md isn't what the manifest gives. Returns a list of messages."""
    if read_text(root, NOTICES) == notices(dependencies):
        return []
    return [f"{NOTICES} drifted from {MANIFEST}: run tools/release/sbom.py --notices"]


def check(root):
    """Every check, on a tree. Returns the list of problems (empty when all is well)."""
    try:
        dependencies = read_manifest(root)
    except (OSError, ValueError, KeyError) as error:
        return [f"{MANIFEST}: {error}"]
    problems = check_entries(dependencies)
    if problems:
        return problems
    files = tree_files(root)
    problems += check_downloads(root, dependencies, files)
    problems += check_vendored(root, dependencies, files)
    problems += check_python(root, dependencies)
    problems += check_notices(root, dependencies)
    return problems


def project_version(root):
    """The library's version (CMakeLists.txt, project VERSION)."""
    found = re.search(r"project\(AffrescoUI VERSION (\d+\.\d+\.\d+)", read_text(root, "CMakeLists.txt"))
    return found.group(1) if found else "0.0.0"


def spdx_id(name):
    """An SPDX element id for a component's name."""
    return "SPDXRef-Package-" + re.sub(r"[^A-Za-z0-9.-]+", "-", name).strip("-")


def spdx_package(root, entry):
    """A component as an SPDX 2.3 package, and its vendored files as SPDX files: (package, files)."""
    package = {
        "SPDXID": spdx_id(entry["name"]), "name": entry["name"], "versionInfo": entry["version"],
        "downloadLocation": entry["url"], "filesAnalyzed": False, "licenseConcluded": entry["license"],
        "licenseDeclared": entry.get("licenseDeclared", entry["license"]), "copyrightText": "NOASSERTION",
        "supplier": "NOASSERTION", "comment": f"{entry['purpose']}; used by " + ", ".join(entry["usedBy"]),
    }
    if entry["kind"] != "vendored":
        package["checksums"] = [{"algorithm": "SHA256", "checksumValue": entry["sha256"]}]
    files = []
    for item in entry.get("files", []):
        path = os.path.join(root, item["path"])
        files.append({
            "SPDXID": "SPDXRef-File-" + re.sub(r"[^A-Za-z0-9.-]+", "-", item["path"]),
            "fileName": "./" + item["path"],
            "checksums": [{"algorithm": "SHA1", "checksumValue": file_hash(path, "sha1")},
                          {"algorithm": "SHA256", "checksumValue": item["sha256"]}],
            "licenseConcluded": entry["license"], "copyrightText": "NOASSERTION",
        })
    return package, files


def sbom(root, created):
    """The SPDX 2.3 document for a tree, created at the given UTC time (a datetime)."""
    version = project_version(root)
    dependencies = read_manifest(root)
    manifest_hash = file_hash(os.path.join(root, MANIFEST))[:16]
    top = {
        "SPDXID": "SPDXRef-Package-AffrescoUI", "name": "AffrescoUI", "versionInfo": version,
        "downloadLocation": "https://github.com/RiccardoSottini/AffrescoUI", "filesAnalyzed": False,
        "licenseConcluded": "Apache-2.0", "licenseDeclared": "Apache-2.0", "copyrightText": "NOASSERTION",
        "supplier": "NOASSERTION",
    }
    packages, files = [top], []
    relationships = [{"spdxElementId": "SPDXRef-DOCUMENT", "relationshipType": "DESCRIBES",
                      "relatedSpdxElement": top["SPDXID"]}]
    kinds = {"runtime": "DEPENDS_ON", "build": "BUILD_DEPENDENCY_OF", "test": "TEST_DEPENDENCY_OF"}
    for entry in dependencies:
        package, contained = spdx_package(root, entry)
        packages.append(package)
        files += contained
        if entry["scope"] == "runtime":
            relationships.append({"spdxElementId": top["SPDXID"], "relationshipType": "DEPENDS_ON",
                                  "relatedSpdxElement": package["SPDXID"]})
        else:
            relationships.append({"spdxElementId": package["SPDXID"], "relationshipType": kinds[entry["scope"]],
                                  "relatedSpdxElement": top["SPDXID"]})
        relationships += [{"spdxElementId": package["SPDXID"], "relationshipType": "CONTAINS",
                           "relatedSpdxElement": f["SPDXID"]} for f in contained]
    return {
        "spdxVersion": "SPDX-2.3", "dataLicense": "CC0-1.0", "SPDXID": "SPDXRef-DOCUMENT",
        "name": f"AffrescoUI-{version}",
        "documentNamespace": f"https://github.com/RiccardoSottini/AffrescoUI/spdx/{version}-{manifest_hash}",
        "creationInfo": {"created": created.strftime("%Y-%m-%dT%H:%M:%SZ"), "creators": ["Tool: affrescoui-sbom.py"]},
        "packages": packages, "files": files, "relationships": relationships,
    }


def creation_time():
    """When the SBOM is made: SOURCE_DATE_EPOCH if it is set (reproducible builds), otherwise now, in UTC."""
    epoch = os.environ.get("SOURCE_DATE_EPOCH")
    if epoch:
        return datetime.datetime.fromtimestamp(int(epoch), datetime.timezone.utc)
    return datetime.datetime.now(datetime.timezone.utc)


def main():
    """Runs what the command line asks: --check, --notices and --sbom, in that order."""
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--check", action="store_true", help="check the manifest against the tree")
    parser.add_argument("--notices", action="store_true", help=f"write {NOTICES}")
    parser.add_argument("--sbom", metavar="FILE", help="write the SPDX 2.3 JSON SBOM to FILE")
    args = parser.parse_args()
    if not (args.check or args.notices or args.sbom):
        parser.error("say --check, --notices or --sbom FILE")
    if args.check:
        problems = check(ROOT)
        for problem in problems:
            print(f"sbom.py: {problem}", file=sys.stderr)
        if problems:
            sys.exit(1)
        print(f"sbom.py: {MANIFEST} matches the tree")
    if args.notices:
        with open(os.path.join(ROOT, NOTICES), "w", encoding="utf-8", newline="\n") as file:
            file.write(notices(read_manifest(ROOT)))
    if args.sbom:
        with open(args.sbom, "w", encoding="utf-8", newline="\n") as file:
            json.dump(sbom(ROOT, creation_time()), file, indent=2)
            file.write("\n")


if __name__ == "__main__":
    main()
