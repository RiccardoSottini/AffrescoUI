#pragma once

/// @file
/// @brief What a command can reach: worked out from its steps and the actions and functions they run.

#include "affresco/core/Commands.hpp"
#include "affresco/core/Json.hpp"

#include <string_view>

namespace affresco::format {

/// What every part of commands as tools that isn't built yet answers.
inline constexpr std::string_view notBuiltYet = "not built yet";

/// @brief What a command can reach: a control or review step, a native or unknown action, a computed `run`; the
/// service uses that write or run; record writes. Not built yet (f4c fills it in): every command reaches control, so
/// no peer may run it.
/// @param command The command, as CommandCatalog::list() gives it.
/// @return What it reaches.
[[nodiscard]] CommandReach reachOf(const json& command);

} // namespace affresco::format
