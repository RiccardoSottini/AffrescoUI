#!/usr/bin/env bash
# check.sh --- checks the clj-kondo configuration in this folder (README.md, "Checking the configuration").
#
# Lints the samples, which must give exactly the mistakes planted in them, and two example apps, which must give
# none of the findings the configuration is meant to silence: a host's word or an app's path left unresolved, a fixed
# alias's namespace unresolved, an ns's name or its required files taken for Clojure's, a ~ outside a backquote, a
# reader error, or a host word's arguments taken for Clojure's. Each sample app (notes.screen.lisp with tally.lisp,
# and board/) and each example app is linted in one run, and a call of a name another of its files defines is left
# out, as examples.sh does. The samples' locals read only through a path (column.done) must not be reported unused.
# Prints what it ran and exits with 1 on a difference, 2 when clj-kondo isn't installed.
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
tally.lisp:45: unresolved-symbol
columns.lisp:15: unused-binding
cards.lisp:14: invalid-arity"

# The linters whose findings the configuration silences in an app, so none may remain.
silenced='unresolved-namespace|namespace-name-mismatch|unused-namespace|unquote-not-syntax-quoted|syntax'

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

# Lists the names some files define, one per line, past any metadata (^:private), so that a call to one from another
# file isn't counted.
# @param ... the files
defined_names() {
	sed -nE 's/^\(def[a-z-]* (\^[^ ]+ )*([^ ()"^]+).*/\2/p' "$@" | sort -u
}

# Lints the files of one app in one run, and prints its findings but the calls of a name that one of its files, or
# one of the extra names given, defines (README.md, "What a linter can't know here").
# @param $1 extra names, one per line
# @param ... the app's files
own_findings() {
	local extra=$1
	shift
	local names
	names=$(printf '%s\n%s\n' "$(defined_names "$@")" "$extra" | sed '/^$/d' | sort -u)
	lint "$@" | while IFS= read -r line; do
		name=$(echo "$line" | sed -nE 's/^[^ ]+ unresolved-symbol Unresolved symbol: (.+)$/\1/p')
		if [ -z "$name" ] || ! echo "$names" | grep -qxF -- "$name"; then echo "$line"; fi
	done
}

# The shared files' code files, those that start with a comment or a form: their names are known in every app.
shared=()
while IFS= read -r file; do
	case "$(head -c 1 "$file")" in ";" | "(") shared+=("$file") ;; esac
done < <(find "$root/examples/shared" -name '*.lisp' -not -path '*/locales/*' | sort)
shared_names=$(defined_names "${shared[@]}")

echo "== samples"
sample_findings=$(
	own_findings "" "$here"/samples/*.lisp
	own_findings "" "$here"/samples/board/*.lisp
)
sample_findings=$(echo "$sample_findings" | sed -E 's/^([^ ]+ [a-z-]+).*/\1/' | sed '/^$/d' | sort)
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
	while IFS= read -r file; do files+=("$file"); done < <(find "$root/$app" -name '*.lisp' -not -path '*/locales/*' -not -path '*/seed/*' | sort)
	findings=$(own_findings "$shared_names" "${files[@]}")
	# An unresolved symbol left is a mistake of the configuration: what no file of the app defines is a host's word.
	unresolved=$(echo "$findings" | grep -E "^[^ ]+ unresolved-symbol " || true)
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
