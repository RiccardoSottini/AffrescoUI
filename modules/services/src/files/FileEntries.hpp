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
#include <string>
#include <string_view>
#include <vector>

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

/// @brief How a copy goes, beyond its paths.
struct CopyOptions {
	bool free = false;                ///< A taken path picks a free name ("<name> copy<ext>", ...).
	bool merge = false;               ///< A folder copied over a folder adds what is missing there and replaces the
	                                  ///< files that are older there; nothing else there changes.
	std::vector<std::string> exclude; ///< Name globs (`*`, `?`) of the files and folders inside left out.
	bool plain = false; ///< From another root: links are left out, and each file copied is given only read and write
	                    ///< for its owner and read for the others, never an execute bit.
};

/// @brief Whether a name matches a glob of `*` (any run of characters) and `?` (one character).
/// @param glob The glob.
/// @param name The name.
/// @return Whether it matches.
[[nodiscard]] bool matchesName(std::string_view glob, std::string_view name) noexcept;

/// @brief Copies a file, a link (as a link) or a folder with all it holds: {path}. With `free`, a path that is
/// taken becomes the first free "<name> copy<ext>", "<name> copy 2<ext>"... beside it; with `merge`, a folder is
/// merged into the folder there; else it fails with "exists". Fails with "tooLarge", copying nothing, past the limits.
/// A link already at a path the copy writes is replaced, never written through.
/// @param root The root the copy is in (canonical).
/// @param from The entry (entryUnderRoot(), of this root or of another the service copies from).
/// @param to The copy's path (entryUnderRoot()); its folder must exist.
/// @param options See CopyOptions.
/// @param limits The bounds.
/// @param stop Says to give up.
/// @param call Where the result goes.
void copyEntry(const std::filesystem::path& root, const std::filesystem::path& from, std::filesystem::path to,
               const CopyOptions& options, const EntryLimits& limits, const std::stop_token& stop, Call& call);

/// @brief Describes what is at a path: {exists, kind, size, modified}, kind "file", "folder" or "other", size for a
/// file, modified in milliseconds since 1970.
/// @param path The path (under the root, links followed as read() and list() follow them).
/// @param call Where the result goes.
void describeEntry(const std::filesystem::path& path, Call& call);

} // namespace affresco::services
