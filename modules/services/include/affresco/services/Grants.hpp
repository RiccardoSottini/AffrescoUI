#pragma once

/// @file
/// @brief What a host may grant from its command line (`--allow`): programs found on the PATH, the login shell, and
/// git under a policy that runs only the commands a source control view needs, on the declaration's own folder.

#include <affresco/services/Provider.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::services {

class ServiceRegistry;

/// @brief Finds a program on the PATH.
/// @param name Its name ("git"; ".exe" is added on Windows).
/// @return Its absolute path, or empty if no folder of the PATH has it.
[[nodiscard]] std::filesystem::path findProgram(std::string_view name);

/// @brief The person's login shell: $SHELL if it is an absolute path to a file, else /bin/sh (cmd.exe on Windows).
/// @return Its absolute path.
[[nodiscard]] std::filesystem::path loginShell();

/// @brief The options every granted git run starts with: no hooks, file system monitor, signing or textconv programs
/// a folder's own configuration could name, and paths taken literally.
/// @return The options.
[[nodiscard]] std::vector<std::string> gitOptions();

/// @brief Whether git arguments are a command the git grant runs: `init`; `status --porcelain=v2 -z --branch
/// --untracked-files=all`; `commit -F -`; `for-each-ref --format=%(HEAD)%00%(refname:short) refs/heads/`;
/// `log --format=%H%x00%an%x00%at%x00%s -n N [-- PATH]` (N up to 1000); `show --no-textconv HEAD:PATH`;
/// `add -- PATH...`; `restore [--staged] -- PATH...` (1 to 1000 paths); `switch [-c] BRANCH`.
/// @param arguments The arguments, after the grant's own options.
/// @return Why they are refused, or std::nullopt.
[[nodiscard]] std::optional<std::string> checkGitArguments(const std::vector<std::string>& arguments);

/// @brief Whether a path is one inside a folder: relative, no `..` part, no control character or backslash, at most
/// 4 KiB.
/// @param path The path.
/// @return Whether it is.
[[nodiscard]] bool isFolderPath(std::string_view path) noexcept;

/// @brief Whether a name is a branch name git accepts that the git grant switches to: up to 200 characters of
/// letters, digits and `._/-`, by git's rules on where `.`, `/` and `-` may be.
/// @param name The name.
/// @return Whether it is.
[[nodiscard]] bool isBranchName(std::string_view name) noexcept;

/// @brief The git grant's rule: checkGitArguments(), and the repository of the declaration's root only (never one
/// above it).
/// @return The shaper.
[[nodiscard]] ProcessShaper gitShaper();

/// @brief Grants a program from a command-line spec: `NAME=PROGRAM ARGUMENT...` (the program found on the PATH
/// unless absolute, the arguments split at spaces); or `NAME` alone: `shell` is the login shell (`-l`), `git` is git
/// under gitShaper(), any other the program of that name on the PATH.
/// @param services The registry.
/// @param spec The spec: at most 4 KiB and 32 arguments.
/// @return Why it can't, or std::nullopt.
[[nodiscard]] std::optional<std::string> grantProgram(ServiceRegistry& services, std::string_view spec);

} // namespace affresco::services
