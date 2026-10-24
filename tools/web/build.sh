#!/bin/sh
# Builds the browser site (web/README.md) into build-web/site/: the host with every template, and the pages that
# start them. Run it with emsdk active (source emsdk_env.sh); it prints the program's size, gzipped.
#
#   tools/web/build.sh
#
# The site is static: serve build-web/site/ from any web server (web/README.md says how to publish it).
set -eu

root=$(cd "$(dirname "$0")/../.." && pwd)
build="$root/build-web"
site="$build/site"
version=$(sed -n 's/^set(AFFRESCOUI_EMSDK_VERSION \(.*\))$/\1/p' "$root/cmake/AffrescoUIWeb.cmake")

if ! command -v emcmake >/dev/null 2>&1; then
	echo "tools/web/build.sh: emsdk isn't active: install emsdk $version and source its emsdk_env.sh (web/README.md)" >&2
	exit 1
fi
if ! emcc --version | head -n 1 | grep -q " $version "; then
	echo "tools/web/build.sh: warning: the site is checked with emsdk $version, and this is: $(emcc --version | head -n 1)" >&2
fi

# Without warnings as errors: the desktop builds hold the library to them, and a few of its files warn on wasm32 (a
# 32-bit long) or with emsdk's newer clang, which is theirs to fix (web/README.md)
emcmake cmake -S "$root" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_COMPILE_WARNING_AS_ERROR=OFF
cmake --build "$build" --target affrescoui_host affrescoui_cli

rm -rf "$site"
mkdir -p "$site"
cp "$root/web/index.html" "$root/web/site.css" "$root/web/site.js" "$site/"
cp "$build/affrescoui_host.js" "$build/affrescoui_host.wasm" "$build/affrescoui_host.data" "$site/"

# A page per template, and its link in the picker
links="$build/site-links.html"
: >"$links"
python3 -I -c 'import json, sys; [print(name, folder) for name, folder in json.load(open(sys.argv[1]))["templates"].items()]' \
	"$root/examples/templates.json" | while read -r name folder; do
	file="$root/examples/$folder/index.screen.json"
	title=$(python3 -I -c 'import html, json, sys; print(html.escape(json.load(open(sys.argv[1])).get("window", {}).get("title", sys.argv[2]).split(":")[0]))' "$file" "$name")
	replacement=$(printf '%s' "$title" | sed 's/[&|\\]/\\&/g') # sed's own characters, escaped
	sed -e "s|@FOLDER@|$folder|g" -e "s|@SCREEN@|$name|g" -e "s|@TITLE@|$replacement|g" "$root/web/template.html" >"$site/$name.html"
	echo "<li><a href=\"$name.html\"><strong>$title</strong><br>$name</a></li>" >>"$links"
done
awk -v links="$links" '/<!-- templates -->/ { while ((getline line < links) > 0) print line; next } { print }' \
	"$root/web/index.html" >"$site/index.html"
rm "$links"

echo "Site: $site"
echo "affrescoui_host.wasm: $(wc -c <"$site/affrescoui_host.wasm" | tr -d ' ') bytes, $(gzip -9 -c "$site/affrescoui_host.wasm" | wc -c | tr -d ' ') gzipped"
echo "assets (affrescoui_host.data): $(wc -c <"$site/affrescoui_host.data" | tr -d ' ') bytes, $(gzip -9 -c "$site/affrescoui_host.data" | wc -c | tr -d ' ') gzipped"
