#!/bin/sh
# Builds the documentation site into website/build/site, strictly: a failing snippet, a broken link or a missing page
# fails the build. Needs Python 3.10 or later with venv ($PYTHON, else python3), doxygen, and the affrescoui
# tool: $AFFRESCOUI_CLI, else build/affrescoui, else affrescoui on PATH. See website/README.md.
set -eu

website=$(cd "$(dirname "$0")" && pwd)
repo=$(dirname "$website")
build="$website/build"

cli=${AFFRESCOUI_CLI:-}
if [ -z "$cli" ] && [ -x "$repo/build/affrescoui" ]; then
	cli="$repo/build/affrescoui"
fi
if [ -z "$cli" ]; then
	cli=$(command -v affrescoui || true)
fi
if [ -z "$cli" ]; then
	echo "build.sh: no affrescoui tool: build it (cmake --build build --target affrescoui) or set AFFRESCOUI_CLI" >&2
	exit 1
fi

mkdir -p "$build"
if [ ! -x "$build/venv/bin/mkdocs" ] || ! cmp -s "$website/requirements.txt" "$build/venv/requirements.txt"; then
	rm -rf "$build/venv"
	"${PYTHON:-python3}" -m venv "$build/venv"
	"$build/venv/bin/python" -m pip install --quiet --disable-pip-version-check --require-hashes --no-deps \
		-r "$website/requirements.txt"
	cp "$website/requirements.txt" "$build/venv/requirements.txt"
fi

"$build/venv/bin/python" "$website/tools/check_snippets.py" --cli "$cli"

# The C++ API reference: the repository's Doxyfile with its output folder changed, and its warnings shown but not
# fatal: CI's Linux job holds the headers to the Doxyfile's rules, and other Doxygen versions warn differently.
rm -rf "$build/api-reference"
(cd "$repo" && { cat Doxyfile; echo "OUTPUT_DIRECTORY = $build/api-reference"; echo "WARN_AS_ERROR = NO"; } | doxygen -)

"$build/venv/bin/python" "$website/tools/generate.py" --cli "$cli" --api "$build/api-reference/html"
(cd "$website" && "$build/venv/bin/mkdocs" build --strict --quiet)
echo "build.sh: the site is in $build/site"
