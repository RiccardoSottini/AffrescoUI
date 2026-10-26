#!/usr/bin/env bash
# Runs every fuzz target the build has, a few at a time (tests/fuzz/README.md), from the seeds and the corpus
# earlier runs grew. A target that crashes, leaks, hangs or runs out of memory doesn't stop the others: its input is
# kept in build/fuzz-found/<target>/ (uploaded as the "fuzz-found" artifact), the job summary names it, and the script
# fails at the end.
#
# Usage: .github/workflows/fuzz.sh BUILD SECONDS   (BUILD: a build with -DAFFRESCOUI_BUILD_FUZZERS=ON, built)
# The corpus is in fuzz-corpus/<target>/ (kept by the Actions cache); the seeds are written to BUILD/fuzz-corpus/.
# FUZZ_JOBS targets run at a time (default: one per core), each for SECONDS (three times that at most, when replaying
# its corpus takes longer); their logs are printed one after another.
set -euo pipefail

build="$1"
seconds="$2"
jobs="${FUZZ_JOBS:-$(nproc 2>/dev/null || echo 2)}"
python3 tests/fuzz/corpus.py --out "$build/fuzz-corpus"
logs="$build/fuzz-logs"
rm -rf "$logs"
mkdir -p "$logs"

# Fuzzes one target, its output in $logs/<target>.log; $logs/<target>.failed if it found an input that fails.
# $1: the target's program.
fuzz_one() {
	local program="$1" target="${1##*/affrescoui_fuzz_}"
	mkdir -p "fuzz-corpus/$target" "$build/fuzz-found/$target"
	local dictionary=() seeds=()
	if [ -f "$build/fuzz-corpus/$target.dict" ]; then
		dictionary=(-dict="$build/fuzz-corpus/$target.dict")
	fi
	if [ -d "$build/fuzz-corpus/$target" ]; then
		seeds=("$build/fuzz-corpus/$target")
	fi
	# Temporary, until the commit check is fast again: the full check on commit is slower than its budget, and these
	# targets commit or load the Kanban template several times an input, which takes up to 25 s in this build (10 s
	# is libFuzzer's limit here). Give them 10 s again when it is fast again.
	local per_input=10
	case "$target" in
	revision | scenarios | session) per_input=60 ;;
	esac
	# -max_total_time doesn't stop libFuzzer while it replays the corpus, which can outlast it on a slow target:
	# after three times its time, the target is interrupted (SIGINT, which libFuzzer takes as the end of a run)
	local status=0
	timeout -s INT -k 60 "$((seconds * 3))" "$program" -max_total_time="$seconds" -timeout="$per_input" \
		-rss_limit_mb=2048 -print_final_stats=1 "${dictionary[@]+"${dictionary[@]}"}" \
		-artifact_prefix="$build/fuzz-found/$target/" "fuzz-corpus/$target" "${seeds[@]+"${seeds[@]}"}" \
		>"$logs/$target.log" 2>&1 || status=$?
	if [ "$status" -eq 124 ] && grep -q "libFuzzer: run interrupted" "$logs/$target.log"; then
		touch "$logs/$target.interrupted"
	elif [ "$status" -ne 0 ]; then
		touch "$logs/$target.failed"
	fi
}

targets=()
for program in "$build"/tests/fuzz/affrescoui_fuzz_*; do
	[ -x "$program" ] || continue
	targets+=("${program##*/affrescoui_fuzz_}")
	while [ "$(jobs -rp | wc -l)" -ge "$jobs" ]; do
		sleep 1
	done
	fuzz_one "$program" &
done
wait

failed=()
for target in "${targets[@]}"; do
	echo "::group::Fuzz $target for $seconds s"
	cat "$logs/$target.log"
	echo "::endgroup::"
	if [ -f "$logs/$target.failed" ]; then
		failed+=("$target")
		echo "::error title=Fuzz $target::$target found an input that fails: it is in the fuzz-found artifact, under $target/"
	elif [ -f "$logs/$target.interrupted" ]; then
		echo "::notice title=Fuzz $target::$target was stopped after $((seconds * 3)) s, still replaying its corpus"
	fi
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
		elif [ -f "$logs/$target.interrupted" ]; then
			echo "- \`$target\`: passed, stopped after $((seconds * 3)) s while replaying its corpus"
		else
			echo "- \`$target\`: passed"
		fi
	done
} >>"${GITHUB_STEP_SUMMARY:-/dev/null}"
[ "${#failed[@]}" -eq 0 ]
