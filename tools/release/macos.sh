#!/usr/bin/env bash
# Signs, notarizes, staples and checks the macOS downloads; the release workflow runs it, and it runs on a Mac by hand
# with an ad-hoc signature (no account needed) to try the bundle and the disk image.
#
#   tools/release/macos.sh import-certificate         MACOS_CERTIFICATE_P12 (base64), MACOS_CERTIFICATE_PASSWORD
#   tools/release/macos.sh delete-keychain
#   tools/release/macos.sh sign <file or .app>...     with SIGNING_IDENTITY ("-", the default, signs ad hoc)
#   tools/release/macos.sh notarize <.zip or .dmg>    APPLE_API_KEY_P8 (base64), APPLE_API_KEY_ID, APPLE_API_ISSUER_ID
#   tools/release/macos.sh staple <.app or .dmg>
#   tools/release/macos.sh zip <out.zip> <file or .app>...   what notarize takes: ditto keeps a bundle whole
#   tools/release/macos.sh dmg <AffrescoUI.app> <out.dmg> <volume name>
#   tools/release/macos.sh verify <file, .app or .dmg>... [--notarized]
#
# The certificate goes into a keychain of its own, in RUNNER_TEMP (or TMPDIR), with a random password: it is never
# added to the login keychain, and delete-keychain removes it. The API key is written to a file only for the time of
# one submission. Nothing secret is printed: GitHub masks the secrets, and the keychain's password is masked too.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
entitlements="$here/macos/AffrescoUI.entitlements"
temporary="${RUNNER_TEMP:-${TMPDIR:-/tmp}}"
keychain="$temporary/affrescoui-signing.keychain-db"
identity="${SIGNING_IDENTITY:--}"

secret_file="" # a file holding a secret, removed however the script ends
trap 'if [ -n "$secret_file" ]; then rm -f "$secret_file"; fi' EXIT

fail() { echo "::error::macos.sh: $*" >&2; exit 1; }
need() { for name in "$@"; do [ -n "${!name:-}" ] || fail "$name is not set"; done; }

