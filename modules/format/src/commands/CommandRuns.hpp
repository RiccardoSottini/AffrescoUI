#pragma once

/// @file
/// @brief format::CommandRuns: runs a command under its runner's authority, and undoes a run.

#include "affresco/core/Commands.hpp"

#include "RunUndo.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace affresco::format {

struct Engine;

/// @brief Runs main's commands for peers, each step and record write judged as the runner's, and puts back what a
/// run changed.
///
/// A run executes the command's steps from the window's root under StepAuthority{runner}: a control or review step,
/// a native action, or a record write the data rules refuse is refused as the runner's wherever it is reached (another
/// action, a program function, advice, a hook, and what any of them schedules: a timer, a call's callbacks, a sound's
/// actions). What the run changed in the state is judged by effect (RunControl::stateDenial()); a run refused so, or
/// ended by an error, is rolled back whole: the state as it was, its record writes undone. Effects of what it
/// scheduled that land after it returned aren't part of its outcome, and can't be undone with it.
///
/// The state is copied before each run (its cost grows with the state). Callers: Runtime::runCommandAs(),
/// undoCommandRun() and the undoRun step. Main thread only.
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
	/// @return What happened: refused (never started) under the person's name, without a window, while a proposal is
	/// previewed, while another run runs, or for what isn't a command of the screen.
	CommandOutcome run(const CommandRun& run);
	/// @brief Undoes a run: puts back what it changed, refused when that changed since. Tells the RunControl, if any
	/// (RunState::Undone).
	/// @param run The run's number.
	/// @return What happened: `refusal` set when it was refused.
	CommandOutcome undo(int run);

	/// The most errors a run's outcome keeps, and the most bytes each keeps.
	static constexpr std::size_t maxErrors = 8;
	static constexpr std::size_t maxErrorBytes = 512; ///< See maxErrors.
	/// The most operations `changed` holds with their values, and the most bytes it holds (CommandOutcome::changed).
	static constexpr std::size_t maxChangedOperations = 64;
	static constexpr std::size_t maxChangedBytes = std::size_t{16} << 10U; ///< See maxChangedOperations.

private:
	/// @brief Why a run can't start now, if it can't.
	/// @param run The run.
	/// @return The reason, or std::nullopt.
	[[nodiscard]] std::optional<std::string> startRefusal(const CommandRun& run) const;
	/// @brief Puts the state and the records back as they were before a run that was refused while it ran.
	/// @param before The state before it.
	/// @param groups The journal groups of its record writes.
	/// @return Why the records couldn't all be put back, or std::nullopt.
	std::optional<std::string> rollBack(const json& before, const std::vector<std::int64_t>& groups);
	/// @brief Fills in what a run changed in the state: `changed`, as a JSON Patch under "/state", within its limits.
	/// @param patch What it changed.
	/// @param outcome Its outcome.
	static void describe(const json& patch, CommandOutcome& outcome);

	Engine& engine_;       ///< The runtime.
	RunUndo undo_;         ///< What the newest runs changed, for undo.
	bool running_ = false; ///< A run is running: another one waits for it to end.
};

} // namespace affresco::format
