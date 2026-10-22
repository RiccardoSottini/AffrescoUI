/// @file
/// @brief The checks of the service steps a revision adds (ServiceRules.hpp).

#include "ServiceRules.hpp"
#include "ConnectionSpec.hpp"

#include <algorithm>
#include <iterator>
#include <ranges>
#include <utility>

namespace affresco::format {

namespace {

/// @brief Collects the "invoke", "request" and "subscribe" steps in a value, as "<step> <name>" (e.g. "request
/// api.save"); a name that isn't text counts as "{{computed}}".
/// @param value The value.
/// @param found Receives the uses.
void collectUses(const json& value, std::vector<std::string>& found) {
	if(value.is_object()) {
		for(const char* key : {"invoke", "request", "subscribe"}) {
			if(!value.contains(key)) continue;
			const json& name = value[key];
			found.push_back(std::string(key) + " " +
			                (name.is_string() ? name.get<std::string>() : std::string("{{computed}}")));
		}
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

/// @brief The uses a revision adds: those after it that weren't before, counted (a second copy is added too).
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

/// @brief Splits a use into its step and its name.
/// @param use "<step> <name>".
/// @return The step and the name.
std::pair<std::string, std::string> split(const std::string& use) {
	const std::size_t space = use.find(' ');
	return {use.substr(0, space), use.substr(space + 1)};
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

} // namespace

void checkServiceUses(const json& before, const json& after, const services::Services* services) {
	const json declared = after.is_object() && after.contains("services") ? after["services"] : json::object();
	const ConnectionMap connections = connectionsIn(after);
	for(const auto& [step, name] : addedUses(before, after) | std::views::transform(split)) {
		const std::size_t dot = name.find('.');
		if(name.contains("{{") || dot == std::string::npos || dot == 0 || dot + 1 == name.size())
			throw DocumentError("revision: permission denied: a service step names its service and method, written "
			                    "out (\"files.read\"), not \"" + name + "\"");
		const std::string service = name.substr(0, dot), method = name.substr(dot + 1);
		if(step == "request" || (step == "subscribe" && connections.contains(service))) {
			checkOperation(connections, name, dot);
			continue;
		}
		if(!declared.is_object() || !declared.contains(service))
			throw DocumentError("revision: \"" + name + "\" uses the service \"" + service +
			                    "\", which the screen doesn't declare");
		if(!services) continue; // checked again when it runs
		const services::MethodSpec* spec = services->method(service, method);
		if(!spec) throw DocumentError("revision: the service \"" + service + "\" has no method \"" + method + "\"");
		if(!spec->revisable)
			throw DocumentError("revision: permission denied: revisions can't add uses of \"" + name + "\"");
	}
}

std::vector<std::string> serviceReviewLines(const json& before, const json& after,
                                            const services::Services* services) {
	std::vector<std::string> lines;
	const ConnectionMap connections = connectionsIn(after);
	for(const auto& [step, name] : addedUses(before, after) | std::views::transform(split)) {
		const std::size_t dot = name.find('.');
		const std::string service = dot == std::string::npos ? name : name.substr(0, dot);
		const auto connection = connections.find(service);
		if(connection != connections.end() && (step == "request" || step == "subscribe")) {
			const auto operation = connection->second.operations.find(name.substr(dot + 1));
			const services::MethodSpec spec = operation->second.spec(); // checkServiceUses() found it
			if(spec.effect != services::Effect::Read)
				lines.push_back("uses " + name + " (" + std::string(services::effectName(spec.effect)) + ": " +
				                spec.about + ")");
			continue;
		}
		const services::MethodSpec* spec =
		    services && dot != std::string::npos ? services->method(service, name.substr(dot + 1)) : nullptr;
		if(!spec) lines.push_back("uses " + name + " (a method the screen can't describe yet)");
		else if(spec->effect != services::Effect::Read)
			lines.push_back("uses " + name + " (" + std::string(services::effectName(spec->effect)) +
			                (spec->about.empty() ? "" : ": " + spec->about) + ")");
	}
	lines.erase(std::ranges::unique(lines).begin(), lines.end());
	return lines;
}

} // namespace affresco::format
