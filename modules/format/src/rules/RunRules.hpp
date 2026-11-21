#pragma once

/// @file
/// @brief The rules of `permissions.run`: who may run which of a document's commands as a peer.

#include "affresco/core/DocumentRules.hpp"
#include "affresco/core/Json.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace affresco::format {

/// How many rules each list of `permissions.run` may hold.
inline constexpr std::size_t maxRunRules = 64;
/// How long a run rule may be, in bytes.
inline constexpr std::size_t maxRunRuleBytes = 256;

/// @brief Parses a run rule: `author`, every command, or `author(command)`, the command's name or a prefix ending in
/// `*` (`agent(card-*)`); `author(*)` is every command too, as is the empty pattern it becomes.
/// @param text The rule.
/// @return The rule, or std::nullopt if it is malformed.
[[nodiscard]] std::optional<PermissionRule> parseRunRule(std::string_view text);

/// @brief Checks `permissions.run`: an object of `allow`, `ask` and `deny`, each a list of at most 64 run rules of at
/// most 256 bytes. An author `role:...` isn't supported yet, and a pattern like a change rule's ("/..." or "#...")
/// is refused with a hint.
/// @param value The value.
/// @param place Its place: "permissions.run".
/// @throws DocumentError If it is malformed.
void checkRunRules(const json& value, const std::string& place);

/// @brief Reads `permissions.run` (checked by checkRunRules()).
/// @param value The value, or null.
/// @return Its rules.
[[nodiscard]] RunRules readRunRules(const json& value);

} // namespace affresco::format
