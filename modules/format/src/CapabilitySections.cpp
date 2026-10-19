/// @file
/// @brief The sections that grant capabilities, which revisions can never change: "channels", "persist" and
/// "permissions".

#include "SectionRegistry.hpp"

#include "affresco/core/DocumentRules.hpp"

#include <format>

namespace affresco::format {

namespace {

/// @brief "channels": each a socket, and whether to reconnect.
/// @param value The value.
/// @param name "channels".
void checkChannels(const json& value, const std::string& name) {
	for(const auto& [channel, definition] : expect::object(value, name).items()) {
		const std::string at = std::format("{}.{}", name, channel);
		expect::object(definition, at);
		if(!definition.contains("socket")) expect::fail(at, "a channel needs a \"socket\"");
		for(const auto& [key, item] : definition.items())
			if(key != "socket" && key != "reconnect" && key != "note")
				expect::fail(std::format("{}.{}", at, key), "unknown property (expected socket or reconnect)");
	}
}

/// @brief "persist": a list of paths under state.
/// @param value The value.
/// @param name "persist".
void checkPersist(const json& value, const std::string& name) {
	if(!value.is_array()) expect::fail(name, "expected a list of state paths, e.g. [\"state.habits\"]");
	for(std::size_t i = 0; i < value.size(); ++i) {
		const std::string at = name + "[" + std::to_string(i) + "]";
		if(!expect::string(value[i], at).starts_with("state."))
			expect::fail(at, "expected a path under state, e.g. \"state.habits\"");
	}
}

/// @brief "permissions": allow, ask and deny rules, and the reviewers.
/// @param value The value.
/// @param name "permissions".
void checkPermissions(const json& value, const std::string& name) {
	for(const auto& [key, list] : expect::object(value, name).items()) {
		const std::string at = std::format("{}.{}", name, key);
		if(key != "allow" && key != "ask" && key != "deny" && key != "reviewers")
			expect::fail(at, "unknown property (expected allow, ask, deny or reviewers)");
		if(!list.is_array())
			expect::fail(at, key == "reviewers" ? "expected a list of author names" : "expected a list of rules");
		for(std::size_t i = 0; i < list.size(); ++i) {
			const std::string item = expect::string(list[i], at + "[" + std::to_string(i) + "]");
			if(key != "reviewers" && !parsePermissionRule(item))
				expect::fail(at + "[" + std::to_string(i) + "]",
				             "expected \"author\" or \"author(pattern)\", the pattern a JSON Pointer or \"#id\" path, "
				             "e.g. \"agent(#board)\"");
		}
	}
}

} // namespace

SectionType channelsSection() { return {.name = "channels", .validate = checkChannels, .capability = true}; }

SectionType persistSection() { return {.name = "persist", .validate = checkPersist, .capability = true}; }

SectionType permissionsSection() { return {.name = "permissions", .validate = checkPermissions, .capability = true}; }

} // namespace affresco::format
