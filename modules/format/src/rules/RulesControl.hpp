#pragma once

/// @file
/// @brief The control scan of format::Rules: what in a document can control changes (accept, revert, patch, ...) or
/// reach the outside world, which a revision may never add or alter, so that no author can approve itself.

#include "affresco/core/Json.hpp"

#include <functional>
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

} // namespace affresco::format
