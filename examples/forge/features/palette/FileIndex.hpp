#pragma once

/// @file
/// @brief Forge's file index: the files of the open folder, for quick open and the explorer, found by one walk of the
/// folder. Plain C++ with no window, so the unit tests use it too.

#include <affresco/core/Json.hpp>

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>

namespace forge {

/// @brief How far a walk goes.
struct IndexLimits {
	std::size_t files = 50000; ///< Files listed at most; the rest are left out and `truncated` says so.
	int depth = 24;            ///< Folders nested at most below the root.
};

/// @brief Lists the files under a folder, skipping hidden entries, build output and dependencies, and not following
/// symbolic links (so a link can't lead the walk out of the folder or round in a circle).
/// @param root The folder.
/// @param limits How far to go.
/// @return `{ "root": name, "files": [{ "id", "title", "detail" }], "tree": [items], "truncated": bool }`: each file's
/// id is its path relative to the root with "/" separators, its title the file name and its detail the folder; the
/// tree is the same files as `tree` items (folders first, then files, each by name).
/// @throws std::runtime_error If the root isn't a folder that can be read.
affresco::json indexFolder(const std::filesystem::path& root, const IndexLimits& limits = {});

/// @brief The path of a file inside a folder, if it is one: a relative path with no "..", leading "/" or drive.
/// @param root The folder.
/// @param relative The file, relative to it with "/" separators (an id from indexFolder()).
/// @return The file's path, or an empty path if `relative` would leave the folder.
std::filesystem::path insideFolder(const std::filesystem::path& root, const std::string& relative);

/// @brief Reads a text file of the open folder for the editor.
/// @param root The folder.
/// @param relative The file, relative to it (an id from indexFolder()).
/// @param maxBytes The largest file it reads.
/// @return Its text.
/// @throws std::runtime_error If the path leaves the folder, or the file is missing, too large or can't be read.
std::string readInFolder(const std::filesystem::path& root, const std::string& relative,
                         std::size_t maxBytes = std::size_t{2} * 1024 * 1024);

/// @brief Replaces a file of the open folder atomically: writes a new file beside it, then renames it over it. The
/// new file has a name no one chose and is created, never opened: a link planted beside the file (the folder may be
/// someone else's) can't send the bytes elsewhere. It keeps the file's permissions.
/// @param path The file (from insideFolder()).
/// @param text Its new bytes.
/// @return An error, or std::nullopt.
std::optional<std::string> writeInFolder(const std::filesystem::path& path, const std::string& text);

} // namespace forge
