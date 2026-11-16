/// @file
/// @brief channel::Session's command messages: getCommands, runCommand, undoRun, allowRun, denyRun and revokeRun, the
/// app's commands as tools for peers (CommandService).
///
/// The session reads and checks every field, holds runs to their rate and their arguments to their size, refuses the
/// decisions of a peer that may not review, and limits each answer to what the peer may read. The service decides the
/// rest. A run that waits is answered again when it ran or was refused, with the id of the runCommand that asked; a
/// session that leaves the service first denies its runs that wait.

#include "affresco/channel/Session.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <string_view>
#include <utility>

namespace affresco::channel {

namespace {

/// The command messages.
constexpr std::array<std::string_view, 6> commandMessages{"getCommands", "runCommand", "undoRun",
                                                          "allowRun",    "denyRun",    "revokeRun"};

/// The longest `filter` of a getCommands, in bytes: it is compared with every command's words.
constexpr std::size_t maxFilterBytes = 256;
/// The longest text of a `reason`, a `rule` or a `for`, in bytes.
constexpr std::size_t maxTextBytes = 1024;

/// The fields of a "ran" answer that show what a run did to the state or the screen, or its errors (which can quote
/// values): never for a limited reader.
constexpr std::array<std::string_view, 7> shownFields{"changed", "region", "picture", "width",
                                                      "height",  "scale",  "errors"};

/// @brief Writes a count with a comma between thousands, as people read it.
/// @param count The count.
/// @return E.g. "65,536".
std::string withCommas(std::size_t count) {
	std::string digits = std::to_string(count);
	for(std::size_t at = digits.size(); at > 3U; at -= 3U) digits.insert(at - 3U, ",");
	return digits;
}

/// @brief Reads an optional text field.
/// @param message The message.
/// @param field The field.
/// @param place The place an error names, e.g. "getCommands.filter".
/// @param limit Its most bytes.
/// @return The text, or "" when it is absent or null.
/// @throws DocumentError Naming @p place, if it isn't text or is longer than @p limit.
std::string optionalText(const json& message, const char* field, const std::string& place, std::size_t limit) {
	const auto it = message.find(field);
	if(it == message.end() || it->is_null()) return "";
	if(!it->is_string()) expect::fail(place, "expected text");
	std::string text = it->get<std::string>();
	if(text.size() > limit) expect::fail(place, "at most " + withCommas(limit) + " bytes");
	return text;
}

/// @brief Reads an optional true-or-false field.
/// @param message The message.
/// @param field The field.
/// @param place The place an error names.
/// @return Its value; false when it is absent or null.
/// @throws DocumentError Naming @p place, if it is neither.
bool optionalFlag(const json& message, const char* field, const std::string& place) {
	const auto it = message.find(field);
	if(it == message.end() || it->is_null()) return false;
	if(!it->is_boolean()) expect::fail(place, "expected true or false");
	return it->get<bool>();
}

/// @brief Reads the `run` a message names.
/// @param message The message.
/// @param type Its type, for the place an error names.
/// @return The run's number.
/// @throws DocumentError If it is missing or isn't a whole number from 1.
int runOf(const json& message, const std::string& type) {
	const json& run = message.value("run", json());
	if(!run.is_number_integer() || run.get<std::int64_t>() < 1 ||
	   run.get<std::int64_t>() > std::numeric_limits<int>::max())
		expect::fail(type + ".run", "expected a run's number");
	return run.get<int>();
}

/// @brief Reads a runCommand's arguments.
/// @param message The message.
/// @param limits What the peer may send.
/// @return Its arguments: an object, empty when they are absent or null.
/// @throws DocumentError If they aren't an object, or are longer than PeerLimits::maxArgumentBytes as JSON.
json checkedArguments(const json& message, const PeerLimits& limits) {
	json arguments = message.value("arguments", json());
	if(arguments.is_null()) arguments = json::object();
	if(!arguments.is_object()) expect::fail("runCommand.arguments", "expected an object");
	if(const std::size_t bytes = arguments.dump(-1, ' ', false, json::error_handler_t::replace).size();
	   bytes > limits.maxArgumentBytes)
		expect::fail("runCommand.arguments",
		             withCommas(bytes) + " bytes, at most " + withCommas(limits.maxArgumentBytes));
	return arguments;
}

/// @brief Reads a runCommand's scale.
/// @param message The message.
/// @return Its scale; 1 when it is absent or null.
/// @throws DocumentError If it isn't a whole number from 1 to ScreenImages::maxScale.
int checkedScale(const json& message) {
	const json& given = message.value("scale", json());
	if(given.is_null()) return 1;
	if(!given.is_number_integer() || given.get<std::int64_t>() < 1 ||
	   given.get<std::int64_t>() > ScreenImages::maxScale)
		expect::fail("runCommand.scale", "expected 1 to " + std::to_string(ScreenImages::maxScale));
	return given.get<int>();
}

/// @brief Checks a runCommand and gives it as the service reads it: only the fields it knows.
/// @param message The message.
/// @param limits What the peer may send.
/// @param limited Whether the peer's reads are limited: it never gets a picture, nor what changed.
/// @return `{"command", "arguments", "evidence", "scale"}`.
/// @throws DocumentError Naming the first field that is wrong.
json checkedRun(const json& message, const PeerLimits& limits, bool limited) {
	const json& command = message.value("command", json());
	if(!command.is_string() || command.get_ref<const std::string&>().empty())
		expect::fail("runCommand.command", "expected text");
	if(command.get_ref<const std::string&>().size() > maxTextBytes)
		expect::fail("runCommand.command", "at most " + withCommas(maxTextBytes) + " bytes");
	json arguments = checkedArguments(message, limits);
	std::string evidence = optionalText(message, "evidence", "runCommand.evidence", maxTextBytes);
	if(evidence.empty()) evidence = "state";
	if(evidence != "state" && evidence != "picture" && evidence != "none")
		expect::fail("runCommand.evidence", "expected \"state\", \"picture\" or \"none\"");
	const int scale = checkedScale(message);
	if(limited) evidence = "none";
	return {{"command", command}, {"arguments", std::move(arguments)}, {"evidence", evidence}, {"scale", scale}};
}

/// @brief Checks a reviewer's decision and gives it as the service reads it: only the fields it knows.
/// @param message The message: allowRun, denyRun or revokeRun.
/// @param type Its type.
/// @return The decision.
/// @throws DocumentError Naming the first field that is wrong.
json checkedDecision(const json& message, const std::string& type) {
	if(type == "revokeRun") {
		const std::string rule = optionalText(message, "rule", "revokeRun.rule", maxTextBytes);
		if(rule.empty()) expect::fail("revokeRun.rule", "expected text");
		return {{"type", type}, {"rule", rule}};
	}
	json decision = {{"type", type}, {"run", runOf(message, type)}};
	if(type == "denyRun") {
		decision["reason"] = optionalText(message, "reason", "denyRun.reason", maxTextBytes);
		return decision;
	}
	decision["always"] = optionalFlag(message, "always", "allowRun.always");
	decision["for"] = optionalText(message, "for", "allowRun.for", maxTextBytes);
	return decision;
}

/// @brief Tells whether a message is a reviewer's decision on runs.
/// @param type Its type.
/// @return Whether it is allowRun, denyRun or revokeRun.
bool isDecision(std::string_view type) { return type == "allowRun" || type == "denyRun" || type == "revokeRun"; }

/// @brief Tells whether a reader's view shows a command: its entry in the document's actions.
/// @param view The document as the reader may read it.
/// @param name The command's name.
/// @return Whether `/actions/<name>` is in it.
bool shows(const json& view, const json& name) {
	if(!name.is_string()) return false;
	const auto actions = view.find("actions");
	return actions != view.end() && actions->is_object() && actions->contains(name.get_ref<const std::string&>());
}

/// @brief Keeps in a "commands" answer only those its reader's view shows.
/// @param answer The answer, changed in place.
/// @param view The document as the reader may read it.
void limitCommands(json& answer, const json& view) {
	const auto commands = answer.find("commands");
	if(commands == answer.end() || !commands->is_array()) return;
	json shown = json::array();
	for(json& command : *commands)
		if(command.is_object() && shows(view, command.value("name", json()))) shown.push_back(std::move(command));
	*commands = std::move(shown);
}

/// @brief Limits an answer to what its reader may read.
/// @param answer The answer, changed in place.
/// @param view The document as the reader may read it, if its reads are limited.
void limitFor(json& answer, const std::optional<json>& view) {
	const std::string type = answer.value("type", "");
	if(type == "commands" && view) return limitCommands(answer, *view);
	if(type == "rejected" && view && answer.value("failed", false)) answer["error"] = "the run failed while it ran";
	if(type != "ran" && type != "undone") return;
	if(view)
		for(const std::string_view field : shownFields) answer.erase(std::string(field));
	else if(answer.contains("changed")) answer.erase("changes"); // the patch says it, and more
}

/// @brief The run an answer names.
/// @param answer The answer.
/// @return Its `run`, or 0.
int runIn(const json& answer) {
	const json& run = answer.value("run", json());
	return run.is_number_integer() ? run.get<int>() : 0;
}

} // namespace

void Session::setCommands(CommandService* commands) {
	if(commands == commands_) return;
	if(commands_) {
		commands_->onChanged.disconnect(commandsChanged_);
		// Its runs that wait end with it: a yes given later would run a command for a peer that can't hear it
		const std::vector<int> waiting = std::exchange(waitingRuns_, {});
		for(const int run : waiting)
			(void)commands_->decide(author_, {{"type", "denyRun"}, {"run", run}, {"reason", "the peer went away"}});
	}
	commandsAlive_.reset(); // what the service still holds for this session is dropped
	commandsChanged_ = 0;
	commands_ = commands;
	if(!commands_) return;
	commandsAlive_ = std::make_shared<bool>(true);
	commandsChanged_ = commands_->onChanged.connect([this] { send(reviewState()); });
}

// Lists through the service; runs at runRate_ with arguments of at most maxArgumentBytes; reviewers alone decide; the
// answer after a wait comes through a guard the session drops when it leaves the service
bool Session::handleCommands(const Incoming& in) {
	if(std::ranges::find(commandMessages, in.type) == commandMessages.end()) return false;
	if(!commands_) expect::fail(in.type, "this host runs no commands");
	const json& m = in.message;
	const auto answer = [this, id = in.id](json message) {
		message["id"] = id;
		message["revision"] = versions_.head().number;
		limitFor(message, versions_.readable(author_));
		send(message);
	};

	if(in.type == "getCommands") {
		const std::string filter = optionalText(m, "filter", "getCommands.filter", maxFilterBytes);
		answer(commands_->list(in.author, filter, optionalFlag(m, "all", "getCommands.all")));
		return true;
	}
	if(isDecision(in.type)) {
		if(!versions_.mayReview(author_))
			expect::fail(in.type, "permission denied: '" + author_ + "' may not review runs");
		answer(commands_->decide(in.author, checkedDecision(m, in.type)));
		return true;
	}
	// runCommand and undoRun run steps: they share a rate
	if(!runRate_.take(TokenBucket::Clock::now())) {
		const std::string what = "too many runs: at most " + toText(limits_.runsPerMinute) + " a minute";
		overLimit(what);
		throw DocumentError(what);
	}
	if(in.type == "undoRun") {
		answer(commands_->undo(in.author, {{"type", "undoRun"}, {"run", runOf(m, "undoRun")}}));
		return true;
	}
	const json run = checkedRun(m, limits_, versions_.readable(author_).has_value());
	if(run["evidence"] == "picture" && !pictureRate_.take(TokenBucket::Clock::now())) {
		const std::string what = "too many pictures: at most " + toText(limits_.revisionsPerMinute) + " a minute";
		overLimit(what);
		throw DocumentError(what);
	}
	commands_->run(in.author, run, [answer, this, alive = std::weak_ptr<bool>(commandsAlive_)](const json& message) {
		if(alive.expired()) return; // the session left the service, or is gone
		if(message.value("type", "") == "waiting") waitingRuns_.push_back(runIn(message));
		else std::erase(waitingRuns_, runIn(message));
		answer(message);
	});
	return true;
}

} // namespace affresco::channel
