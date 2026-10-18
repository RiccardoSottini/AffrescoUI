#!/usr/bin/env bash
# The rendering check in CI. The golden images aren't in git, and their pixels depend on the GPU and driver, so this
# records them on this runner from the base commit, HEAD^1 (for a pull request, the checkout is GitHub's merge commit
# and HEAD^1 is the PR's base; for a push, it is the previous commit), then checks HEAD against them.
#
# Usage, from the repository root once HEAD is built in build/:
#   .github/workflows/render-check.sh [options for configuring the base's build, e.g. -DCMAKE_BUILD_TYPE=Debug]
#
# If the base can't be built or checked (it predates the check, or doesn't build on this system yet), a warning says
# so and the check records the missing golden images from HEAD, which then tests nothing.
set -euo pipefail

base="${RUNNER_TEMP:-/tmp}/base"

record_base() {
	git rev-parse -q --verify HEAD^1 >/dev/null || return 1
	git worktree add --detach "$base" HEAD^1 || return 1
	[ -f "$base/tests/render/check.py" ] || return 1
	cmake -S "$base" -B "$base/build" -G Ninja "$@" || return 1
	cmake --build "$base/build" --target affrescoui_render_runner || return 1 # all check.py needs
	python3 "$base/tests/render/check.py" --build "$base/build" || return 1
	mkdir -p tests/render/golden
	cp "$base"/tests/render/golden/*.png tests/render/golden/
}

if record_base "$@"; then
	echo "Golden images recorded from the base commit $(git rev-parse --short HEAD^1)"
else
	echo "::warning title=Rendering check::No golden images from the base commit: frames are recorded, not compared"
fi

python3 tests/render/check.py
