/// @file
/// @brief detail::ScreenCommands, and affresco::Screen's commands as tools. Not built yet: the seams f4e fills in.

#include "commands/ScreenCommands.hpp"
#include "ScreenImpl.hpp"

#include <algorithm>
#include <string>
#include <utility>

namespace affresco {

namespace detail {

namespace {

/// @brief The answer of every request that isn't built yet.
/// @return A "rejected".
json notBuiltYet() { return {{"type", "rejected"}, {"error", "not built yet"}}; }

} // namespace

ScreenCommands::ScreenCommands(format::Runtime& runtime, RunControl& control, VersionControl& versions,
                               ScreenImages& images)
    : runtime_(runtime), control_(control), versions_(versions), images_(images) {
	changed_ = control_.onChanged.connect([this] { onChanged(); });
}

ScreenCommands::~ScreenCommands() { control_.onChanged.disconnect(changed_); }

// f4e: main's commands with the run control's verdict for this peer
json ScreenCommands::list([[maybe_unused]] std::string_view author, [[maybe_unused]] std::string_view filter,
                          [[maybe_unused]] bool all) const {
	return notBuiltYet();
}

// f4e: checks the arguments, judges, begins, then runs as the runner at once or after a decision
void ScreenCommands::run([[maybe_unused]] std::string_view author, [[maybe_unused]] const json& message,
                         Answer answer) {
	if(answer) answer(notBuiltYet());
}

// f4e: only the runner's own runs
json ScreenCommands::undo([[maybe_unused]] std::string_view author, [[maybe_unused]] const json& message) {
	return notBuiltYet();
}

// f4e: never the runner's own channel; a personOnly run never from a peer
json ScreenCommands::decide([[maybe_unused]] std::string_view reviewer, [[maybe_unused]] const json& message) {
	return notBuiltYet();
}

// f4e: a limited reader sees its own runs, without their arguments
json ScreenCommands::pending([[maybe_unused]] std::string_view reader) const {
	json runs = json::array(), grants = json::array();
	for(const RunRecord& run : control_.runs()) runs.push_back(toJson(run));
	for(const Grant& grant : control_.grants()) grants.push_back(toJson(grant));
	return {{"runs", std::move(runs)}, {"runGrants", std::move(grants)}};
}

} // namespace detail

void Screen::allowCommandRuns(bool allow) {
	impl_->commandsAllowed = allow;
	for(const auto& peer : impl_->peers) peer->session->setCommands(allow ? &impl_->commands : nullptr);
}

bool Screen::allowRun(int run, bool always, std::string& error) {
	if(const std::optional<std::string> refused = impl_->runs.allow(run, "user", always, "")) {
		error = *refused;
		return false;
	}
	return true;
}

bool Screen::denyRun(int run) { return !impl_->runs.deny(run, "").has_value(); }

json Screen::commandsJson() const {
	json commands = impl_->commands.pending("user");
	commands["commands"] = impl_->runtime.commandList();
	return commands;
}

} // namespace affresco
