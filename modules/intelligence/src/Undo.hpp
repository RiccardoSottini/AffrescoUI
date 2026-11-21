#pragma once

/// @file
/// @brief Undoing an earlier change, as the in-process agent offers it to a model (SCREEN_FORMAT.md, "Undoing an
/// earlier commit"): the revert_revision tool, the model's choices checked before anything is sent, the `undoPlan`
/// and `revert` messages built from them, the plan cut to what the model needs, and the host's answer in words.
/// Pure functions, on any thread.

#include "affresco/intelligence/Providers.hpp"

#include <affresco/core/Json.hpp>

#include <cstddef>
#include <optional>
#include <string>

namespace affresco::intelligence {

/// The most conflict paths the model may name in drop and keep together (the host's limit too).
inline constexpr std::size_t maxUndoPaths = 256;

/// What every model is told of undoing an earlier change, with tools or without.
extern const char* const undoRules;

/// How a model without tools (claude-code) writes an undo in its answer, and asks for a plan in a reading round.
extern const char* const undoAnswerRules;

/// @brief The revert_revision tool, mirroring the MCP server's (mcp/affrescoui_mcp/tools.py).
/// @return It.
[[nodiscard]] ToolSpec undoTool();

/// @brief Why the model's undo can't be sent, if it can't: `revision` a number or a commit id; `drop` and `keep` each
/// a list of paths or "all", at most maxUndoPaths paths together; `then` a list of operations; `head` and `message`
/// texts; `dry_run` true or false. The host checks the rest. Never throws.
/// @param input The model's arguments (any JSON).
/// @return The reason, naming the argument, or nullopt.
[[nodiscard]] std::optional<std::string> undoProblem(const affresco::json& input);

/// @brief The message to the host: `undoPlan` (the plan, nothing changes) or `revert` (the undo, by @p author), with
/// the revision and each choice the model gave, and nothing else. The message is a revert's only.
/// @param type "undoPlan" or "revert".
/// @param input The model's arguments, checked by undoProblem().
/// @param author The agent's author name.
/// @return The message.
[[nodiscard]] affresco::json undoMessage(const std::string& type, const affresco::json& input,
                                         const std::string& author);

/// @brief The plan as the model is given it: of the commit to undo only its number, short id and message; the
/// outcome, why it is blocked, the head, the later commits and the conflicts (with how many more, when there are
/// more), the live state and the warnings. The lines of the change, its impact and the open proposals stay out.
/// @param plan The host's plan.
/// @return The summary.
[[nodiscard]] affresco::json planSummary(const affresco::json& plan);

/// @brief Whether a rejection is an undo refused on a conflict: then the plan says what was built on it.
/// @param error The rejection's error.
/// @return Whether it is.
[[nodiscard]] bool undoConflict(const std::string& error);

} // namespace affresco::intelligence
