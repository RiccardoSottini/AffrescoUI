/// @file
/// @brief The app's commands as the in-process agent tells a model of them (Commands.hpp).

#include "Commands.hpp"

#include <algorithm>
#include <format>
#include <stdexcept>
#include <utility>

namespace affresco::intelligence {

using affresco::json;

namespace {

/// @brief A value as text: a string as itself, anything else as JSON.
/// @param value The value.
/// @return It.
std::string textOf(const json& value) {
	return value.is_string() ? value.get<std::string>() : value.dump(-1, ' ', false, json::error_handler_t::replace);
}

/// @brief An enum's values in a few words, joined by "|" (null left out).
/// @param choices The values.
/// @return The words, or "null" when only null is left.
std::string enumText(const json& choices) {
	std::string out;
	for(const json& choice : choices)
		if(!choice.is_null()) out += (out.empty() ? "" : "|") + oneLine(textOf(choice));
	return out.empty() ? "null" : out;
}

/// @brief An argument's type in a few words: its enum's values joined by "|", "list of <type>", its basic type, or
/// "any" for {} (and anything that isn't a schema).
/// @param schema The argument's schema.
/// @return The words.
std::string typeText(const json& schema) {
	if(!schema.is_object()) return "any";
	if(const auto choices = schema.find("enum"); choices != schema.end() && choices->is_array())
		return enumText(*choices);
	const json kinds = schema.value("type", json());
	std::string out;
	for(const json& kind : kinds.is_array() ? kinds : json::array({kinds})) {
		if(!kind.is_string() || (kind == "null" && kinds.is_array() && kinds.size() > 1)) continue;
		const std::string word =
		    kind == "array" ? "list of " + typeText(schema.value("items", json())) : oneLine(kind.get<std::string>());
		out += (out.empty() ? "" : "|") + word;
	}
	return out.empty() ? "any" : out;
}

/// @brief One argument as a command's line shows it: `name: type`, `name?` when it has a default, then ` = default`.
/// @param name Its name.
/// @param schema Its schema.
/// @return The text.
std::string argumentText(const std::string& name, const json& schema) {
	const bool defaulted = schema.is_object() && schema.contains("default");
	std::string text = oneLine(name) + (defaulted ? "?: " : ": ") + typeText(schema);
	if(defaulted) text += " = " + oneLine(textOf(schema["default"]));
	return text;
}

/// @brief What a command's line says of running it: nothing when it runs at once, [ask], [person] or [deny: why],
/// and [disabled now] when it can't run now.
/// @param command The command.
/// @return The marks, or "".
std::string marksOf(const json& command) {
	const std::string run = command.value("run", json("allow")).is_string() ? command.value("run", "allow") : "";
	std::string marks;
	if(run == "ask" || run == "person") marks = "[" + run + "]";
	else if(run == "deny")
		marks = command.value("reason", json()).is_string() && !command["reason"].get_ref<const std::string&>().empty()
		            ? "[deny: " + oneLine(command["reason"].get<std::string>()) + "]"
		            : "[deny]";
	if(command.value("enabled", json(true)) == false) marks += std::string(marks.empty() ? "" : " ") + "[disabled now]";
	return marks;
}

/// @brief One command on one line: `name(arguments)  description  [marks]`, the title when it has no description.
/// @param command The command.
/// @return The line.
std::string commandLine(const json& command) {
	std::string listed;
	if(const auto arguments = command.find("arguments"); arguments != command.end() && arguments->is_object())
		for(const auto& [name, schema] : arguments->items())
			listed += (listed.empty() ? "" : ", ") + argumentText(name, schema);
	std::string line = oneLine(textOf(command.value("name", json("")))) + "(" + listed + ")";
	json words = command.value("description", json());
	if(!words.is_string() || words.get_ref<const std::string&>().empty()) words = command.value("title", json());
	if(words.is_string() && !words.get_ref<const std::string&>().empty())
		line += "  " + oneLine(words.get<std::string>());
	if(const std::string marks = marksOf(command); !marks.empty()) line += "  " + marks;
	return line;
}

/// @brief One JSON Patch operation on one line: `replace /state/x 3`, `remove /state/y`, `move /from -> /to`.
/// @param operation The operation.
/// @return The line.
std::string operationLine(const json& operation) {
	if(!operation.is_object()) return oneLine(textOf(operation));
	std::string line = oneLine(textOf(operation.value("op", json("")))) + " ";
	if(operation.contains("from")) line += oneLine(textOf(operation["from"])) + " -> ";
	line += oneLine(textOf(operation.value("path", json(""))));
	if(operation.contains("value"))
		line += " " + oneLine(operation["value"].dump(-1, ' ', false, json::error_handler_t::replace));
	return line;
}

/// @brief How many changes a `ran` answer says a run made, then each on its line.
/// @param answer The answer.
/// @return The text.
std::string changesText(const json& answer) {
	const json& patch = answer.value("changed", json());
	if(!patch.is_array()) {
		const json& count = answer.value("changes", json());
		return count.is_number_integer() ? std::format("{} change{}", count.get<long long>(), count == 1 ? "" : "s")
		                                 : "no change listed";
	}
	if(patch.empty()) return "no change";
	std::string out = std::format("{} change{}", patch.size(), patch.size() == 1U ? "" : "s");
	for(std::size_t at = 0; at < patch.size() && at < maxChangeLines; ++at) out += "\n" + operationLine(patch[at]);
	if(patch.size() > maxChangeLines) out += std::format("\n… {} more", patch.size() - maxChangeLines);
	if(answer.value("truncated", json(false)) == json(true)) out += "\n… more the app left out";
	return out;
}

/// @brief The records a run wrote: `records: tasks +1 ~0 -0`, a collection each.
/// @param records The answer's "records".
/// @return The line, or "".
std::string recordsText(const json& records) {
	if(!records.is_object()) return "";
	std::string parts;
	for(const auto& [name, counts] : records.items()) {
		if(!counts.is_object()) continue;
		parts += std::format("{}{} +{} ~{} -{}", parts.empty() ? "" : ", ", oneLine(name),
		                     oneLine(textOf(counts.value("inserted", json(0)))),
		                     oneLine(textOf(counts.value("updated", json(0)))),
		                     oneLine(textOf(counts.value("deleted", json(0)))));
	}
	return parts.empty() ? "" : "records: " + parts;
}

/// @brief A `ran` answer: what ran and what it changed, its errors, the records, and whether it can be undone.
/// @param answer The answer.
/// @param command The command asked for.
/// @return The lines.
std::string ranText(const json& answer, const std::string& command) {
	const std::string name =
	    answer.value("command", json()).is_string() ? answer["command"].get<std::string>() : command;
	std::string out =
	    "ran " + oneLine(name) + " (run " + oneLine(textOf(answer.value("run", json(0)))) + "): " + changesText(answer);
	std::string errors;
	if(const json& list = answer.value("errors", json()); list.is_array())
		for(const json& error : list) errors += (errors.empty() ? "" : "; ") + oneLine(textOf(error));
	if(!errors.empty()) out += "\nerrors: " + errors;
	if(answer.value("failed", json(false)) == json(true))
		out += "\nfailed: the run stopped and what it changed was rolled back";
	if(const std::string records = recordsText(answer.value("records", json())); !records.empty())
		out += "\n" + records;
	const json& undoable = answer.value("undoable", json());
	if(undoable == "state" || undoable == "partial")
		out += std::string("\nthe person can undo it from its card in the chat") +
		       (undoable == "partial" ? " (what it did outside the app's state stays)" : "");
	return out;
}

} // namespace

std::vector<ToolSpec> commandTools() {
	const json text = {{"type", "string"}};
	return {
	    {"list_commands",
		 "The app's commands: what the app itself can do for the person (add an entry, send a message, sort a list), "
		 "one line each, name(argument: type, ...), what it does, and [ask] when the person must allow each run, "
		 "[person] when the window asks them. filter narrows the list to the commands whose words match.",
		 {{"type", "object"}, {"properties", {{"filter", text}}}}},
	    {"run_command",
		 "Runs one of the app's commands for the person, under your name, with its arguments (an object, an entry per "
		 "argument, from list_commands). The result: ran (with what changed), still waiting for the person's yes, or "
		 "refused with the reason. Never run a command again because it waits or was refused.",
		 {{"type", "object"},
		  {"properties", {{"name", text}, {"arguments", {{"type", "object"}}}}},
		  {"required", json::array({"name"})}}},
	};
}

std::string oneLine(const std::string& text) {
	std::string out;
	std::size_t characters = 0;
	for(const char each : text) {
		const auto byte = static_cast<unsigned char>(each);
		const bool continuation = (byte & 0xC0U) == 0x80U;
		if(!continuation && characters == maxCommandText) return out + "…";
		if(!continuation) ++characters;
		if(byte == '\n' || byte == '\r' || byte == '\t') out += ' ';
		else if(byte >= 0x20U && byte != 0x7FU) out += static_cast<char>(byte);
	}
	return out;
}

std::string commandList(const json& answer) {
	json commands = json::array();
	if(const json& listed = answer.value("commands", json()); listed.is_array())
		for(const json& command : listed)
			if(command.is_object()) commands.push_back(command);
	const std::size_t shown = std::min(commands.size(), maxCommandLines);
	std::string out = std::format("{} command{}", commands.size(), commands.size() == 1U ? "" : "s");
	if(shown < commands.size()) out += std::format(", {} shown", shown);
	if(commands.empty()) out += ": this app has none you may run";
	for(std::size_t at = 0; at < shown; ++at) out += "\n" + commandLine(commands[at]);
	if(shown < commands.size())
		out += std::format("\n… {} more left out: pass filter to narrow the list", commands.size() - shown);
	return out;
}

std::string runText(const json& answer, const std::string& command) {
	const std::string type = answer.value("type", "");
	if(type == "ran") return ranText(answer, command);
	const std::string run = oneLine(textOf(answer.value("run", json(0))));
	if(type == "waiting")
		return "run " + run + " still waits for the person's yes (" +
		       oneLine(textOf(answer.value("reason", json("")))) +
		       "): its card in the chat asks them. Say so in your reply; don't run it again.";
	json why = answer.value("error", json());
	if(!why.is_string()) why = answer.value("message", json("the app refused it"));
	return "refused: " + oneLine(textOf(why));
}

bool runRefused(const json& answer) {
	const std::string type = answer.value("type", "");
	if(type == "ran") return answer.value("failed", json(false)) == json(true);
	return type != "waiting";
}

json runMessage(const json& command, const json& arguments) {
	if(!command.is_string() || command.get_ref<const std::string&>().empty())
		throw std::runtime_error("name must be a command's name, from list_commands");
	if(!arguments.is_null() && !arguments.is_object())
		throw std::runtime_error("arguments must be an object, an entry per argument");
	return {{"type", "runCommand"},
	        {"command", command},
	        {"arguments", arguments.is_null() ? json::object() : arguments},
	        {"evidence", "state"}};
}

} // namespace affresco::intelligence
