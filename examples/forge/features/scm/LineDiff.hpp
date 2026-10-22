#pragma once

/// @file
/// @brief Forge's source control: a line diff of two texts, for the diff view and the editor's gutter marks. Bounded:
/// past a size it marks the changed middle as replaced instead of matching its lines. Plain C++ with no window, so the
/// unit tests use it too.

#include <affresco/core/Json.hpp>

#include <cstddef>
#include <string_view>

namespace forge {

/// @brief How much a diff compares.
struct DiffLimits {
	std::size_t matched = 2000; ///< Lines on each side of the changed middle that are matched one by one, at most.
	std::size_t rows = 20000;   ///< Rows returned, at most; the rest are left out and `truncated` says so.
};

/// @brief Compares two texts line by line: the lines both start and end with are kept, and the middle is matched by a
/// longest common subsequence (or, past DiffLimits::matched lines, all removed and then all added).
/// @param original The text before.
/// @param modified The text after.
/// @param limits How much it compares.
/// @return `{ "rows": [{ "kind": "same"|"removed"|"added", "original", "modified", "text" }], "added", "removed",
/// "truncated" }`: `original` and `modified` are 1-based line numbers, 0 where a row has none.
[[nodiscard]] affresco::json diffLines(std::string_view original, std::string_view modified,
                                       const DiffLimits& limits = {});

/// @brief The editor's gutter marks for a file against its committed text, in the editor's `decorations` shape:
/// `[{ "line", "endLine", "kind": "gutter", "color", "text" }]`, one per changed block of the modified text, its
/// `text` "added", "modified" or "deleted" (a deletion marks the line after it).
/// @param diff A diffLines() result.
/// @return The marks.
[[nodiscard]] affresco::json gutterMarks(const affresco::json& diff);

} // namespace forge
