#pragma once

/// @file
/// @brief Forge's search feature, native side: the open folder searched with the services' search engine
/// (affresco/services/Search.hpp) into the rows of features/search/search.json's results tree, and the matches of the
/// files found replaced on disk. Plain C++ but for registerSearch(), so the unit tests use it too.

#include "Forge.hpp"

#include <affresco/affresco.hpp>

#include <cstddef>
#include <filesystem>
#include <functional>
#include <memory>

namespace forge {

/// @brief How much a search shows and a replace changes.
struct SearchLimits {
	std::size_t results = 2000;      ///< Matches shown, at most.
	std::size_t files = 5000;        ///< Files a replace changes, at most.
	std::size_t fileBytes = 8 << 20; ///< The largest file a replace reads.
};

/// @brief Searches the open folder, but for the folders the file index skips (node_modules, build, ...).
/// @param root The open folder.
/// @param arguments `{ "pattern", "regex", "wholeWord", "caseSensitive", "include", "exclude" }` (untrusted).
/// @param cancelled Polled while it runs: true stops it.
/// @param onPartial Gets the results so far, now and then, on the calling thread.
/// @param limits Its caps.
/// @return `{ "tree": [items], "hits": [{ "file", "label", "line", "column" }], "files", "matches", "skipped",
///   "truncated", "done": true }`: a file's tree item has the id `"file:" + path` and its matches as children, each
///   with the id `"hit:" + its index in hits` and the label `line: text`.
/// @throws std::runtime_error With the reason the query is refused.
affresco::json searchFolder(const std::filesystem::path& root, const affresco::json& arguments,
                            const std::function<bool()>& cancelled,
                            const std::function<void(affresco::json)>& onPartial, const SearchLimits& limits = {});

/// @brief Replaces the matches of a pattern in some files of the open folder, writing each one atomically.
/// @param root The open folder.
/// @param arguments `{ "pattern", "regex", "wholeWord", "caseSensitive", "replacement", "files": [paths],
///   "skip": [paths] }` (untrusted): the files to change, but those in skip (the editor's unsaved ones).
/// @param limits Its caps.
/// @return `{ "replaced": count, "changed": [paths], "skipped": [paths], "failed": [{ "file", "error" }] }`.
/// @throws std::runtime_error If the pattern, the replacement or the list is refused.
affresco::json replaceInFolder(const std::filesystem::path& root, const affresco::json& arguments,
                               const SearchLimits& limits = {});

/// @brief Registers the search feature on a screen:
///   - `forge.searchScope`: takes the open folder for the next search, on the main thread (returns `{ "open" }`);
///   - `forge.search` (slow, cancellable with `{ "cancel": "forge.search" }`): searchFolder() in that folder, its
///     partial results streamed into the step's "into";
///   - `forge.replace`: replaceInFolder() in the open folder.
/// @param screen The screen, before it is built.
/// @param workspace The open folder.
void registerSearch(affresco::Screen& screen, std::shared_ptr<Workspace> workspace);

} // namespace forge
