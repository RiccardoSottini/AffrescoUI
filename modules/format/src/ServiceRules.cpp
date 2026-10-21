/// @file
/// @brief The checks of the service steps a revision adds (ServiceRules.hpp).

#include "ServiceRules.hpp"


#include <algorithm>
#include <iterator>

namespace affresco::format {

namespace {

/// @brief Collects the names of the "invoke" and "subscribe" steps in a value; a name that isn't text counts as
/// "{{computed}}".
/// @param value The value.
/// @param found Receives the names.
void collectUses(const json& value, std::vector<std::string>& found) {
	if(value.is_object()) {
		for(const char* key : {"invoke", "subscribe"}) {
			if(!value.contains(key)) continue;
			const json& name = value[key];
			found.push_back(name.is_string() ? name.get<std::string>() : std::string("{{computed}}"));
		}
		for(const auto& [key, item] : value.items()) collectUses(item, found);
	} else if(value.is_array()) {
		for(const json& item : value) collectUses(item, found);
	}
}

/// @brief The uses in a document, sorted: everywhere but its state and its services, which hold values.
/// @param document The document.
/// @return The names.
std::vector<std::string> usesIn(const json& document) {
	std::vector<std::string> found;
	if(!document.is_object()) return found;
	for(const auto& [key, value] : document.items())
		if(key != "state" && key != "services") collectUses(value, found);
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

} // namespace

void checkServiceUses(const json& before, const json& after, const services::Services* services) {
	const json declared = after.is_object() && after.contains("services") ? after["services"] : json::object();
	for(const std::string& name : addedUses(before, after)) {
		const std::size_t dot = name.find('.');
		if(name.contains("{{") || dot == std::string::npos || dot == 0 || dot + 1 == name.size())
			throw DocumentError("revision: permission denied: a service step names its service and method, written "
			                    "out (\"files.read\"), not \"" + name + "\"");
		const std::string service = name.substr(0, dot), method = name.substr(dot + 1);
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
	for(const std::string& name : addedUses(before, after)) {
		const std::size_t dot = name.find('.');
		const services::MethodSpec* spec =
		    services && dot != std::string::npos ? services->method(name.substr(0, dot), name.substr(dot + 1)) : nullptr;
		if(!spec) lines.push_back("uses " + name + " (a method the screen can't describe yet)");
		else if(spec->effect != services::Effect::Read)
			lines.push_back("uses " + name + " (" + std::string(services::effectName(spec->effect)) +
			                (spec->about.empty() ? "" : ": " + spec->about) + ")");
	}
	lines.erase(std::ranges::unique(lines).begin(), lines.end());
	return lines;
}

} // namespace affresco::format
