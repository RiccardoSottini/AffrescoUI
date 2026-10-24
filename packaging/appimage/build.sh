#!/bin/sh
# Makes AffrescoUI-<version>-x86_64.AppImage from the release's Linux archive, unpacked: its two programs, their
# assets and the notices, with this folder's AppRun, desktop file and icon. Run it on the filled copy that
# tools/release/packaging.py writes (packaging/README.md), on Linux x86_64:
#
#   sh build.sh <unpacked affrescoui-<version>-linux-x86_64> <appimagetool> <runtime-x86_64> [out folder]
#
# appimagetool and the runtime are the pinned downloads of cmake/dependencies.json; the runtime is passed in, so
# appimagetool fetches nothing.
set -eu
[ $# -ge 3 ] || { echo "usage: build.sh <unpacked linux archive> <appimagetool> <runtime> [out]" >&2; exit 2; }
archive=$1 tool=$2 runtime=$3 out=${4:-.}
here="$(cd "$(dirname "$0")" && pwd)"
version=$(sed -n 's/^X-AppImage-Version=//p' "$here/affrescoui.desktop")
appdir="$(mktemp -d)/AffrescoUI.AppDir"
mkdir -p "$appdir/usr/bin" "$appdir/usr/share/doc/affrescoui" "$appdir/usr/share/applications" \
	"$appdir/usr/share/icons/hicolor/scalable/apps"
for program in affrescoui affrescoui_launcher; do
	install -m 755 "$archive/$program" "$appdir/usr/bin/$program"
done
cp -R "$archive/assets" "$appdir/usr/bin/assets"
cp "$archive/LICENSE" "$archive/NOTICE" "$archive/THIRD_PARTY_NOTICES.md" "$appdir/usr/share/doc/affrescoui/"
install -m 755 "$here/AppRun" "$appdir/AppRun"
cp "$here/affrescoui.desktop" "$appdir/affrescoui.desktop"
cp "$here/affrescoui.desktop" "$appdir/usr/share/applications/affrescoui.desktop"
cp "$here/affrescoui.svg" "$appdir/affrescoui.svg"
cp "$here/affrescoui.svg" "$appdir/usr/share/icons/hicolor/scalable/apps/affrescoui.svg"
mkdir -p "$out"
ARCH=x86_64 APPIMAGE_EXTRACT_AND_RUN=1 "$tool" --runtime-file "$runtime" "$appdir" \
	"$out/AffrescoUI-$version-x86_64.AppImage"
echo "$out/AffrescoUI-$version-x86_64.AppImage"
