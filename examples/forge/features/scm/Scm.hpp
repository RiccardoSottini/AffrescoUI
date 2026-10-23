#pragma once

/// @file
/// @brief Forge's source control (features/scm/scm.json): the git grant and the natives that read git's output. git
/// runs through the `process` provider, never a shell: the host grants the one program, and checkGitArguments() is its
/// rule for each run's arguments, which runs in the open folder's repository only.

#include "Forge.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace forge {

/// @brief Finds git: the first `git` program in one of PATH's absolute folders.
/// @return Its absolute path, or an empty path if there is none.
[[nodiscard]] std::filesystem::path findGit();

/// @brief The options every run of git starts with: no file system monitor and no hooks (programs the repository
/// could name), no signing program, paths as they are, no locks taken just to read, and paths never read as pathspec
/// magic.
/// @return The arguments.
[[nodiscard]] std::vector<std::string> gitOptions();

/// @brief Grants git as the `git` program (if findGit() finds it) and registers the source control natives:
///   - `forge.scm.parseStatus` `{ "output" }`: parseStatus() (GitStatus.hpp) with the files as list items too,
///     `staged` and `changes`;
///   - `forge.scm.parseBranches` `{ "output" }`: parseBranches();
///   - `forge.scm.diff` `{ "original", "modified" }`: diffLines() (LineDiff.hpp) and its `gutter` marks.
/// Every run of git starts in the open folder with `--git-dir` and `--work-tree` naming it, so it never reaches a
/// repository above it; with no folder open a run is refused.
/// @param screen The screen.
/// @param workspace The open folder.
void registerScm(affresco::Screen& screen, const std::shared_ptr<Workspace>& workspace);

} // namespace forge
