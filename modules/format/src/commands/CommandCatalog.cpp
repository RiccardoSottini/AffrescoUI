/// @file
/// @brief format::CommandCatalog: main's commands as tools.

#include "CommandCatalog.hpp"
#include "CommandArguments.hpp"
#include "CommandReach.hpp"
#include "Engine.hpp"
#include "Picks.hpp"

#include <format>

namespace affresco::format {

namespace {

/// @brief One entry of the catalog, from one of the `commands` variable.
/// @param command The variable's entry.
/// @param action The command's value in "actions", or nullptr.
/// @return `{name, title, category, description, arguments, enabled, key?}`.
json entryOf(const json& command, const json* action) {
	const auto text = [&](const char* key, std::size_t most) {
		const auto value = command.find(key);
		return value != command.end() && value->is_string() ? cutToCharacter(value->get<std::string>(), most)
		                                                    : std::string();
	};
	json entry = {{"name", command.value("name", std::string())},
	              {"title", text("title", CommandCatalog::maxTitleBytes)},
	              {"category", text("category", CommandCatalog::maxTitleBytes)},
	              {"description", text("description", CommandCatalog::maxDescriptionBytes)},
	              {"arguments", argumentTypes(action ? action->value("arguments", json()) : json())},
	              {"enabled", command.value("enabled", true)}};
	if(const auto key = command.find("key"); key != command.end() && key->is_string()) entry["key"] = *key;
	return entry;
}

} // namespace

json CommandCatalog::list() const {
	json catalog = json::array();
	const json* commands = engine_.document.variable("commands");
	if(!commands || !commands->is_array()) return catalog;
	const json& document = engine_.versions.document();
	const json* actions = document.contains("actions") ? &document["actions"] : nullptr;
	for(const json& command : *commands) {
		if(catalog.size() == maxCommands) break;
		const std::string name = command.value("name", std::string());
		catalog.push_back(entryOf(command, actions && actions->contains(name) ? &(*actions)[name] : nullptr));
	}
	return catalog;
}

bool CommandCatalog::lists(std::string_view name) const {
	const json* commands = engine_.document.variable("commands");
	if(!commands || !commands->is_array()) return false;
	std::size_t seen = 0;
	for(const json& command : *commands) {
		if(seen++ == maxCommands) break; // as list() does: a command past the limit isn't listed
		if(command.value("name", std::string()) == name) return true;
	}
	return false;
}

std::optional<json> CommandCatalog::find(std::string_view name) const {
	for(json& entry : list())
		if(entry.value("name", std::string()) == name) return std::move(entry);
	return std::nullopt;
}

CommandReach CommandCatalog::reach(std::string_view name) const {
	if(!lists(name)) return CommandReach{true, "no command '" + cutToCharacter(std::string(name), 80) + "'", {}, false};
	const services::Services* services = engine_.serviceCalls.services();
	const std::optional<int> preview = engine_.versions.previewing();
	const std::string key = std::format("{} {} {}", engine_.versions.head().id, preview ? *preview : -1,
	                                    static_cast<const void*>(services));
	if(key != reachKey_) {
		reaches_.clear();
		reachKey_ = key;
	}
	if(const auto known = reaches_.find(name); known != reaches_.end()) return known->second;
	CommandReach found = reachOf(engine_.versions.document(), std::string(name), services);
	reaches_.emplace(std::string(name), found);
	return found;
}

std::optional<std::string> CommandCatalog::checkArguments(std::string_view name, json& arguments) const {
	const std::optional<json> command = find(name);
	if(!command) return "no command '" + cutToCharacter(std::string(name), 80) + "'";
	return checkCommandArguments(*command, arguments);
}

} // namespace affresco::format
