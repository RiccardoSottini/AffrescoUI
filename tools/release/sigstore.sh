#!/usr/bin/env bash
# Sigstore signatures for a release's files: keyless, with cosign and the GitHub Actions run's OIDC identity, so no key
# is kept anywhere. Each file gets <file>.sigstore.json, a bundle with the signature, the short-lived certificate that
# names the workflow and the ref it ran for, and the entry in the public transparency log (Rekor).
#
#   tools/release/sigstore.sh sign <folder>               every file in the folder (needs id-token: write)
#   tools/release/sigstore.sh verify <folder> <identity>  every file against its bundle, signed by that identity
#
# The identity is the workflow's: https://github.com/<owner>/<repository>/.github/workflows/release.yml@refs/tags/v1.0.1
# (the release workflow passes https://github.com/$GITHUB_WORKFLOW_REF). SECURITY.md, "How releases are signed", says how
# anyone checks a downloaded file the same way.
set -euo pipefail

issuer="https://token.actions.githubusercontent.com"

files() {
	find "$1" -maxdepth 1 -type f ! -name '*.sigstore.json' | sort
}

sign() {
	local folder="$1" count=0
	while IFS= read -r file; do
		cosign sign-blob --yes --bundle "$file.sigstore.json" "$file" > /dev/null
		count=$((count + 1))
	done < <(files "$folder")
	[ "$count" -gt 0 ] || { echo "::error::sigstore.sh: nothing to sign in $folder" >&2; exit 1; }
	echo "sigstore.sh: signed $count files"
}

verify() {
	local folder="$1" identity="$2" count=0
	while IFS= read -r file; do
		cosign verify-blob --bundle "$file.sigstore.json" --certificate-identity "$identity" \
			--certificate-oidc-issuer "$issuer" "$file"
		count=$((count + 1))
	done < <(files "$folder")
	[ "$count" -gt 0 ] || { echo "::error::sigstore.sh: nothing to verify in $folder" >&2; exit 1; }
	echo "sigstore.sh: verified $count files, signed by $identity"
}

case "${1:-}" in
sign) sign "$2" ;;
verify) verify "$2" "$3" ;;
*) sed -n '2,11p' "$0" >&2; exit 2 ;;
esac
