#pragma once

/// @file
/// @brief Forge's source control: the git commands Forge runs, and nothing else. A call's arguments come from the
/// screen (so from a revision too); this is the host's rule for them, applied before git starts. Plain C++ with no
/// window, so the unit tests use it too.

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace forge {

/// @brief Whether a text is a branch name Forge creates or switches to: git's ref name rules, a little stricter
/// (letters, digits and `._/-` only), so it can never read as an option or a revision expression.
/// @param name The name.
/// @return Whether it is one.
[[nodiscard]] bool isBranchName(std::string_view name) noexcept;

/// @brief Whether a text is a path inside the open folder that git may be given: relative, no `..`, no control
/// characters or backslashes, at most 4 KiB.
/// @param path The path, with "/" separators.
/// @return Whether it is one.
[[nodiscard]] bool isFolderPath(std::string_view path) noexcept;

/// @brief Checks a git command line against the commands Forge runs:
///   - `status --porcelain=v2 -z --branch --untracked-files=all`;
///   - `add -- PATH...` and `restore --staged -- PATH...` (at most 1,000 paths);
///   - `commit -F -` (the message is the run's input, never an argument);
///   - `for-each-ref --format=%(HEAD)%00%(refname:short) refs/heads/`;
///   - `switch NAME` and `switch -c NAME` (isBranchName());
///   - `show --no-textconv HEAD:PATH`.
/// @param arguments The arguments after `git` and the options the host puts first.
/// @return Why they are refused, or std::nullopt if they are one of these.
[[nodiscard]] std::optional<std::string> checkGitArguments(const std::vector<std::string>& arguments);

} // namespace forge
