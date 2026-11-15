/// @file
/// @brief vcs::Runs: who may run which command, and the runs that wait. Not built yet: the seams f4b fills in.

#include "affresco/vcs/Runs.hpp"

#include <utility>

namespace affresco::vcs {

namespace {

/// The answer of every part that isn't built yet.
constexpr std::string_view notBuilt = "commands can't be run yet";

} // namespace

// f4b: the rules of permissions.run, the mode, the reviewers and the grants, in the plan's order
RunJudgement Runs::judge([[maybe_unused]] std::string_view author, [[maybe_unused]] std::string_view command,
                         [[maybe_unused]] const CommandReach& reach) const {
	return {RunVerdict::Deny, std::string(notBuilt)};
}

// f4b: numbers the run and keeps it, at most maxWaitingRuns waiting
RunControl::Begun Runs::begin([[maybe_unused]] std::string_view author, [[maybe_unused]] std::string_view command,
                              [[maybe_unused]] const json& arguments, [[maybe_unused]] std::string_view title,
                              [[maybe_unused]] RunVerdict verdict, [[maybe_unused]] std::string_view reason) {
	return {0, std::string(notBuilt)};
}

// f4b: allows a waiting run, and grants it with `always`
std::optional<std::string> Runs::allow(int run, [[maybe_unused]] std::string_view reviewer,
                                       [[maybe_unused]] bool always, [[maybe_unused]] std::string_view duration) {
	return "no run " + std::to_string(run) + " waits";
}

// f4b: denies a waiting run
std::optional<std::string> Runs::deny(int run, [[maybe_unused]] std::string_view reason) {
	return "no run " + std::to_string(run) + " waits";
}

// f4b: records how a run ended
void Runs::finish([[maybe_unused]] int run, [[maybe_unused]] RunState state, [[maybe_unused]] std::string_view reason,
                  [[maybe_unused]] std::size_t changes, [[maybe_unused]] std::vector<std::string> errors,
                  [[maybe_unused]] Undoable undoable) {}

// f4b: the newest maxKeptRuns runs
std::vector<RunRecord> Runs::runs() const { return {}; }

// f4b: the grants "always" added
std::vector<Grant> Runs::grants() const { return {}; }

// f4b: withdraws a grant
bool Runs::revoke([[maybe_unused]] std::string_view rule) { return false; }

// f4b: refuses the runs that waited too long, drops the grants that expired
void Runs::expire([[maybe_unused]] std::int64_t now) {}

// f4b: the runner's data rules on what the run changed in the state
std::optional<std::string> Runs::stateDenial([[maybe_unused]] std::string_view author,
                                             [[maybe_unused]] const json& stateBefore,
                                             [[maybe_unused]] const json& stateAfter) const {
	return std::string(notBuilt);
}

} // namespace affresco::vcs
