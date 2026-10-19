#!/usr/bin/env bash
# The quality gates and reports (tools/quality/README.md), the same on macOS, on Linux and in CI.
#
# Usage, from anywhere in the repository:
#   tools/quality/run.sh gate [BASE]  clang-tidy and function size on what changed since BASE, uncommitted changes
#                                     included (default BASE: where HEAD left origin/master); fails on a new problem
#   tools/quality/run.sh coverage     builds and runs the unit tests with clang's source-based coverage
#   tools/quality/run.sh report       the whole repository: the longest and most complex functions, tidy findings per
#                                     module, and the coverage if `coverage` ran before
#   tools/quality/run.sh all          gate, coverage and report
#   tools/quality/run.sh test         the unit tests of these tools (test_quality.py)
#
# Environment:
#   LLVM_BIN       the folder with clang++, llvm-profdata and llvm-cov (found if unset: Xcode's on macOS, the newest
#                  /usr/lib/llvm-*/bin on Linux)
#   CLANG_TIDY     the clang-tidy executable (found if unset: in LLVM_BIN, on the PATH, then Homebrew's llvm)
#   QUALITY_BUILD  the build directory (default build-quality)
#   QUALITY_JOBS   parallel clang-tidy runs and build jobs (default 4)
#
# Output, in the build directory: quality-report.json, and coverage/ (coverage.json, coverage.lcov, html/).
set -euo pipefail

root=$(git rev-parse --show-toplevel)
here="$root/tools/quality"
build="${QUALITY_BUILD:-$root/build-quality}"
jobs="${QUALITY_JOBS:-4}"

# Prints an error and stops.
fail() {
	echo "run.sh: $*" >&2
	exit 1
}

# Finds the LLVM tools that build with coverage: the compiler and llvm-profdata must come from the same release.
find_llvm() {
	if [ -n "${LLVM_BIN:-}" ]; then
		echo "$LLVM_BIN"
	elif [ "$(uname)" = Darwin ]; then
		dirname "$(xcrun -f llvm-cov)"
	else
		# The newest installed release, e.g. /usr/lib/llvm-18/bin (version-sorted, last)
		find /usr/lib -maxdepth 1 -name 'llvm-*' -type d 2>/dev/null | sort -V | tail -n 1 | sed 's|$|/bin|'
	fi
}

# Finds clang-tidy: any release reads .clang-tidy, though newer ones know more checks.
find_clang_tidy() {
	local candidate
	for candidate in "${CLANG_TIDY:-}" "$llvm/clang-tidy" "$(command -v clang-tidy || true)" \
		/opt/homebrew/opt/llvm/bin/clang-tidy /usr/local/opt/llvm/bin/clang-tidy; do
		if [ -n "$candidate" ] && [ -x "$candidate" ]; then
			echo "$candidate"
			return
		fi
	done
	fail "clang-tidy not found: brew install llvm (macOS), apt install clang-tidy (Linux), or set CLANG_TIDY"
}

# A virtual environment in the build directory with lizard, installed by hash (requirements.txt).
setup_python() {
	if [ ! -x "$build/venv/bin/python" ]; then
		python3 -m venv "$build/venv"
	fi
	"$build/venv/bin/python" -m pip install --quiet --disable-pip-version-check --require-hashes \
		-r "$here/requirements.txt"
}

# Configures the build with clang, coverage instrumentation and compile_commands.json (what clang-tidy reads). The
# benchmarks are on so tests/perf is in it too: a file missing from it can't be parsed, so it can't be checked.
configure() {
	local flags="-fprofile-instr-generate -fcoverage-mapping"
	cmake -S "$root" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
		-DAFFRESCOUI_BUILD_BENCHMARKS=ON \
		-DCMAKE_C_COMPILER="$llvm/clang" -DCMAKE_CXX_COMPILER="$llvm/clang++" \
		"-DCMAKE_C_FLAGS=$flags" "-DCMAKE_CXX_FLAGS=$flags" >/dev/null
}

# The gate on the changes since $1 (default: the merge base with origin/master).
gate() {
	local base="${1:-$(git -C "$root" merge-base origin/master HEAD)}"
	"$build/venv/bin/python" "$here/quality.py" gate --base "$base" --build "$build" --clang-tidy "$clang_tidy" \
		--jobs "$jobs"
}

# Builds and runs the unit tests with coverage, then writes coverage.json, coverage.lcov and an HTML report.
coverage() {
	local out="$build/coverage"
	local tests="$build/affrescoui_unit_tests"
	local ignore='(_deps|external|tests)/'
	cmake --build "$build" --target affrescoui_unit_tests -j "$jobs"
	rm -rf "$out"
	mkdir -p "$out"
	LLVM_PROFILE_FILE="$out/unit-%p.profraw" "$tests"
	"$llvm/llvm-profdata" merge -sparse -o "$out/unit.profdata" "$out"/unit-*.profraw
	"$llvm/llvm-cov" export -summary-only -instr-profile "$out/unit.profdata" "$tests" \
		-ignore-filename-regex="$ignore" >"$out/export.json"
	"$llvm/llvm-cov" export -format=lcov -instr-profile "$out/unit.profdata" "$tests" \
		-ignore-filename-regex="$ignore" >"$out/coverage.lcov"
	"$llvm/llvm-cov" show -format=html -output-dir "$out/html" -instr-profile "$out/unit.profdata" "$tests" \
		-ignore-filename-regex="$ignore" -show-line-counts-or-regions
	"$build/venv/bin/python" "$here/quality.py" coverage --export "$out/export.json" --output "$out/coverage.json"
}

# The whole-repository report, with the coverage if it was measured.
report() {
	local with_coverage=()
	if [ -f "$build/coverage/coverage.json" ]; then
		with_coverage=(--coverage "$build/coverage/coverage.json")
	fi
	"$build/venv/bin/python" "$here/quality.py" report --build "$build" --clang-tidy "$clang_tidy" --jobs "$jobs" \
		--output "$build/quality-report.json" "${with_coverage[@]+"${with_coverage[@]}"}"
}

command="${1:-}"
case "$command" in
gate | coverage | report | all | test) ;;
*) fail "usage: tools/quality/run.sh gate [BASE] | coverage | report | all | test" ;;
esac

mkdir -p "$build"
setup_python
if [ "$command" = test ]; then # the tools' own unit tests need neither LLVM nor a build
	cd "$here"
	exec "$build/venv/bin/python" -m unittest test_quality
fi

llvm=$(find_llvm)
[ -x "$llvm/clang++" ] || fail "no clang++ in '$llvm': set LLVM_BIN to the folder with clang++ and llvm-cov"
clang_tidy=$(find_clang_tidy)
configure

case "$command" in
gate) gate "${2:-}" ;;
coverage) coverage ;;
report) report ;;
all)
	status=0
	gate "${2:-}" || status=$?
	coverage
	report
	exit "$status"
	;;
esac
