/// @file
/// @brief format::CommandRuns: runs a command as its runner. Not built yet: the seams f4d fills in.

#include "CommandRuns.hpp"
#include "CommandReach.hpp"

#include <string>

namespace affresco::format {

// f4d: the steps under the runner's StepAuthority, the state judged by effect, the changes kept for undo
// NOLINTNEXTLINE(readability-convert-member-functions-to-static): f4d runs the steps on the engine
CommandOutcome CommandRuns::run([[maybe_unused]] const CommandRun& run) {
	CommandOutcome outcome;
	outcome.refusal = std::string(notBuiltYet);
	return outcome;
}

CommandOutcome CommandRuns::undo(int run) {
	CommandOutcome outcome;
	if(std::optional<std::string> error = undo_.undo(run)) outcome.refusal = std::move(error);
	return outcome;
}

} // namespace affresco::format
