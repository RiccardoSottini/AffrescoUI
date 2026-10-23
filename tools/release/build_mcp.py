#!/usr/bin/env python3
"""Builds the MCP server's Python package (affrescoui-mcp): a wheel and a source archive, in dist/ by default.

    pip install --require-hashes -r tools/release/requirements.txt
    python3 tools/release/build_mcp.py [--out DIR]

mcp/pyproject.toml describes the package; the files it ships live in several places of the repository, so they are
staged in a temporary folder first (the server's package, its README, the format guide it serves, LICENSE and NOTICE)
and built there with `python -m build --no-isolation`: the build tools are the pinned ones installed above, nothing
else is fetched. Then the wheel is checked: it must hold the server and the guide, and its entry point must be there.
"""

import argparse
import glob
import os
import shutil
import subprocess
import sys
import tempfile
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
STAGED = {"mcp/pyproject.toml": "pyproject.toml", "mcp/README.md": "README.md", "SCREEN_FORMAT.md": "SCREEN_FORMAT.md",
          "LICENSE": "LICENSE", "NOTICE": "NOTICE"}
PACKAGE = "mcp/affrescoui_mcp"  # the server's package folder, staged whole (without compiled files)


def check_wheel(path):
    """Fails unless the wheel has the server as a package, the guide next to it, and the console script."""
    with zipfile.ZipFile(path) as wheel:
        names = set(wheel.namelist())
        entry_points = next((wheel.read(n).decode() for n in names if n.endswith("entry_points.txt")), "")
    for needed in ("affrescoui_mcp/__init__.py", "affrescoui_mcp/server.py", "affrescoui_mcp/SCREEN_FORMAT.md"):
        if needed not in names:
            sys.exit(f"build_mcp.py: {os.path.basename(path)} has no {needed}")
    if "affrescoui-mcp = affrescoui_mcp:main" not in entry_points:
        sys.exit(f"build_mcp.py: {os.path.basename(path)} has no affrescoui-mcp command")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--out", default=os.path.join(ROOT, "dist"), help="where the wheel and archive go")
    args = parser.parse_args()
    out = os.path.abspath(args.out)

    with tempfile.TemporaryDirectory(prefix="affrescoui-mcp-") as stage:
        for source, target in STAGED.items():
            shutil.copyfile(os.path.join(ROOT, source), os.path.join(stage, target))
        shutil.copytree(os.path.join(ROOT, PACKAGE), os.path.join(stage, os.path.basename(PACKAGE)),
                        ignore=shutil.ignore_patterns("__pycache__", "*.pyc"))
        subprocess.run([sys.executable, "-m", "build", "--no-isolation", "--outdir", out, stage], check=True)

    wheels = glob.glob(os.path.join(out, "affrescoui_mcp-*.whl"))
    if not wheels or not glob.glob(os.path.join(out, "affrescoui_mcp-*.tar.gz")):
        sys.exit("build_mcp.py: the build made no wheel or no source archive")
    for wheel in wheels:
        check_wheel(wheel)
    print("\n".join(sorted(os.path.basename(p) for p in glob.glob(os.path.join(out, "affrescoui_mcp-*")))))


if __name__ == "__main__":
    main()
