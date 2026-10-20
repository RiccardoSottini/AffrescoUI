#!/usr/bin/env bash
# The benchmarks in CI (.github/workflows/ci.yml, job "Benchmarks (callgrind)"). Builds HEAD and a base commit the same way, measures both on this
# runner (instructions under callgrind for the workloads that don't draw, the main thread's CPU time for the frames,
# tokens for the documents), then compare.py writes the tables and fails on a growth in instructions over 3%.
#
# Usage, from the repository root, under a display (xvfb-run): tests/perf/ci.sh
#
# Environment:
#   PERF_BASE   the base commit (default HEAD^1: a pull request's checkout is GitHub's merge commit, whose first parent
#               is the base branch); it must be in the clone
#   PERF_ALLOW  "true" reports growth without failing (the pull request has the perf-change label)
#   PERF_OUT    where the results go (default $RUNNER_TEMP/perf, else /tmp/perf)
#
# A base without the benchmarks only gets a notice: head's numbers are reported alone. Tokens are counted for both,
# with head's script and edits, since the base's templates and MCP server are all they need.
set -euo pipefail

base_commit="${PERF_BASE:-HEAD^1}"
out="${PERF_OUT:-${RUNNER_TEMP:-/tmp}/perf}"
base="${RUNNER_TEMP:-/tmp}/perf-base"
here="$(pwd)"

# Builds the benchmark of a checkout, the same way for base and head: optimised, nothing else built. More options
# may follow the checkout: head's warnings are errors, as no other job builds the benchmark (it changes no code).
build() {
	cmake -S "$1" -B "$1/build-perf" -G Ninja -DCMAKE_BUILD_TYPE=Release -DAFFRESCOUI_BUILD_BENCHMARKS=ON \
		-DAFFRESCOUI_BUILD_TESTS=OFF -DAFFRESCOUI_BUILD_EXAMPLES=OFF "${@:2}" || return 1
	cmake --build "$1/build-perf" --target affrescoui_bench || return 1
}

# Measures a checkout's build into $out/<side>: instructions (not the frames, which callgrind makes very slow with
# software OpenGL) and the frames' CPU time. Uses this checkout's run.py, which drives any version of the program.
measure() {
	python3 "$here/tests/perf/run.py" --build "$1/build-perf" --callgrind --skip frame --out "$out/$2/callgrind.jsonl" ||
		return 1
	python3 "$here/tests/perf/run.py" --build "$1/build-perf" --only frame --out "$out/$2/frame.jsonl" || return 1
}

rm -rf "$out"
mkdir -p "$out/base" "$out/head"

if ! git rev-parse -q --verify "$base_commit^{commit}" >/dev/null; then
	echo "::error title=Benchmarks::The base commit $base_commit is not in the clone"
	exit 1
fi
trap 'git worktree remove --force "$base" 2>/dev/null || true' EXIT # a second run would fail on the old one
git worktree add --detach "$base" "$base_commit"

build "$here" -DCMAKE_COMPILE_WARNING_AS_ERROR=ON
measure "$here" head
python3 tests/perf/tokens.py --out "$out/head/tokens.jsonl"
python3 tests/perf/tokens.py --root "$base" --out "$out/base/tokens.jsonl"

if [ ! -f "$base/tests/perf/bench.cpp" ]; then
	echo "::notice title=Benchmarks::The base commit has no benchmarks: head's numbers are reported alone"
elif ! { build "$base" && measure "$base" base; }; then
	echo "::error title=Benchmarks::The base commit $(git rev-parse --short "$base_commit") can't be built or measured"
	exit 1
fi

allow=()
if [ "${PERF_ALLOW:-}" = true ]; then
	echo "::notice title=Benchmarks::The perf-change label is set: growth is reported, not failed"
	allow=(--allow)
fi
python3 tests/perf/compare.py --base "$out/base" --head "$out/head" --summary "${GITHUB_STEP_SUMMARY:-/dev/null}" \
	"${allow[@]}"
