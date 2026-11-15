#pragma once

/// @file
/// @brief The typed `arguments` of a command: checked against what it declares, defaults filled in.

#include "affresco/core/Json.hpp"

#include <cstddef>
#include <optional>
#include <string>

namespace affresco::format {

/// @brief Checks a run's arguments against what a command declares (`arguments`: name → `{type, required?,
/// default?, description?}`), and fills in their defaults. Not built yet (f4c fills it in): refuses every run.
/// @param command The command, as CommandCatalog::list() gives it.
/// @param arguments The arguments: an object, changed in place.
/// @return Why they are refused, or std::nullopt.
std::optional<std::string> checkCommandArguments(const json& command, json& arguments);

} // namespace affresco::format
