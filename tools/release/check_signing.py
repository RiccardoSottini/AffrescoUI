#!/usr/bin/env python3
"""Decides which platforms the release workflow signs, and checks that a platform being signed has its secrets
(RELEASING.md). The signing secrets live in the GitHub environment "release", which only v* tags may use and which
waits for the maintainer's approval, so only a tag's run is signed:

    macOS    MACOS_CERTIFICATE_P12, MACOS_CERTIFICATE_PASSWORD, MACOS_SIGNING_IDENTITY,
             APPLE_API_KEY_P8, APPLE_API_KEY_ID, APPLE_API_ISSUER_ID
    Windows  SIGNPATH_API_TOKEN, SIGNPATH_ORGANIZATION_ID, SIGNPATH_PROJECT_SLUG, SIGNPATH_SIGNING_POLICY_SLUG

    python3 tools/release/check_signing.py                       which platforms this run signs
    python3 tools/release/check_signing.py --secrets <platform>  fails, naming them, if the platform's secrets aren't set

The first form reads EVENT (github.event_name), REF_TYPE (github.ref_type) and UNSIGNED_PLATFORMS (the repository
variable RELEASE_UNSIGNED_PLATFORMS: "macos", "windows" or both, the platforms a tag may release unsigned), and writes
macos=true|false and windows=true|false to GITHUB_OUTPUT:
- a tag signs every platform, except one that UNSIGNED_PLATFORMS names;
- anything else (a pull request, a manual run, a push to a branch) is built unsigned: the environment refuses it.

The second form runs in the job that signs, with the environment's secrets: for each secret, an environment variable of
the same name that says "true" when it is set (never its value). A tag whose secrets are missing fails there, before
anything is built, rather than being released unsigned.

Both write a line to GITHUB_STEP_SUMMARY when it is set, and to standard output. Only the standard library is needed.
"""

import argparse
import os
import sys

SIGNING_INPUTS = {  # the names of the secrets each platform needs (names only)
    "macos": ("MACOS_CERTIFICATE_P12", "MACOS_CERTIFICATE_PASSWORD", "MACOS_SIGNING_IDENTITY", "APPLE_API_KEY_P8",
              "APPLE_API_KEY_ID", "APPLE_API_ISSUER_ID"),
    "windows": ("SIGNPATH_API_TOKEN", "SIGNPATH_ORGANIZATION_ID", "SIGNPATH_PROJECT_SLUG",
                "SIGNPATH_SIGNING_POLICY_SLUG"),
}
HOW = {"macos": "Developer ID, notarized and stapled", "windows": "SignPath (Authenticode)"}


def decide(event, ref_type, unsigned_allowed):
    """Which platforms to sign.

    event, ref_type: github.event_name and github.ref_type. unsigned_allowed: the platforms a tag may release
    unsigned. Returns (sign, lines): a dictionary from each platform to whether it is signed, and one line per
    platform for the summary."""
    sign, lines = {}, []
    for platform in SIGNING_INPUTS:
        if event == "pull_request":
            sign[platform], why = False, "unsigned: a pull request is never signed"
        elif ref_type != "tag":
            sign[platform], why = False, "unsigned: only a v* tag is signed (the release environment's secrets)"
        elif platform in unsigned_allowed:
            sign[platform], why = False, "unsigned: allowed by RELEASE_UNSIGNED_PLATFORMS"
        else:
            sign[platform], why = True, "signed: " + HOW[platform] + ", once the release environment is approved"
        lines.append(f"| {platform} | {why} |")
    sigstore = "signed: a Sigstore bundle for every file" if ref_type == "tag" else \
        "not signed: Sigstore signs a tag's release only"
    lines.append(f"| every file (Sigstore) | {sigstore} |")
    return sign, lines


def missing(platform, present):
    """The secrets of a platform that aren't set, given the names of those that are."""
    return [name for name in SIGNING_INPUTS[platform] if name not in present]


def summary(text):
    """Writes text to standard output and to the job's summary."""
    print(text)
    if os.environ.get("GITHUB_STEP_SUMMARY"):
        with open(os.environ["GITHUB_STEP_SUMMARY"], "a", encoding="utf-8") as file:
            file.write(text + "\n")


def main():
    """Writes the decision, or checks one platform's secrets (--secrets)."""
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--secrets", dest="platform", metavar="PLATFORM", choices=sorted(SIGNING_INPUTS),
                        help="check that this platform's secrets are set")
    args = parser.parse_args()
    if args.platform:
        present = {name for name in SIGNING_INPUTS[args.platform] if os.environ.get(name) == "true"}
        absent = missing(args.platform, present)
        if absent:
            summary(f"{args.platform}: unsigned: secret {', '.join(absent)} missing in the release environment")
            print(f"::error::check_signing.py: a tag must be signed, and the release environment has no "
                  f"{', '.join(absent)} (RELEASING.md); to release {args.platform} unsigned, name it in the repository "
                  f"variable RELEASE_UNSIGNED_PLATFORMS", file=sys.stderr)
            sys.exit(1)
        summary(f"{args.platform}: every signing secret is set")
        return
    allowed = set(os.environ.get("UNSIGNED_PLATFORMS", "").replace(",", " ").lower().split())
    sign, lines = decide(os.environ.get("EVENT", ""), os.environ.get("REF_TYPE", ""), allowed)
    summary("\n".join(["### Signing", "", "| Platform | |", "|---|---|", *lines, ""]))
    if os.environ.get("GITHUB_OUTPUT"):
        with open(os.environ["GITHUB_OUTPUT"], "a", encoding="utf-8") as file:
            for platform, signed in sign.items():
                file.write(f"{platform}={'true' if signed else 'false'}\n")


if __name__ == "__main__":
    main()
