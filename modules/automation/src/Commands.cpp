/// @file
/// @brief The `command` step: a scenario runs one of the app's commands as an author's peer would, over the live
/// protocol and on the scenario's own screen, so the arguments are checked, the run rules judge, and it runs under
/// that author's name. detail::Peers holds the channels it speaks on.

#include "Player.hpp"

#include <affresco/Screen.hpp>

#include <string>
#include <utility>

namespace affresco::automation::detail {

namespace {

/// @brief What a verdict means for a run, as a failure says it.
/// @param name The command.
/// @param verdict "allow", "ask", "person" or "deny".
/// @param reason Why, as the run rules say it; may be empty.
/// @return E.g. "'addCard' would ask (no run rule allows 'addCard' for 'agent'; ...)".
std::string verdictText(const std::string& name, const std::string& verdict, const std::string& reason) {
	const std::string why = reason.empty() ? "" : " (" + reason + ")";
	if(verdict == "allow") return "'" + name + "' would run";
	if(verdict == "ask") return "'" + name + "' would ask" + why;
	if(verdict == "person") return "'" + name + "' would wait for the person" + why;
	return "'" + name + "' is denied" + why;
}

/// @brief Finds a command in a "commands" answer by its exact name.
/// @param listed The answer.
/// @param name The command.
/// @return Its entry, or nullptr.
const json* findCommand(const json& listed, const std::string& name) {
	const auto commands = listed.find("commands");
	if(commands == listed.end() || !commands->is_array()) return nullptr;
	for(const json& entry : *commands)
		if(entry.is_object() && entry.value("name", json()) == name) return &entry;
	return nullptr;
}

/// @brief The names in a "commands" answer, for a failure that names a command there is none of.
/// @param listed The answer.
/// @return E.g. "addCard, moveCard", at most 16 of them; "none" when empty.
std::string commandNames(const json& listed) {
	std::string names;
	std::size_t count = 0;
	for(const json& entry : listed.value("commands", json::array())) {
		if(!entry.is_object() || !entry.value("name", json()).is_string()) continue;
		if(++count > 16U) return names + ", ...";
		names += (names.empty() ? "" : ", ") + entry["name"].get<std::string>();
	}
	return names.empty() ? "none" : names;
}

} // namespace

json Peers::ask(Screen& screen, std::string_view author, json message) {
	const std::string_view channel = author.substr(0, author.find('/'));
	auto line = lines_.find(channel);
	if(line == lines_.end()) {
		line = lines_.emplace(std::string(channel), std::make_unique<Line>()).first;
		screen.connect(*line->second, std::string(channel));
	}
	if(author.size() > channel.size()) message["author"] = std::string(author.substr(channel.size() + 1U));
	message["id"] = ++nextId_;
	line->second->sent.clear(); // what came before (review messages, earlier answers) answers nothing now
	line->second->onMessage(message.dump());
	for(const std::string& sent : line->second->sent) {
		json answer = json::parse(sent, nullptr, false);
		if(answer.is_object() && answer.value("id", json()) == nextId_) return answer;
	}
	return {{"type", "rejected"}, {"error", "the screen gave no answer to " + message.value("type", "")}};
}

void Player::command(const json& step, std::size_t number) {
	const std::string name = step["command"].get<std::string>();
	const std::string author = step.value("as", "agent");
	const json listed = stage_.peers.ask(stage_.screen, author, {{"type", "getCommands"}, {"all", true}});
	if(listed.value("type", "") != "commands")
		throw StepError("'" + author + "' can't list the commands: " + listed.value("error", ""));
	const json* entry = findCommand(listed, name);
	if(!entry)
		throw StepError("no command '" + name + "' that '" + author + "' sees (it sees " + commandNames(listed) + ")");
	const std::string verdict = entry->value("run", "deny");
	const std::string reason = entry->value("reason", "");

	if(step.contains("verdict")) { // an expectation: it runs nothing, and the scenario goes on whatever it finds
		if(step["verdict"] != verdict)
			result_.failures.push_back(
			    {number, verdictText(name, verdict, reason) + ", expected " + step["verdict"].get<std::string>()});
		return;
	}
	// Only a run that goes ahead at once is played: one that would wait has nobody to decide it here
	if(verdict != "allow") throw StepError(verdictText(name, verdict, reason));
	const json ran = stage_.peers.ask(
	    stage_.screen, author,
	    {{"type", "runCommand"}, {"command", name}, {"arguments", step.value("with", json::object())}, {"evidence", "none"}});
	if(ran.value("type", "") != "ran")
		throw StepError("'" + name + "' was refused: " + ran.value("error", ran.value("type", "")));
	if(ran.value("failed", false)) {
		const json errors = ran.value("errors", json::array());
		throw StepError("'" + name + "' failed" +
		                (errors.empty() || !errors[0].is_string() ? "" : ": " + errors[0].get<std::string>()));
	}
}

} // namespace affresco::automation::detail
