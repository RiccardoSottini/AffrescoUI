#!/usr/bin/env python3
"""Decides which platforms the release workflow signs, from which of their secrets are set (RELEASING.md):

    macOS    MACOS_CERTIFICATE_P12, MACOS_CERTIFICATE_PASSWORD, MACOS_SIGNING_IDENTITY,
             APPLE_API_KEY_P8, APPLE_API_KEY_ID, APPLE_API_ISSUER_ID
    Windows  SIGNPATH_API_TOKEN, SIGNPATH_ORGANIZATION_ID, SIGNPATH_PROJECT_SLUG, SIGNPATH_SIGNING_POLICY_SLUG

    python3 tools/release/check_signing.py

The workflow passes, for each secret, an environment variable of the same name that says "true" when it is set (never
its value), and EVENT (github.event_name), REF_TYPE (github.ref_type) and UNSIGNED_PLATFORMS (the repository variable
RELEASE_UNSIGNED_PLATFORMS: "macos", "windows" or both, the platforms a tag may release unsigned).

- A platform with every secret is signed, except on a pull request: a pull request's code is never signed.
- A platform with a secret missing is built unsigned, and the summary says which secret is missing.
- On a tag, a platform that would be unsigned fails the run, unless UNSIGNED_PLATFORMS names it: a release never
  goes out unsigned without the maintainer having said so.

It writes macos=true|false and windows=true|false to GITHUB_OUTPUT, a table to GITHUB_STEP_SUMMARY (both when set),
and the same to standard output. Only the standard library is needed.
"""

import os
import sys

SECRETS = {
    "macos": ("MACOS_CERTIFICATE_P12", "MACOS_CERTIFICATE_PASSWORD", "MACOS_SIGNING_IDENTITY", "APPLE_API_KEY_P8",
              "APPLE_API_KEY_ID", "APPLE_API_ISSUER_ID"),
    "windows": ("SIGNPATH_API_TOKEN", "SIGNPATH_ORGANIZATION_ID", "SIGNPATH_PROJECT_SLUG",
                "SIGNPATH_SIGNING_POLICY_SLUG"),
}
HOW = {"macos": "Developer ID, notarized and stapled", "windows": "SignPath (Authenticode)"}


def decide(event, ref_type, present, unsigned_allowed):
    """Which platforms to sign.

    event, ref_type: github.event_name and github.ref_type. present: the names of the secrets that are set.
    unsigned_allowed: the platforms a tag may release unsigned.
    Returns (sign, lines, failed): a dictionary from each platform to whether it is signed, one line per platform
    for the summary, and whether the run must fail."""
    sign, lines, failed = {}, [], False
    for platform, names in SECRETS.items():
        missing = [name for name in names if name not in present]
        if event == "pull_request":
            sign[platform], why = False, "unsigned: a pull request is never signed"
        elif missing:
            sign[platform], why = False, "unsigned: secret " + ", ".join(missing) + " missing"
        else:
            sign[platform], why = True, "signed: " + HOW[platform]
        if ref_type == "tag" and not sign[platform]:
            if platform in unsigned_allowed:
                why += " (allowed by RELEASE_UNSIGNED_PLATFORMS)"
            else:
                why += "; a tag must be signed: set the secrets (RELEASING.md), or name the platform in the " \
                       "repository variable RELEASE_UNSIGNED_PLATFORMS to release it unsigned"
                failed = True
        lines.append(f"| {platform} | {why} |")
    sigstore = "signed: a Sigstore bundle for every file" if ref_type == "tag" else \
        "not signed: Sigstore signs a tag's release only (a manual run signs the Linux archive, as a test)"
    lines.append(f"| every file (Sigstore) | {sigstore} |")
    return sign, lines, failed


def main():
    """Reads the environment, writes the decision, and fails a tag that would be unsigned."""
    present = {name for names in SECRETS.values() for name in names if os.environ.get(name) == "true"}
    allowed = set(os.environ.get("UNSIGNED_PLATFORMS", "").replace(",", " ").lower().split())
    sign, lines, failed = decide(os.environ.get("EVENT", ""), os.environ.get("REF_TYPE", ""), present, allowed)
    table = "\n".join(["### Signing", "", "| Platform | |", "|---|---|", *lines, ""])
    print(table)
    if os.environ.get("GITHUB_STEP_SUMMARY"):
        with open(os.environ["GITHUB_STEP_SUMMARY"], "a", encoding="utf-8") as file:
            file.write(table + "\n")
    if os.environ.get("GITHUB_OUTPUT"):
        with open(os.environ["GITHUB_OUTPUT"], "a", encoding="utf-8") as file:
            for platform, signed in sign.items():
                file.write(f"{platform}={'true' if signed else 'false'}\n")
    if failed:
        print("::error::check_signing.py: a tag would be released unsigned (see the table above)", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
