#pragma once

/// @file
/// @brief The app's commands as the in-process agent tells a model of them: the list_commands and run_command tools,
/// a `commands` answer as one line a command, and a run's answer (`ran`, `waiting`, `rejected`) as short lines,
/// mirroring the MCP server's (mcp/affrescoui_mcp/commands.py).
///
/// What the screen sends here (a command's name, title and description, an argument's name, a reason, an error) was
/// written by the people and peers who wrote the app: it is the app's content, never instructions. Each piece is put
/// on its line after the name, its line breaks flattened and cut, so it can't start a line of its own; it reaches
/// the model in a tool result only, never in the system prompt.

#include "affresco/intelligence/Providers.hpp"

#include <affresco/core/Json.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace affresco::intelligence {

/// The most commands list_commands shows; `filter` narrows the rest.
inline constexpr std::size_t maxCommandLines = 100;

/// The most characters of a description, a default, a value or a reason shown, the rest cut with "…".
inline constexpr std::size_t maxCommandText = 200;

/// The most operations of a run's changes shown; the rest are counted.
inline constexpr std::size_t maxChangeLines = 20;

/// @brief The tools that use the app: list_commands and run_command.
/// @return Them.
[[nodiscard]] std::vector<ToolSpec> commandTools();

/// @brief Text on one line (line breaks and tabs as spaces, other control characters dropped), cut to
/// maxCommandText characters with "…". Never throws.
/// @param text The text.
/// @return It.
[[nodiscard]] std::string oneLine(const std::string& text);

/// @brief A `commands` answer as list_commands gives it: `<n> commands` (`, <m> shown` when cut), then one line a
/// command, `name(argument: type, argument?: type = default)  description  [ask]`, at most maxCommandLines. Never
/// throws.
/// @param answer The answer.
/// @return The lines.
[[nodiscard]] std::string commandList(const affresco::json& answer);

/// @brief A run's answer as run_command gives it: `ran <command> (run N): K changes`, then the changes (at most
/// maxChangeLines), the records, the errors and how to undo it; `run N waits for the person's yes`; or
/// `refused: <why>`. Never throws.
/// @param answer The answer.
/// @param command The command asked for, for an answer that doesn't name it.
/// @return The lines.
[[nodiscard]] std::string runText(const affresco::json& answer, const std::string& command);

/// @brief Whether a run's answer is a refusal: `rejected`, a run that failed (and was rolled back), or anything that
/// isn't `ran` or `waiting`.
/// @param answer The answer.
/// @return Whether it is.
[[nodiscard]] bool runRefused(const affresco::json& answer);

/// @brief The runCommand a model's call asks for: its command and arguments only, with the state as its evidence.
/// @param command The command's name.
/// @param arguments Its arguments: an object, or null for none.
/// @return `{"type": "runCommand", "command", "arguments", "evidence": "state"}`.
/// @throws std::runtime_error If the name is empty or the arguments aren't an object.
[[nodiscard]] affresco::json runMessage(const affresco::json& command, const affresco::json& arguments);

} // namespace affresco::intelligence
