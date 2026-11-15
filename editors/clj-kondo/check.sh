#!/usr/bin/env bash
# check.sh --- checks the clj-kondo configuration in this folder (README.md, "Checking the configuration").
#
# Lints the samples, which must give exactly the mistakes planted in them, and two example apps, which must give
# none of the findings the configuration is meant to silence: a host's word or an app's path left unresolved, a fixed
# alias's namespace unresolved, a ~ outside a backquote, a reader error, or a host word's arguments taken for
# Clojure's. Prints what it ran and exits with 1 on a difference, 2 when clj-kondo isn't installed.
set -u

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)

if ! command -v clj-kondo >/dev/null 2>&1; then
	echo "check.sh: clj-kondo isn't on PATH: install it to run this check" >&2
	exit 2
fi

# The findings the samples must give, exactly: "file:line: linter".
expected_samples="notes.screen.lisp:31: invalid-arity
notes.screen.lisp:57: unused-binding
tally.lisp:38: misplaced-docstring
tally.lisp:45: unresolved-symbol"

# The linters whose findings the configuration silences in an app, so none may remain.
silenced='unresolved-namespace|unquote-not-syntax-quoted|syntax'

# The host's words that Clojure has too (config.edn, :config-in-call), whose arguments aren't Clojure's.
host_words='clojure\.core/(set|find|empty|time|type|repeat) '

status=0

# Lints files and prints each finding as "file:line: linter message".
# @param ... the files
lint() {
	echo "+ clj-kondo --lang clj --config-dir ${here#"$root"/} --lint ($# files)" >&2
	clj-kondo --lang clj --cache false --config-dir "$here" --config '{:output {:linter-name true}}' --lint "$@" |
		sed -nE 's|^.*/([^/:]+):([0-9]+):[0-9]+: [a-z]+: (.*) \[:([a-z-]+)\]$|\1:\2: \4 \3|p'
}

# Lists the names an app defines, one per line, so that a call to one from another file isn't counted.
# @param ... the app's files
defined_names() {
	sed -nE 's/^\(def[a-z-]* ([^ ()"]+).*/\1/p' "$@" | sort -u
}

echo "== samples"
sample_findings=$(lint "$here"/samples/*.lisp | sed -E 's/^([^ ]+ [a-z-]+).*/\1/' | sort)
if [ "$sample_findings" != "$(echo "$expected_samples" | sort)" ]; then
	echo "The samples' findings differ from the planted mistakes."
	echo "Expected:"
	echo "$expected_samples" | sort | sed 's/^/  /'
	echo "Found:"
	echo "$sample_findings" | sed 's/^/  /'
	status=1
else
	echo "ok: the $(echo "$expected_samples" | wc -l | tr -d ' ') planted mistakes, and nothing else"
fi

for app in examples/streak/lisp examples/flow/lisp; do
	echo "== $app"
	files=()
	while IFS= read -r file; do files+=("$file"); done < <(find "$root/$app" -name '*.lisp' | sort)
	names=$(defined_names "${files[@]}")
	findings=$(lint "${files[@]}")
	# An unresolved symbol is a mistake of the configuration unless the app defines it in another of its files: a
	# linter reads one file at a time (README.md, "What a linter can't know here").
	unresolved=$(echo "$findings" | sed -nE 's/^([^ ]+) unresolved-symbol Unresolved symbol: (.+)$/\1 \2/p' |
		while read -r place name; do
			echo "$names" | grep -qxF -- "$name" || echo "$place unresolved-symbol $name"
		done)
	wrong=$(
		echo "$findings" | grep -E "^[^ ]+ ($silenced) " || true
		echo "$findings" | grep -E "^[^ ]+ (invalid-arity|type-mismatch) .*$host_words" || true
		echo "$unresolved"
	)
	wrong=$(echo "$wrong" | sed '/^$/d')
	if [ -n "$wrong" ]; then
		echo "Findings the configuration should have silenced:"
		echo "$wrong" | sed 's/^/  /'
		status=1
	else
		echo "ok: ${#files[@]} files, $(echo "$findings" | sed '/^$/d' | wc -l | tr -d ' ') other findings (the app's own)"
	fi
done

exit $status
