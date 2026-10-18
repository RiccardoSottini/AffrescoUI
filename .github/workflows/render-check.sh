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
# can't be built or checked fails the job: passing would test nothing.
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

# Builds the base commit's worktree and records the golden images from it; fails if it can't. Each command has its
# own "|| return 1": set -e is off in a function called as an if condition.
record_base() {
	cmake -S "$base" -B "$base/build" -G Ninja "$@" || return 1
	cmake --build "$base/build" --target affrescoui_render_runner || return 1 # all check.py needs
	python3 "$base/tests/render/check.py" --build "$base/build" || return 1
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
	echo "::error title=Rendering check::The base commit $(git rev-parse --short "$base_commit") can't be built or checked, so there is nothing to compare with"
	exit 1
fi

python3 tests/render/check.py
