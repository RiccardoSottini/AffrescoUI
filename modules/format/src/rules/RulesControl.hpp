#pragma once

/// @file
/// @brief The control scan of format::Rules: what in a document can control changes (accept, revert, patch, ...) or
/// reach the outside world, which a revision may never add or alter, so that no author can approve itself.

#include "affresco/core/Json.hpp"

#include <functional>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace affresco::format {

/// @brief Collects what in a document can control changes or reach the outside world, to compare before and after a
/// revision: every control step, and every reference to an action that may run one, each with its trigger.
/// @param doc The document.
/// @param granted The host's actions its points pass on (grantedActions()).
/// @return The findings as text, sorted.
[[nodiscard]] std::vector<std::string> controlsIn(const json& doc, const std::set<std::string, std::less<>>& granted);

/// @brief Finds a definition of a program that a change alters while it reaches a control or a review step, before or
/// after: as a JSON action holding one, its text may not change, arguments included (`(signIn "other")`), nor may it
/// go. Only a definition that runs code counts: a view, a screen or a section's definition builds a tree, judged as
/// any.
/// @param before The document before.
/// @param after The document after.
/// @param granted The host's actions its points pass on.
/// @return Its address, `file/name`, or nothing.
[[nodiscard]] std::optional<std::string> alteredControlDefinition(const json& before, const json& after,
                                                                  const std::set<std::string, std::less<>>& granted);

} // namespace affresco::format
