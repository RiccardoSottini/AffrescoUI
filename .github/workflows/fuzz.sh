#!/usr/bin/env bash
# Runs every fuzz target the build has, one after the other (tests/fuzz/README.md), from the seeds and the corpus
# earlier runs grew. A target that crashes, leaks, hangs or runs out of memory doesn't stop the others: its input is
# kept in build/fuzz-found/<target>/ (uploaded as the "fuzz-found" artifact), the job summary names it, and the script
# fails at the end.
#
# Usage: .github/workflows/fuzz.sh BUILD SECONDS   (BUILD: a build with -DAFFRESCOUI_BUILD_FUZZERS=ON, built)
# The corpus is in fuzz-corpus/<target>/ (kept by the Actions cache); the seeds are written to BUILD/fuzz-corpus/.
set -euo pipefail

build="$1"
seconds="$2"
python3 tests/fuzz/corpus.py --out "$build/fuzz-corpus"

failed=()
targets=()
for program in "$build"/tests/fuzz/affrescoui_fuzz_*; do
	[ -x "$program" ] || continue
	target="${program##*/affrescoui_fuzz_}"
	targets+=("$target")
	mkdir -p "fuzz-corpus/$target" "$build/fuzz-found/$target"
	echo "::group::Fuzz $target for $seconds s"
	dictionary=()
	if [ -f "$build/fuzz-corpus/$target.dict" ]; then
		dictionary=(-dict="$build/fuzz-corpus/$target.dict")
	fi
	seeds=()
	if [ -d "$build/fuzz-corpus/$target" ]; then
		seeds=("$build/fuzz-corpus/$target")
	fi
	if ! "$program" -max_total_time="$seconds" -timeout=10 -rss_limit_mb=2048 -print_final_stats=1 \
		"${dictionary[@]+"${dictionary[@]}"}" -artifact_prefix="$build/fuzz-found/$target/" \
		"fuzz-corpus/$target" "${seeds[@]+"${seeds[@]}"}"; then
		failed+=("$target")
		echo "::error title=Fuzz $target::$target found an input that fails: it is in the fuzz-found artifact, under $target/"
	fi
	echo "::endgroup::"
done

if [ "${#targets[@]}" -eq 0 ]; then
	echo "::error title=Fuzz::no fuzz targets in $build/tests/fuzz"
	exit 1
fi
{
	echo "### Fuzzing: ${#targets[@]} targets, $seconds s each"
	echo
	for target in "${targets[@]}"; do
		if [[ " ${failed[*]-} " == *" $target "* ]]; then
			echo "- \`$target\`: **failed** (input in the \`fuzz-found\` artifact, \`$target/\`)"
		else
			echo "- \`$target\`: passed"
		fi
	done
} >>"${GITHUB_STEP_SUMMARY:-/dev/null}"
[ "${#failed[@]}" -eq 0 ]
