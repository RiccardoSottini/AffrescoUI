#pragma once

/// @file
/// @brief The three-way merge of JSON documents that rebases a proposal onto a newer main: objects merge key by key,
/// and lists of nodes merge by their ids (D15), so two peers who change different nodes never conflict.

#include "affresco/core/Json.hpp"

namespace affresco::vcs::detail {

/// @brief What merge() gives.
struct MergeResult {
	json merged;                    ///< The merged document: main's changes and the proposal's.
	json conflicts = json::array(); ///< `[{"path", "reason", "base", "main", "proposal"}]`; empty if it merged.

	/// @brief Tells whether nothing conflicted.
	/// @return true if so.
	[[nodiscard]] bool clean() const { return conflicts.empty(); }
};

/// @brief Merges the changes two sides made to a common base, like `git merge-file` for JSON.
///
/// A value only one side changed takes that side's value. Objects merge key by key. A list whose items are all
/// objects with distinct string ids, on all three sides, merges by id: items changed on both sides merge in turn, an
/// item either side removed goes (unless the other side changed it), and an item either side added lands after the
/// item it followed on that side. Anything else changed differently on both sides is a conflict; there the merged
/// document keeps main's value.
/// @param base The common ancestor: the document the proposal was built on.
/// @param main main's document now.
/// @param proposal The proposal's document.
/// @return The merged document and the conflicts. Paths are JSON Pointers, from the nearest node with an id
/// ("#title/text") when the value is inside one.
[[nodiscard]] MergeResult merge(const json& base, const json& main, const json& proposal);

} // namespace affresco::vcs::detail
