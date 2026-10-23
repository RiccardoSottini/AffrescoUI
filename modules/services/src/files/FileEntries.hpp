#pragma once

/// @file
/// @brief The `fs` provider's operations on entries (files, folders and links) rather than on text: `mkdir`, `rename`,
/// `delete`, `copy` and `stat`, each bounded.

#include "Providers.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <stop_token>
#include <string_view>

namespace affresco::services {

/// @brief The bounds of the entry operations.
struct EntryLimits {
	std::size_t entries = 10000;                      ///< The most entries a delete or a copy touches.
	std::uintmax_t bytes = std::uintmax_t{256} << 20; ///< The most bytes a copy writes.
	int names = 1000;                                 ///< The most names a copy with `free` tries.
};

/// @brief Resolves the path of an entry itself, not what it links to: its folder is resolved as underRoot() does
/// and its name appended, so a link is renamed, deleted or copied as a link.
/// @param root The root (canonical).
/// @param relative The path, relative to the root; never the root itself, never ending in "." or "..".
/// @return The entry's path, or std::nullopt if it is the root, absolute, or its folder is outside the root.
[[nodiscard]] std::optional<std::filesystem::path> entryUnderRoot(const std::filesystem::path& root,
                                                                  std::string_view relative);

/// @brief Makes a folder and the folders missing above it: {path}. Fails with "exists" if something is there.
/// @param root The root (canonical).
/// @param path The folder (under the root).
/// @param call Where the result goes.
void makeFolder(const std::filesystem::path& root, const std::filesystem::path& path, Call& call);

/// @brief Moves or renames an entry: {path}. Fails with "exists" if the new path is taken (a change of case only
/// is allowed), and refuses to move a folder into itself.
/// @param root The root (canonical).
/// @param from The entry (entryUnderRoot()).
/// @param to Its new path (entryUnderRoot()); its folder must exist.
/// @param call Where the result goes.
void moveEntry(const std::filesystem::path& root, const std::filesystem::path& from, const std::filesystem::path& to,
               Call& call);

/// @brief Deletes a file, a link or a folder with all it holds: {removed}. Fails with "tooLarge", touching nothing,
/// if it holds more than the limit's entries.
/// @param from The entry (entryUnderRoot()).
/// @param limits The bounds.
/// @param stop Says to give up.
/// @param call Where the result goes.
void removeEntry(const std::filesystem::path& from, const EntryLimits& limits, const std::stop_token& stop, Call& call);

/// @brief Copies a file, a link (as a link) or a folder with all it holds: {path}. With @p free, a path that is
/// taken becomes the first free "<name> copy<ext>", "<name> copy 2<ext>"... beside it; without, it fails with
/// "exists". Fails with "tooLarge", copying nothing, past the limits.
/// @param root The root (canonical).
/// @param from The entry (entryUnderRoot()).
/// @param to The copy's path (entryUnderRoot()); its folder must exist.
/// @param free Whether a taken path picks a free name.
/// @param limits The bounds.
/// @param stop Says to give up.
/// @param call Where the result goes.
void copyEntry(const std::filesystem::path& root, const std::filesystem::path& from, std::filesystem::path to,
               bool free, const EntryLimits& limits, const std::stop_token& stop, Call& call);

/// @brief Describes what is at a path: {exists, kind, size, modified}, kind "file", "folder" or "other", size for a
/// file, modified in milliseconds since 1970.
/// @param path The path (under the root, links followed as read() and list() follow them).
/// @param call Where the result goes.
void describeEntry(const std::filesystem::path& path, Call& call);

} // namespace affresco::services
