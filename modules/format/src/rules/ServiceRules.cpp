/// @file
/// @brief The checks of the service steps a revision adds (ServiceRules.hpp).

#include "ServiceRules.hpp"
#include "ConnectionSpec.hpp"
#include "TerminalView.hpp"

#include <algorithm>
#include <cctype>
#include <iterator>
#include <optional>
#include <ranges>
#include <utility>

namespace affresco::format {

namespace {

/// @brief A use's name as text: "{{computed}}" if it isn't text.
/// @param name The name.
/// @return The text.
std::string nameText(const json& name) {
	return name.is_string() ? name.get<std::string>() : std::string("{{computed}}");
}

/// @brief Collects a `terminal` node's use: it runs its service's program, as a use of its "open" with its session
/// (TerminalView.hpp). (Its steps, `runInTerminal` among them, are control steps: no revision adds one.)
/// @param value An object.
/// @param found Receives the use.
void collectTerminalUse(const json& value, std::vector<std::string>& found) {
	const auto type = value.find("type");
	if(type == value.end() || *type != "terminal") return;
	const auto service = value.find("service");
	found.push_back("invoke " + (service == value.end() ? std::string(defaultService) : nameText(*service)) +
	                ".open\n" + json{{"session", value.value("session", json())}}.dump());
}

/// @brief Collects the "invoke", "request" and "subscribe" steps in a value, as "<step> <name>" (e.g. "request
/// api.save"), then a line with the step's "with" if it has one: a use whose arguments change is a new use (a write to
/// another path). A name that isn't text counts as "{{computed}}". A terminal node counts too (collectTerminalUse()).
/// @param value The value.
/// @param found Receives the uses.
void collectUses(const json& value, std::vector<std::string>& found) {
	if(value.is_object()) {
		for(const char* key : {"invoke", "request", "subscribe"}) {
			if(!value.contains(key)) continue;
			found.push_back(std::string(key) + " " + nameText(value[key]) +
			                (value.contains("with") ? "\n" + value["with"].dump() : std::string()));
		}
		collectTerminalUse(value, found);
		for(const auto& [key, item] : value.items()) collectUses(item, found);
	} else if(value.is_array()) {
		for(const json& item : value) collectUses(item, found);
	}
}

/// @brief The uses in a document, sorted: everywhere but its state, services and connections, which hold values.
/// @param document The document.
/// @return The names.
std::vector<std::string> usesIn(const json& document) {
	std::vector<std::string> found;
	if(!document.is_object()) return found;
	for(const auto& [key, value] : document.items())
		if(key != "state" && key != "services" && key != "connections") collectUses(value, found);
	std::ranges::sort(found);
	return found;
}

/// @brief The uses a revision adds: those after it that weren't before, counted (a second copy is added too), a use
/// whose arguments changed among them.
/// @param before The document before.
/// @param after The document after.
/// @return The names, sorted.
std::vector<std::string> addedUses(const json& before, const json& after) {
	const std::vector<std::string> old = usesIn(before), now = usesIn(after);
	std::vector<std::string> added;
	std::ranges::set_difference(now, old, std::back_inserter(added));
	return added;
}

/// @brief The connections a document declares; none if its section is wrong (its own check refuses it).
/// @param document The document.
/// @return The connections.
ConnectionMap connectionsIn(const json& document) {
	if(!document.is_object() || !document.contains("connections")) return {};
	try {
		return parseConnections(document["connections"]);
	} catch(const DocumentError&) {
		return {};
	}
}

/// @brief Splits a use into its step and its name, leaving out its arguments.
/// @param use "<step> <name>", then perhaps a line with its arguments.
/// @return The step and the name.
std::pair<std::string, std::string> split(const std::string& use) {
	const std::size_t space = use.find(' ');
	const std::size_t line = use.find('\n', space);
	return {use.substr(0, space), use.substr(space + 1, line == std::string::npos ? line : line - space - 1)};
}

/// @brief The state a use's arguments read that a revision changes: the first `state.<key>` its arguments name whose
/// value differs, or the whole state if they name it otherwise (`state[...]`) and any of it differs. A use reads
/// what it names only: state copied first into other state, or passed in by the caller, isn't followed.
/// @param use The use, "<step> <name>" then its arguments.
/// @param before The document before the revision.
/// @param after The document after it.
/// @return "state.<key>" or "state", or "" if what it reads is the same.
std::string changedStateRead(const std::string& use, const json& before, const json& after) {
	static const json none = json::object();
	const json& old = before.is_object() && before.contains("state") ? before["state"] : none;
	const json& now = after.is_object() && after.contains("state") ? after["state"] : none;
	const std::size_t line = use.find('\n');
	if(line == std::string::npos || old == now) return "";
	const std::string_view arguments = std::string_view(use).substr(line + 1);
	const auto identifier = [](char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '$'; };
	for(std::size_t at = arguments.find("state"); at != std::string_view::npos; at = arguments.find("state", at + 5)) {
		if(at > 0 && (identifier(arguments[at - 1]) || arguments[at - 1] == '.')) continue; // "mystate", "event.state"
		std::size_t end = at + 5;
		if(end < arguments.size() && identifier(arguments[end])) continue;   // "states"
		if(end >= arguments.size() || arguments[end] != '.') return "state"; // all of it, or a key computed
		const std::size_t start = ++end;
		while(end < arguments.size() && identifier(arguments[end])) ++end;
		const std::string key(arguments.substr(start, end - start));
		if(old.value(key, json()) != now.value(key, json())) return "state." + key;
	}
	return "";
}

/// @brief The reviewer's line for a use of a method or operation that writes or runs; none for one that only reads.
/// @param step The step ("invoke", "request" or "subscribe").
/// @param name What it uses: "service.method".
/// @param connections The connections after the revision.
/// @param services The host's services, or nullptr.
/// @return The line, or std::nullopt.
std::optional<std::string> useLine(const std::string& step, const std::string& name, const ConnectionMap& connections,
                                   const services::Services* services) {
	const std::size_t dot = name.find('.');
	const std::string service = dot == std::string::npos ? name : name.substr(0, dot);
	const auto connection = connections.find(service);
	if(connection != connections.end() && (step == "request" || step == "subscribe")) {
		const auto operation = connection->second.operations.find(name.substr(dot + 1));
		const services::MethodSpec spec = operation->second.spec(); // checkServiceUses() found it
		if(spec.effect == services::Effect::Read) return std::nullopt;
		return "uses " + name + " (" + std::string(services::effectName(spec.effect)) + ": " + spec.about + ")";
	}
	const services::MethodSpec* spec =
	    services && dot != std::string::npos ? services->method(service, name.substr(dot + 1)) : nullptr;
	if(!spec) return "uses " + name + " (a method the screen can't describe yet)";
	if(spec->effect == services::Effect::Read) return std::nullopt;
	return "uses " + name + " (" + std::string(services::effectName(spec->effect)) +
	       (spec->about.empty() ? "" : ": " + spec->about) + ")";
}

/// @brief Refuses a use of a connection's operation the connection doesn't declare: revisions use operations, never
/// add them (the section is a capability).
/// @param connections The connections after the revision.
/// @param name "connection.operation".
/// @param dot Where its dot is.
/// @throws DocumentError If it isn't declared.
void checkOperation(const ConnectionMap& connections, const std::string& name, std::size_t dot) {
	const auto connection = connections.find(name.substr(0, dot));
	if(connection == connections.end())
		throw DocumentError("revision: \"" + name + "\" uses the connection \"" + name.substr(0, dot) +
		                    "\", which the screen doesn't declare");
	if(!connection->second.operations.contains(name.substr(dot + 1)))
		throw DocumentError("revision: the connection \"" + connection->first + "\" has no operation \"" +
		                    name.substr(dot + 1) + "\"");
}

/// @brief Refuses a service step's name unless it is "service.method", written out.
/// @param name The name.
/// @param dot Where its first dot is.
/// @throws DocumentError If it isn't.
void checkUseName(const std::string& name, std::size_t dot) {
	if(name.contains("{{") || dot == std::string::npos || dot == 0 || dot + 1 == name.size())
		throw DocumentError("revision: permission denied: a service step names its service and method, written "
		                    "out (\"files.read\"), not \"" +
		                    name + "\"");
}

/// @brief Refuses a use of a service's method the screen doesn't declare, or one the host doesn't let revisions add.
/// @param declared The screen's "services" after the revision.
/// @param services The host's services, or nullptr (then the method is checked again when it runs).
/// @param name "service.method".
/// @param dot Where its dot is.
/// @throws DocumentError If it is refused.
void checkMethod(const json& declared, const services::Services* services, const std::string& name, std::size_t dot) {
	const std::string service = name.substr(0, dot), method = name.substr(dot + 1);
	if(!declared.is_object() || !declared.contains(service))
		throw DocumentError("revision: \"" + name + "\" uses the service \"" + service +
		                    "\", which the screen doesn't declare");
	if(!services) return;
	const services::MethodSpec* spec = services->method(service, method);
	if(!spec) throw DocumentError("revision: the service \"" + service + "\" has no method \"" + method + "\"");
	if(!spec->revisable)
		throw DocumentError("revision: permission denied: revisions can't add or change uses of \"" + name + "\"");
}

} // namespace

void checkServiceUses(const json& before, const json& after, const services::Services* services) {
	static const json none = json::object();
	const json& declared = after.is_object() && after.contains("services") ? after["services"] : none;
	const ConnectionMap connections = connectionsIn(after);
	for(const auto& [step, name] : addedUses(before, after) | std::views::transform(split)) {
		const std::size_t dot = name.find('.');
		checkUseName(name, dot);
		if(step == "request" || (step == "subscribe" && connections.contains(name.substr(0, dot))))
			checkOperation(connections, name, dot);
		else checkMethod(declared, services, name, dot);
	}
}

std::vector<std::string> serviceReviewLines(const json& before, const json& after, const services::Services* services) {
	std::vector<std::string> lines;
	const ConnectionMap connections = connectionsIn(after);
	for(const auto& [step, name] : addedUses(before, after) | std::views::transform(split))
		if(auto line = useLine(step, name, connections, services)) lines.push_back(*std::move(line));
	// A use already accepted is new again when the state its arguments read changes (new input to a program)
	for(const std::string& use : usesIn(after))
		if(const std::string read = changedStateRead(use, before, after); !read.empty()) {
			const auto [step, name] = split(use);
			if(auto line = useLine(step, name, connections, services))
				lines.push_back(*line + ", with " + read + " changed");
		}
	std::ranges::sort(lines);
	lines.erase(std::ranges::unique(lines).begin(), lines.end());
	return lines;
}

} // namespace affresco::format
