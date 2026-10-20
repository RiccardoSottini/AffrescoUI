#!/usr/bin/env bash
# Tells the CI jobs whether a change needs a build (ci.yml, job "Changes"): writes code=true or code=false to
# $GITHUB_OUTPUT. A change to prose alone (docs/, rfcs/, Markdown files no test or tool reads) builds nothing; the
# build jobs are then skipped, which branch protection counts as passing.
#
# Usage: .github/workflows/changes.sh BASE   (BASE: HEAD^1 for a pull request, the commit before a push)
set -euo pipefail

base="${1:-HEAD^1}"
output="${GITHUB_OUTPUT:-/dev/stdout}"

# A push that creates the branch, or a base not in the clone: build
if [ "$base" = 0000000000000000000000000000000000000000 ] || ! git rev-parse -q --verify "$base^{commit}" >/dev/null; then
	echo "code=true" >>"$output"
	exit 0
fi

# Markdown that tests and tools read (the format guide, the version control guide, the notices) counts as code
code=false
while IFS= read -r file; do
	case "$file" in
	docs/* | rfcs/*) ;;
	SCREEN_FORMAT.md | VERSION_CONTROL.md | THIRD_PARTY_NOTICES.md) code=true ;;
	*.md) ;;
	*) code=true ;;
	esac
done < <(git diff --name-only "$base" HEAD)
echo "code=$code" >>"$output"
echo "Changes since $(git rev-parse --short "$base"): code=$code"
