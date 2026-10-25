#pragma once

/// @file
/// @brief git's own output read strictly, for the commands the git grant runs (Grants.hpp): `git status --porcelain=v2
/// -z --branch` and the branch list. Both are untrusted input (a file or branch name is anyone's text), so every field
/// is checked and every size bounded; anything else is refused.

#include "affresco/core/Json.hpp"

#include <cstddef>
#include <string_view>

namespace affresco::services {

/// @brief How much git output is read.
struct GitOutputLimits {
	std::size_t bytes = std::size_t{8} << 20; ///< The output, at most 8 MiB (the `process` provider's own bound).
	std::size_t entries = 10000;              ///< Files or branches, at most.
	std::size_t path = 4096;                  ///< A path, at most 4 KiB.
};

/// @brief Reads `git status --porcelain=v2 -z --branch --untracked-files=all`.
/// @param output git's output: headers and entries, each ended by a NUL (a rename's entry by two).
/// @param limits How much it reads.
/// @return `{ "branch": { "head", "oid", "upstream", "ahead", "behind" }, "files": [{ "path", "original", "index",
/// "worktree", "kind", "staged", "unstaged" }] }`: `index` and `worktree` are git's X and Y letters ("." for none),
/// `kind` one of modified, added, deleted, renamed, copied, typechange, unmerged, untracked or ignored; `original` is a
/// rename's or copy's old path, else "".
/// @throws std::runtime_error If the output is malformed or over a limit.
[[nodiscard]] json readGitStatus(std::string_view output, const GitOutputLimits& limits = {});

/// @brief Reads `git for-each-ref --format=%(HEAD)%00%(refname:short) refs/heads/`: one line per branch, `*` or a space,
/// a NUL, then its name.
/// @param output git's output.
/// @param limits How much it reads.
/// @return `{ "current": name or "", "branches": [{ "id", "label", "current" }] }`.
/// @throws std::runtime_error If the output is malformed or over a limit.
[[nodiscard]] json readGitBranches(std::string_view output, const GitOutputLimits& limits = {});

} // namespace affresco::services
