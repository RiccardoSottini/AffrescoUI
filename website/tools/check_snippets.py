#!/usr/bin/env python3
"""Checks every screen the site shows: each recipe in website/recipes passes `affrescoui check`, each scenarios file
there passes `affrescoui test`, and no page writes a whole screen inline instead of including a checked recipe.

Usage: check_snippets.py --cli PATH
"""

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

WEBSITE = Path(__file__).resolve().parent.parent
RECIPES = WEBSITE / "recipes"
INCLUDE = re.compile(r'--8<--\s+"([^"]+)"')
BLOCK = re.compile(r"^```json[^\n]*\n(.*?)^```", re.M | re.S)
TIMEOUT = 60


def check_screen(cli, path):
    """Runs `affrescoui check --json` on a screen.

    @param cli   the command-line tool
    @param path  the screen file
    @return the problems, one line each; empty when it passes
    """
    run = subprocess.run([cli, "check", str(path), "--json"], capture_output=True, text=True, timeout=TIMEOUT)
    try:
        report = json.loads(run.stdout)
    except json.JSONDecodeError:
        return [f"{path.name}: affrescoui check said: {run.stdout.strip() or run.stderr.strip()}"]
    return [f"{path.name}: {p['source']}: {p['message']}" for p in report.get("problems", [])] or \
        ([] if report.get("ok") else [f"{path.name}: not ok"])


def check_scenarios(cli, path):
    """Runs `affrescoui test --json` on a scenarios file.

    @param cli   the command-line tool
    @param path  the scenarios file
    @return the failures, one line each; empty when every scenario passes
    """
    run = subprocess.run([cli, "test", str(path), "--json"], capture_output=True, text=True, timeout=TIMEOUT)
    if run.returncode == 0:
        return []
    return [f"{path.name}: {line}" for line in (run.stdout + run.stderr).strip().splitlines()] or \
        [f"{path.name}: affrescoui test exited with {run.returncode}"]


def check_pages():
    """Checks the pages' snippets: every included recipe exists, and no page has a whole screen inline.

    @return the problems, one line each
    """
    problems = []
    for page in sorted((WEBSITE / "pages").rglob("*.md")):
        text = page.read_text(encoding="utf-8")
        for name in INCLUDE.findall(text):
            if not (WEBSITE / name).is_file() and not (WEBSITE.parent / name).is_file():
                problems.append(f"{page.relative_to(WEBSITE)}: includes {name}, which doesn't exist")
        for block in BLOCK.findall(text):
            if '"affrescoui-screen"' in block and "--8<--" not in block:
                problems.append(f"{page.relative_to(WEBSITE)}: a whole screen inline; make it a recipe and "
                                "include it, so it is checked")
    return problems


def main():
    """Checks the snippets and exits with 1 on any problem."""
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--cli", required=True)
    args = parser.parse_args()

    screens = sorted(RECIPES.glob("*.screen.json"))
    scenarios = sorted(RECIPES.glob("*.scenarios.json"))
    problems = []
    for path in screens:
        problems += check_screen(args.cli, path)
    for path in scenarios:
        problems += check_scenarios(args.cli, path)
    problems += check_pages()
    for problem in problems:
        print(problem, file=sys.stderr)
    print(f"snippets: {len(screens)} screens, {len(scenarios)} scenarios files, {len(problems)} problems")
    sys.exit(1 if problems else 0)


if __name__ == "__main__":
    main()