# The keychain, with the Developer ID certificate and its private key, made the one codesign searches.
import_certificate() {
	need MACOS_CERTIFICATE_P12 MACOS_CERTIFICATE_PASSWORD
	local password
	password="$(openssl rand -base64 32)"
	if [ -n "${GITHUB_ACTIONS:-}" ]; then echo "::add-mask::$password"; fi
	secret_file="$(mktemp "$temporary/certificate.XXXXXX")"
	printf '%s' "$MACOS_CERTIFICATE_P12" | base64 --decode > "$secret_file"
	security create-keychain -p "$password" "$keychain"
	security set-keychain-settings -lut 21600 "$keychain" # locks itself after 6 hours, at the latest
	security unlock-keychain -p "$password" "$keychain"
	security import "$certificate" -k "$keychain" -f pkcs12 -P "$MACOS_CERTIFICATE_PASSWORD" -T /usr/bin/codesign
	rm -f "$secret_file"
	security set-key-partition-list -S apple-tool:,apple:,codesign: -s -k "$password" "$keychain" > /dev/null
	# Search it first, then the ones already there (the system's, with Apple's intermediate certificates)
	local existing=()
	while IFS= read -r line; do existing+=("$(echo "$line" | tr -d '" ')"); done < <(security list-keychains -d user)
	security list-keychains -d user -s "$keychain" "${existing[@]}"
	security find-identity -v -p codesigning "$keychain" | grep -q "Developer ID Application" \
		|| fail "the certificate is not a Developer ID Application identity"
}

delete_keychain() {
	if [ -f "$keychain" ]; then security delete-keychain "$keychain"; fi
}

# Signs with the hardened runtime and a secure timestamp (both required by notarization); ad hoc without them.
sign() {
	local options=(--force --options runtime --entitlements "$entitlements" --sign "$identity")
	if [ "$identity" != "-" ]; then
		options+=(--timestamp)
		[ -f "$keychain" ] && options+=(--keychain "$keychain")
	fi
	for file in "$@"; do
		codesign "${options[@]}" "$file"
	done
}

# Submits to Apple's notary service and waits; on anything but Accepted, prints Apple's log and fails.
notarize() {
	need APPLE_API_KEY_P8 APPLE_API_KEY_ID APPLE_API_ISSUER_ID
	local file="$1" result id status
	secret_file="$(mktemp "$temporary/api-key.XXXXXX")" # mktemp makes it readable by its owner only
	printf '%s' "$APPLE_API_KEY_P8" | base64 --decode > "$secret_file"
	local credentials=(--key "$secret_file" --key-id "$APPLE_API_KEY_ID" --issuer "$APPLE_API_ISSUER_ID")
	result="$(xcrun notarytool submit "$file" "${credentials[@]}" --wait --timeout 2h --output-format json)" || true
	id="$(printf '%s' "$result" | python3 -c 'import json, sys; print(json.load(sys.stdin).get("id", ""))' || true)"
	status="$(printf '%s' "$result" | python3 -c 'import json, sys; print(json.load(sys.stdin).get("status", ""))' || true)"
	echo "notarytool: $(basename "$file"): ${status:-no answer} (submission ${id:-none})"
	if [ "$status" != "Accepted" ]; then
		[ -n "$id" ] && xcrun notarytool log "$id" "${credentials[@]}" || true
		fail "Apple didn't notarize $(basename "$file"): ${status:-no answer}"
	fi
}

staple() {
	for file in "$@"; do
		xcrun stapler staple "$file"
		xcrun stapler validate "$file"
	done
}

make_zip() {
	local out="$1"
	shift
	rm -f "$out"
	local stage
	stage="$(mktemp -d "$temporary/notarize.XXXXXX")"
	for file in "$@"; do ditto "$file" "$stage/$(basename "$file")"; done
	ditto -c -k "$stage" "$out" # as Apple's notarization guide zips an app
	rm -rf "$stage"
}

# A read-only, compressed image (UDZO) with the app and a link to /Applications to drag it to.
make_dmg() {
	local app="$1" out="$2" volume="$3" stage
	stage="$(mktemp -d "$temporary/dmg.XXXXXX")"
	ditto "$app" "$stage/$(basename "$app")"
	ln -s /Applications "$stage/Applications"
	rm -f "$out"
	hdiutil create -volname "$volume" -srcfolder "$stage" -fs HFS+ -format UDZO -imagekey zlib-level=9 -ov "$out"
	rm -rf "$stage"
}

# Checks a signature; with --notarized, also that it is a Developer ID one Gatekeeper accepts, with its ticket stapled
# where one can be (an app, a disk image) and Apple's notarization found for a program on its own.
verify() {
	local notarized=false files=()
	for argument in "$@"; do
		if [ "$argument" = "--notarized" ]; then notarized=true; else files+=("$argument"); fi
	done
	for file in "${files[@]}"; do
		case "$file" in
		*.dmg)
			codesign --verify --strict --verbose=2 "$file"
			if $notarized; then
				spctl --assess --type open --context context:primary-signature --verbose=4 "$file"
				xcrun stapler validate "$file"
			fi ;;
		*.app)
			codesign --verify --deep --strict --verbose=2 "$file"
			if $notarized; then
				spctl --assess --type execute --verbose=4 "$file"
				xcrun stapler validate "$file"
			fi ;;
		*)
			codesign --verify --strict --verbose=2 "$file"
			if $notarized; then
				codesign --display --verbose=2 "$file" 2>&1 | grep -q "^Authority=Developer ID Application" \
					|| fail "$file isn't signed with a Developer ID"
				codesign --display --verbose=2 "$file" 2>&1 | grep -q "^Runtime Version=" \
					|| fail "$file isn't signed with the hardened runtime"
				# A program can't be stapled: Gatekeeper asks Apple, as this does
				codesign --verify --strict --check-notarization -R="notarized" --verbose=2 "$file"
			fi ;;
		esac
	done
}

command="${1:-}"
[ $# -gt 0 ] && shift
case "$command" in
import-certificate) import_certificate ;;
delete-keychain) delete_keychain ;;
sign) sign "$@" ;;
notarize) notarize "$1" ;;
staple) staple "$@" ;;
zip) make_zip "$@" ;;
dmg) make_dmg "$@" ;;
verify) verify "$@" ;;
*) sed -n '2,12p' "$0" >&2; exit 2 ;;
esac
