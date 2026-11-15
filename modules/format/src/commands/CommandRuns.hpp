#pragma once

/// @file
/// @brief format::CommandRuns: runs a command under its runner's authority, and undoes a run.

#include "affresco/core/Commands.hpp"

#include "RunUndo.hpp"

namespace affresco::format {

struct Engine;

/// @brief Runs main's commands for peers, each step and record write judged as the runner's, and puts back what a
/// run changed.
///
/// Not built yet (f4d fills it in): every run and undo is refused with "not built yet". Callers:
/// Runtime::runCommandAs(), undoCommandRun() and the undoRun step. Main thread only.
class CommandRuns {
public:
	/// @brief Runs for an engine.
	/// @param engine The runtime; outlives this object.
	explicit CommandRuns(Engine& engine) noexcept : engine_(engine) {}

	CommandRuns(const CommandRuns&) = delete;
	CommandRuns& operator=(const CommandRuns&) = delete;
	CommandRuns(CommandRuns&&) = delete;
	CommandRuns& operator=(CommandRuns&&) = delete;
	~CommandRuns() = default;

	/// @brief Runs a command as its runner: its steps run under the runner's authority, the state it changes is
	/// judged by effect (RunControl::stateDenial()), and a refusal rolls it back whole.
	/// @param run What to run; its arguments already checked.
	/// @return What happened; not built yet: refused.
	CommandOutcome run(const CommandRun& run);
	/// @brief Undoes a run: puts back what it changed, refused when that changed since.
	/// @param run The run's number.
	/// @return What happened; not built yet: refused.
	CommandOutcome undo(int run);

private:
	[[maybe_unused]] Engine& engine_; ///< The runtime (f4d runs its steps).
	RunUndo undo_;                    ///< What the newest runs changed, for undo.
};

} // namespace affresco::format
