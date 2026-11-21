#pragma once

/// @file
/// @brief vcs::mergeDocuments: the three-way merge of JSON documents that rebases a proposal onto a newer main, for any
/// caller that holds two versions of a document and the one both came from (VERSION_CONTROL.md, "Rebasing").

#include "affresco/core/Json.hpp"

#include <functional>
#include <string>

namespace affresco::vcs {

/// @brief What a three-way merge gives.
struct MergeResult {
	json merged; ///< The merged document: the changes of both sides, and main's value where they conflict.
	/// `[{"path", "reason", "base", "main", "proposal"}]`; empty if everything merged. The live protocol and the
	/// changes window read these names, so they stay.
	json conflicts = json::array();

	/// @brief Tells whether nothing conflicted.
	/// @return true if so.
	[[nodiscard]] bool clean() const { return conflicts.empty(); }
};

/// @brief How a three-way merge settles its conflicts.
struct MergeOptions {
	/// Asked at each conflict, with its path: true takes the proposal's value there (its order, for a reorder) in
	/// place of main's. The conflict is listed all the same. Empty: main's value stays everywhere, as for a rebase.
	std::function<bool(const std::string& path)> takeProposal = {};
};

/// @brief Merges the changes two sides made to a common base, like `git merge-file` for JSON.
///
/// A value only one side changed takes that side's value. Objects merge key by key, in main's order of keys; a key
/// only the proposal added lands after the key it followed in the proposal, or at the start if it followed none the
/// merge keeps, so a definition added in the middle of a program file stays in the middle. A list whose items are all
/// objects with distinct string ids, on all three sides, merges by id (D15). Text is never merged inside: a string
/// both sides changed differently, such as a definition's source, is one conflict holding the three texts. Anything
/// else changed differently on both sides is a conflict, and there the merged document keeps main's value.
///
/// The names follow a proposal rebased onto main. A caller merging someone else's version into its own reads
/// "main" as mine and "proposal" as theirs, in the arguments and in each conflict. The same code rebases proposals
/// (Repository), so a caller and a rebase always agree.
/// @param base The common ancestor of both sides.
/// @param main One side, whose value a conflict keeps: main's document, or mine.
/// @param proposal The other side: the proposal's document, or theirs.
/// @param options How conflicts are settled: by default, main's value stays.
/// @return The merged document and the conflicts. Paths are JSON Pointers, from the nearest node with an id
/// ("#title/text") when the value is inside one, "/" for the whole document.
[[nodiscard]] MergeResult mergeDocuments(const json& base, const json& main, const json& proposal,
                                         const MergeOptions& options = {});

} // namespace affresco::vcs
