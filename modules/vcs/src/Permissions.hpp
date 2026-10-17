#pragma once

/// @file
/// @brief How permission rules see a change: by its effect on the document, not by the paths in its patch.
///
/// A JSON Patch can reach the same result many ways (array positions shift, a parent can be replaced whole), so
/// rules compare the document before and after instead. A protected node that ends up identical wasn't changed,
/// however the patch was written; a change outside an allowed area is caught even if the patch never names it.

#include "affresco/core/DocumentRules.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace affresco::vcs::detail {

/// @brief The document as revisions address it: "/state" is the live state.
/// @param document The document.
/// @param state The live state.
/// @return A copy of the document with the live state in place of its "state" section.
[[nodiscard]] json view(const json& document, const json& state);

/// @brief Finds the value at a JSON Pointer.
/// @param value The JSON.
/// @param pointer The pointer.
/// @return The value, or nullptr if absent (or the pointer is malformed).
[[nodiscard]] const json* locate(const json& value, const std::string& pointer);

/// @brief The channel part of an author's name: the trusted part, given by the host.
/// @param author e.g. "agent/claude-code".
/// @return e.g. "agent".
[[nodiscard]] std::string channelOf(std::string_view author);

/// @brief Tells whether a rule's author covers an author.
/// @param ruleAuthor "*", a channel name or a full name.
/// @param author The author's full name.
/// @return true for "*", the same name, or the author's channel.
[[nodiscard]] bool authorMatches(std::string_view ruleAuthor, std::string_view author);

/// @brief Tells whether a change alters anything a pattern covers.
/// @param rules Finds "#id" nodes.
/// @param before The view before.
/// @param after The view after.
/// @param pattern A rule's pattern (empty: everything).
/// @return true if something it covers is different, added or gone.
[[nodiscard]] bool changes(const DocumentRules& rules, const json& before, const json& after,
                           const std::string& pattern);

/// @brief Tells whether everything a change alters is inside the patterns.
/// @param rules Finds "#id" nodes.
/// @param before The view before.
/// @param after The view after.
/// @param patterns Rule patterns (an empty one covers everything).
/// @return true if the views are equal once what the patterns cover is masked out of both.
[[nodiscard]] bool onlyInside(const DocumentRules& rules, const json& before, const json& after,
                              const std::vector<std::string>& patterns);

/// @brief Names the part of a document a JSON Patch operation at a path touches, for reviewers and for
/// "don't ask again". Adding, removing or replacing a list element touches whatever holds the list.
/// @param rules Finds "#id" nodes.
/// @param view The view as it is when the operation runs.
/// @param pointer The operation's (resolved) path.
/// @return The nearest node with an id ("#board"), else the first two segments ("/state/cards", "/colors/accent"),
/// or "/root" for layout no id names.
[[nodiscard]] std::string area(const DocumentRules& rules, const json& view, const std::string& pointer);

/// @brief Tells whether an area from area() is narrow enough to allow without asking.
/// @param area The area.
/// @return false for the whole layout or a whole section ("/root", "/actions").
[[nodiscard]] bool narrow(std::string_view area);

} // namespace affresco::vcs::detail
