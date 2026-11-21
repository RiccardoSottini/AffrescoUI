#pragma once

/// @file
/// @brief What a command can reach: worked out from its steps and the actions and functions they run.

#include "affresco/core/Commands.hpp"
#include "affresco/core/Json.hpp"
#include "affresco/core/Services.hpp"

#include <string>
#include <string_view>

namespace affresco::format {

/// What every part of commands as tools that isn't built yet answers.
inline constexpr std::string_view notBuiltYet = "not built yet";

/// @brief What a command can reach. It follows the command's `do`, each `run` of a name written out (the document's
/// action, else the program's function, else the application's own action), each `lisp` step's function and the
/// definitions it uses, advice and hooks:
/// - `control`, with the first reason, when it may run a control or a review step, a computed `run`, an action the
///   document doesn't have, or a component that runs computed actions; or when the program can't be built;
/// - `personUses`: each use of a method written out that writes or runs, as the reviewer reads it ("uses files.write
///   (writes: ...)"); one the screen can't describe counts;
/// - `writesRecords`: a step that writes records (insert, update, delete, undoData, redoData, importCsv).
/// @param document The document (built: with its program).
/// @param name The command's name.
/// @param services The screen's services, or nullptr.
/// @return What it reaches.
[[nodiscard]] CommandReach reachOf(const json& document, const std::string& name, const services::Services* services);

} // namespace affresco::format
