#!/usr/bin/env bash
# CMake's configure step, tried up to three times. Configuring downloads the dependencies (FetchContent), so one
# download that fails on the network fails the job: CMake retries a few network errors itself, but not an HTTP error,
# such as the 403 that GitHub's release downloads sometimes answer. The next try downloads only what is still missing.
# A configure that fails for any other reason fails three times the same way, so the step still fails, with the same
# error.
#
# Usage: .github/workflows/configure.sh <cmake's configure arguments>, e.g. -S . -B build -G Ninja
set -uo pipefail

attempts=3
for attempt in $(seq "$attempts"); do
	cmake "$@" && exit 0
	status=$?
	if [ "$attempt" -lt "$attempts" ]; then
		echo "::warning title=Configure::cmake failed (try $attempt of $attempts); trying again in $((attempt * 30)) s"
		sleep $((attempt * 30))
	fi
done
exit "$status"
