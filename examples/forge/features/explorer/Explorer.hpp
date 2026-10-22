#pragma once

/// @file
/// @brief Forge's explorer: the open folder's tree, kept current by the file watcher (a services::DirectoryModel),
/// and the file operations its side bar offers. The tree and the dialogs are in explorer.json.

#include "Forge.hpp"

#include <affresco/affresco.hpp>
#include <affresco/core/Executor.hpp>
#include <affresco/core/Json.hpp>

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace forge {

/// @brief One file or folder the explorer shows.
struct ExplorerEntry {
	std::string path; ///< Relative to the folder, with "/".
	bool folder;      ///< Whether it is a folder.
};

/// @brief The most rows the explorer's tree holds; past them it says `truncated`.
constexpr std::size_t mostExplorerRows = 50000;

/// @brief Turns entries in tree order (each folder before what it holds) into `tree` items: a folder's id ends with
/// "/" and it has `children`. An entry whose folder isn't listed before it is left out.
/// @param entries The entries.
/// @param most The most items it makes.
/// @return The items.
[[nodiscard]] affresco::json explorerItems(const std::vector<ExplorerEntry>& entries,
                                           std::size_t most = mostExplorerRows);

/// @brief The folders to open so that an item shows: each folder above it, as tree ids ("src/", "src/app/").
/// @param id The item's id.
/// @return The ids, outermost first.
[[nodiscard]] std::vector<std::string> explorerAncestors(const std::string& id);

/// @brief Whether a name may be given to a new or renamed entry: not empty, at most 255 bytes, not "." or "..", and
/// without "/", "\" or control characters.
/// @param name The name.
/// @return Whether it may.
[[nodiscard]] bool isPlainName(const std::string& name) noexcept;

/// @brief Registers the explorer's native functions, for `call` steps. Every path they take is an id under the open
/// folder (FileIndex.hpp's insideFolder() checks it, links included), and the entry itself is changed, never what a
/// link points to.
///   - `forge.explorer.tree` `{ "icons" }`: `{ "tree", "truncated", "live" }`, the tree dressed with the icon theme;
///     `live` says the watcher keeps it current (with an executor), else it is listed at each call;
///   - `forge.explorer.create` `{ "in", "name", "folder" }`: makes an empty file or a folder in the folder `in` (or
///     the folder of the file `in`); returns its id;
///   - `forge.explorer.rename` `{ "id", "name" }`: renames in place; returns the new id;
///   - `forge.explorer.delete` `{ "id" }`: deletes a file, or a folder and all it holds;
///   - `forge.explorer.drop` `{ "ids", "into", "place", "copy" }`: moves (or copies) entries next to the row `into`
///     (`place` as a tree's `onDrop` gives it: inside a folder, else in the row's folder); returns their ids;
///   - `forge.explorer.reveal` `{ "id" }`: the folders to open to show it (explorerAncestors()).
///
/// With an executor, each change the watcher reports runs the action `explorerRefresh` once (changes are debounced,
/// so at most ten times a second).
/// @param screen The screen.
/// @param workspace The open folder.
/// @param executor The main thread's executor (outlives the screen), or nullptr to list the folder at each call.
void registerExplorer(affresco::Screen& screen, std::shared_ptr<Workspace> workspace, affresco::Executor* executor);

} // namespace forge
