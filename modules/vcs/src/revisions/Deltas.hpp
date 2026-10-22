#pragma once

/// @file
/// @brief Deltas: how one document differs from another, as a plain JSON Patch that rebuilds it exactly, and the
/// safe way to apply one. Commits keep the delta from their first parent instead of a whole document.

#include "affresco/core/Json.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace affresco::vcs::detail {

/// @brief The JSON Patch that turns one document into another, exactly: applied to @p from it gives a value equal to
/// @p to, keys in the same order (nlohmann's json::diff loses the order of an object's keys, which a commit's id
/// covers). Lists keep their common start and end, so adding or removing one item is one operation.
/// @param from The document before.
/// @param to The document after.
/// @return Operations "add", "remove" and "replace", with numeric list positions (never "-").
[[nodiscard]] json delta(const json& from, const json& to);

/// @brief Applies one JSON Patch operation, its paths resolved, refusing one that adds a value inside a number, a
/// string or a boolean.
///
/// RFC 6902 makes that an error, but nlohmann::json asserts on it (and without assertions silently changes nothing),
/// so the parent is checked first. A move is applied as nlohmann::json does it, a remove and then an add of the value
/// removed, so that the check sees the document after the remove.
/// @param document The document, changed in place (left half changed if this throws).
/// @param operation The operation.
/// @param where Its place, for the error.
/// @throws json::exception If it can't apply.
/// @throws DocumentError If it adds inside a value that holds nothing.
void applyOperation(json& document, json operation, const std::string& where);

/// @brief Applies a delta, one operation at a time (applyOperation()).
/// @param document The document, changed in place.
/// @param patch The operations.
/// @param where Its place, for errors.
/// @throws json::exception If an operation can't apply.
/// @throws DocumentError If one adds inside a value that holds nothing, or the patch isn't a list.
void applyDelta(json& document, const json& patch, const std::string& where);

/// @brief The segments of a JSON Pointer, unescaped: "/a~1b/0" gives {"a/b", "0"}.
/// @param pointer The pointer ("" is the whole document).
/// @return Its segments.
/// @throws DocumentError If it doesn't start with '/'.
[[nodiscard]] std::vector<std::string> segmentsOf(std::string_view pointer);

/// @brief Escapes an object key for a JSON Pointer (RFC 6901).
/// @param key The key.
/// @return It, with '~' as "~0" and '/' as "~1".
[[nodiscard]] std::string escapeKey(std::string_view key);

/// @brief Writes segments as a JSON Pointer, escaping '~' and '/'.
/// @param segments The segments.
/// @return The pointer.
[[nodiscard]] std::string pointerOf(const std::vector<std::string>& segments);

} // namespace affresco::vcs::detail
