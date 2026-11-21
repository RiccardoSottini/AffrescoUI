#!/usr/bin/env bash
# examples.sh --- lints the example apps' Lisp programs with the configuration in this folder (README.md, "Linting an
# example app").
#
#     editors/clj-kondo/examples.sh [--summary] [app...]
#
# An app is a folder of examples/ (flow, forge...), or shared for examples/shared/; with none, every one. Its code files
# are its .lisp files that start with a comment or a form, outside locales/ and seed/ (those are data). All the files
# of an app are linted in one run, and each finding printed as "file:line: linter message". A call of a name that
# another file of the app, or a file of examples/shared/, defines is left out and counted apart: the linter reads each
# file's names on their own, where the language has one flat space of names. --summary prints one line per app
# instead. Exits with 0 with no finding, 1 with findings, 2 when clj-kondo isn't installed or an app doesn't exist.
set -u

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)

if ! command -v clj-kondo >/dev/null 2>&1; then
	echo "examples.sh: clj-kondo isn't on PATH: install it to lint the examples" >&2
	exit 2
fi

summary=false
apps=()
for argument in "$@"; do
	case "$argument" in
	--summary) summary=true ;;
	-*)
		echo "examples.sh: unknown option $argument (examples.sh [--summary] [app...])" >&2
		exit 2
		;;
	*) apps+=("$argument") ;;
	esac
done
if [ ${#apps[@]} -eq 0 ]; then
	while IFS= read -r folder; do apps+=("$(basename "$folder")"); done < <(find "$root/examples" -mindepth 1 -maxdepth 1 -type d | sort)
fi

# Lists an app's code files, one per line: .lisp files whose first character that isn't a space starts a comment or a
# form, outside locales/ and seed/.
# @param $1 the app's folder
code_files() {
	find "$1" -name '*.lisp' -not -path '*/locales/*' -not -path '*/seed/*' | sort | while IFS= read -r file; do
		first=$(tr -d ' \t\r\n' <"$file" | head -c 1)
		if [ "$first" = ";" ] || [ "$first" = "(" ]; then echo "$file"; fi
	done
}

# Lists the names some files define, one per line: the word after a definition form that starts a line, past any
# metadata (^:private).
# @param ... the files
defined_names() {
	[ $# -eq 0 ] && return
	sed -nE 's/^\(def[a-z-]* (\^[^ ]+ )*([^ ()"^]+).*/\2/p' "$@" | sort -u
}

# Lints files in one run and prints each finding as "file:line: linter message", the file from the top of the
# repository.
# @param ... the files
lint() {
	clj-kondo --lang clj --cache false --config-dir "$here" --config '{:output {:linter-name true}}' --lint "$@" |
		sed -nE 's|^(.*):([0-9]+):[0-9]+: [a-z]+: (.*) \[:([a-z-]+)\]$|\1:\2: \4 \3|p' | sed "s|^$root/||"
}

shared_files=()
while IFS= read -r file; do shared_files+=("$file"); done < <(code_files "$root/examples/shared")
shared_names=$(defined_names "${shared_files[@]+"${shared_files[@]}"}")

status=0
for app in "${apps[@]}"; do
	folder="$root/examples/$app"
	if [ ! -d "$folder" ]; then
		echo "examples.sh: no app examples/$app" >&2
		exit 2
	fi
	files=()
	while IFS= read -r file; do files+=("$file"); done < <(code_files "$folder")
	if [ ${#files[@]} -eq 0 ]; then
		$summary && echo "$app: no code files"
		continue
	fi
	names=$(printf '%s\n%s\n' "$(defined_names "${files[@]}")" "$shared_names" | sed '/^$/d' | sort -u)
	found=$(lint "${files[@]}")
	# A name the app or the shared files define is known to the language wherever it is called.
	resolved=$(echo "$found" | sed -nE 's/^[^ ]+ unresolved-symbol Unresolved symbol: (.+)$/\1/p' |
		grep -xF -f <(echo "$names" | sed '/^$/d') || true)
	findings=$(echo "$found" | while IFS= read -r line; do
		name=$(echo "$line" | sed -nE 's/^[^ ]+ unresolved-symbol Unresolved symbol: (.+)$/\1/p')
		if [ -z "$name" ] || ! echo "$names" | grep -qxF -- "$name"; then echo "$line"; fi
	done | sed '/^$/d')
	count=$(echo "$findings" | sed '/^$/d' | wc -l | tr -d ' ')
	calls=$(echo "$resolved" | sed '/^$/d' | wc -l | tr -d ' ')
	distinct=$(echo "$resolved" | sed '/^$/d' | sort -u | wc -l | tr -d ' ')
	if $summary; then
		echo "$app: ${#files[@]} files, $count findings, $calls calls of $distinct names defined in other files"
	elif [ -n "$findings" ]; then
		echo "$findings"
	fi
	[ "$count" -gt 0 ] && status=1
done

exit $status
