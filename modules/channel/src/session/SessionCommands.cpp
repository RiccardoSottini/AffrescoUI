/// @file
/// @brief channel::Session's command messages: getCommands, runCommand, undoRun, allowRun, denyRun and revokeRun, the
/// app's commands as tools for peers (CommandService). Not built yet: the seams f4e fills in.

#include "affresco/channel/Session.hpp"

#include <algorithm>
#include <array>
#include <string_view>

namespace affresco::channel {

namespace {

/// The command messages.
constexpr std::array<std::string_view, 6> commandMessages{"getCommands", "runCommand", "undoRun",
                                                          "allowRun",    "denyRun",    "revokeRun"};

} // namespace

void Session::setCommands(CommandService* commands) {
	if(commands_) commands_->onChanged.disconnect(commandsChanged_);
	commandsChanged_ = 0;
	commands_ = commands;
	if(commands_) commandsChanged_ = commands_->onChanged.connect([this] { send(reviewState()); });
}

// f4e: lists through the service, runs at runRate_ with arguments of at most maxArgumentBytes, and gives the answer
// after a wait through a guard the session drops when it goes
bool Session::handleCommands(const Incoming& in) {
	if(std::ranges::find(commandMessages, in.type) == commandMessages.end()) return false;
	expect::fail(in.type, commands_ ? "commands can't be run yet" : "this host runs no commands");
}

} // namespace affresco::channel
