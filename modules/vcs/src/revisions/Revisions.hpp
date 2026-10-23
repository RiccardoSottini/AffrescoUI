#pragma once

/// @file
/// @brief What a revision says: who wrote it, why, and its changes as JSON Patch; and the time commits are stamped
/// with. Shared by vcs::Repository's parts.

#include "affresco/core/Json.hpp"

#include <string>

namespace affresco::vcs::detail {

/// @brief The current time.
/// @return UTC, ISO 8601.
[[nodiscard]] std::string utcNow();

/// @brief A time from now.
/// @param seconds How far ahead.
/// @return UTC, ISO 8601.
[[nodiscard]] std::string utcAfter(long long seconds);

/// @brief A revision's author.
/// @param revision The revision.
/// @return "author", or "unknown".
[[nodiscard]] std::string authorOf(const json& revision);

/// @brief A revision's message.
/// @param revision The revision.
/// @return "message" as text.
[[nodiscard]] std::string messageOf(const json& revision);

/// @brief A revision's changes as JSON Patch: its "patch", then its compact edits in a fixed order: "set" (replace,
/// or add where nothing is there yet), "add", "remove", "move". `{"set": {"#statusBar/color": "@topBar"}}`.
/// @param revision The revision.
/// @return The operations ("set" ones keep op "set" until prepare() sees the document).
/// @throws DocumentError If it has neither, or they are malformed.
[[nodiscard]] json patchOf(const json& revision);

} // namespace affresco::vcs::detail
