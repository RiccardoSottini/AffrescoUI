#!/usr/bin/env bash
# The rendering check in CI. The golden images aren't in git, and their pixels depend on the GPU and driver, so this
# records them on this runner from a base commit, then checks HEAD against them. The base is $RENDER_CHECK_BASE:
# HEAD^1 for a pull request (the checkout is GitHub's merge commit, whose first parent is the PR's base), and the
# commit before the push for a push, so a push of several commits is compared as a whole.
#
# Usage, from the repository root once HEAD is built in build/:
#   .github/workflows/render-check.sh [options for configuring the base's build, e.g. -DCMAKE_BUILD_TYPE=Debug]
#
# Environment:
#   RENDER_CHECK_BASE    the base commit (default HEAD^1); it must be in the clone
#   RENDER_CHECK_RECORD  "true" records HEAD's frames instead of comparing them (a pull request labelled
#                        visual-change), and lists them in $GITHUB_STEP_SUMMARY
#
# A base that predates the check only gets a warning, and HEAD's frames are recorded. A base that has the check but
# can't be built, or can't record every picture its scenarios name, fails the job: passing would test nothing. A base
# whose own expectations fail only gets a warning: its pictures are compared all the same.
set -euo pipefail

base_commit="${RENDER_CHECK_BASE:-HEAD^1}"
base="${RUNNER_TEMP:-/tmp}/base"

# Records HEAD's frames as the golden images and lists them in the job summary.
record_head() {
	python3 tests/render/check.py --update
	{
		echo "### Rendering check: frames recorded, not compared"
		echo
		echo "The pull request has the \`visual-change\` label, so these are the frames of $(git rev-parse --short HEAD)."
		echo "They are in the \`render-check-*\` artifact of this run."
		echo
		for frame in tests/render/golden/*.png; do
			echo "- \`$(basename "$frame")\`"
		done
	} >>"${GITHUB_STEP_SUMMARY:-/dev/null}"
}

# Lists the golden images a commit's scenarios name ({"expect": {"image": NAME}} in scenario S is S-NAME.png) that
# its folder doesn't have.
# $1: the commit's tests/render folder.
missing_goldens() {
	python3 - "$1" <<'EOF'
import glob, json, os, sys

def images(value):
    if isinstance(value, dict):
        expect = value.get("expect")
        if isinstance(expect, dict) and isinstance(expect.get("image"), str):
            yield expect["image"]
        for child in value.values():
            yield from images(child)
    elif isinstance(value, list):
        for child in value:
            yield from images(child)

folder = sys.argv[1]
for path in sorted(glob.glob(os.path.join(folder, "scenarios", "*.json"))):
    with open(path, encoding="utf-8") as file:
        for scenario in json.load(file).get("scenarios", []):
            for image in images(scenario.get("steps", [])):
                name = f"{scenario['name']}-{image}.png"
                if not os.path.exists(os.path.join(folder, "golden", name)):
                    print(name)
EOF
}

# Builds the base commit's worktree and records the golden images from it; fails if it can't. Each command has its
# own "|| return 1": set -e is off in a function called as an if condition.
#
# The base's own check may fail on its own expectations (a broken master, which the pull request may be fixing): its
# pictures are still the base's, so they are compared all the same. What fails the job is a picture the base could
# not record: HEAD would record it in its place, and that frame would be compared with nothing.
record_base() {
	cmake -S "$base" -B "$base/build" -G Ninja "$@" || return 1
	cmake --build "$base/build" --target affrescoui_render_runner || return 1 # all check.py needs
	if ! python3 "$base/tests/render/check.py" --build "$base/build"; then
		echo "::warning title=Rendering check::The base commit's own check failed (above); its pictures are compared all the same"
	fi
	local missing
	missing=$(missing_goldens "$base/tests/render") || return 1
	if [ -n "$missing" ]; then
		echo "::error title=Rendering check::The base commit recorded no picture for: $(echo "$missing" | tr '\n' ' ')"
		return 1
	fi
	mkdir -p tests/render/golden
	cp "$base"/tests/render/golden/*.png tests/render/golden/
}

if [ "${RENDER_CHECK_RECORD:-}" = true ]; then
	echo "::notice title=Rendering check::The visual-change label is set: frames are recorded, not compared"
	record_head
	exit 0
fi

if ! git rev-parse -q --verify "$base_commit^{commit}" >/dev/null; then
	echo "::error title=Rendering check::The base commit $base_commit is not in the clone"
	exit 1
fi
trap 'git worktree remove --force "$base" 2>/dev/null || true' EXIT # a second run would fail on the old one
git worktree add --detach "$base" "$base_commit"

if [ ! -f "$base/tests/render/check.py" ]; then
	echo "::warning title=Rendering check::The base commit has no rendering check: frames are recorded, not compared"
elif record_base "$@"; then
	echo "Golden images recorded from the base commit $(git rev-parse --short "$base_commit")"
else
	short=$(git rev-parse --short "$base_commit")
	echo "::error title=Rendering check::The base commit $short can't be built or record its pictures: nothing to compare with"
	exit 1
fi

python3 tests/render/check.py
