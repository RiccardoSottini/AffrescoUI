/// @file
/// @brief Settings: the "settings" section, its checked writes, and the `settings` and `settingsList` variables.

#include "Settings.hpp"
#include "Contributions.hpp"
#include "Engine.hpp"
#include "StateTypes.hpp"

#include <algorithm>
#include <format>
#include <string_view>
#include <unordered_map>

namespace affresco::format {

namespace {

/// @brief The value a setting has: the one in `state.settings`, else its default.
/// @param state The state.
/// @param key The setting.
/// @param type Its type.
/// @return The value.
const json& settingValue(const json& state, const std::string& key, const json& type) {
	if(const auto values = state.find("settings"); values != state.end() && values->is_object())
		if(const auto value = values->find(key); value != values->end()) return *value;
	return type["default"];
}

/// @brief The `settings` variable: each setting's value, nested by the dots of its key.
/// @param engine The runtime.
/// @return The object, e.g. `{ "editor": { "fontSize": 14 } }`.
json settingsOf(Engine& engine) {
	const json& document = engine.versions.document();
	json nested = json::object();
	if(!document.contains("settings") || !document["settings"].is_object()) return nested;
	for(const auto& [key, type] : document["settings"].items()) {
		json* at = &nested;
		std::string_view rest = key;
		for(auto dot = rest.find('.'); dot != std::string_view::npos; dot = rest.find('.')) {
			json& group = (*at)[std::string(rest.substr(0, dot))];
			if(!group.is_object()) group = json::object(); // checkSettings() keeps groups and settings apart
			at = &group;
			rest.remove_prefix(dot + 1);
		}
		(*at)[std::string(rest)] = settingValue(engine.versions.state(), key, type);
	}
	return nested;
}

/// @brief The `settingsList` variable: one entry per setting, for a settings page.
/// @param engine The runtime.
/// @return `{ key, title, description, type, value, default, modified, enum, minimum, maximum }` per setting, by key.
json settingsListOf(Engine& engine) {
	const json& document = engine.versions.document();
	json list = json::array();
	if(!document.contains("settings") || !document["settings"].is_object()) return list;
	for(const auto& [key, type] : document["settings"].items()) {
		const json& value = settingValue(engine.versions.state(), key, type);
		const json& kind = type.value("type", json());
		list.push_back(
		    {{"key", key},
			 {"title", declaredText(engine, type, "title", key, "settings." + key + ".title")},
			 {"description", declaredText(engine, type, "description", "", "settings." + key + ".description")},
			 {"type", kind.is_array() && !kind.empty() ? kind[0] : kind},
			 {"value", value},
			 {"default", type["default"]},
			 {"modified", value != type["default"]},
			 {"enum", type.value("enum", json())},
			 {"minimum", type.value("minimum", json())},
			 {"maximum", type.value("maximum", json())}});
	}
	// NOLINTNEXTLINE(modernize-use-ranges): a json array isn't a sortable range
	std::sort(list.begin(), list.end(), [](const json& a, const json& b) { return a["key"] < b["key"]; });
	return list;
}

} // namespace

void checkSettings(const json& section, const std::string& name) {
	const json& settings = expect::object(section, name);
	// Each group a key is in ("editor" for "editor.fontSize"), with the first key in it: one pass, not one per setting
	std::unordered_map<std::string_view, std::string_view> firstInGroup;
	for(const auto& [key, unused] : settings.items())
		for(auto dot = key.find('.'); dot != std::string::npos; dot = key.find('.', dot + 1))
			firstInGroup.try_emplace(std::string_view(key).substr(0, dot), key);
	for(const auto& [key, type] : settings.items()) {
		const std::string at = std::format("{}.{}", name, key);
		if(key.empty() || key.front() == '.' || key.back() == '.' || key.contains(".."))
			expect::fail(at, "a setting's key is names joined by dots, e.g. \"editor.fontSize\"");
		checkType(type, at);
		if(!type.contains("default")) expect::fail(at, "a setting needs a \"default\"");
		if(const auto problem = typeMismatch(type["default"], type, at + ".default"))
			expect::fail(problem->place, problem->problem);
		if(const auto inside = firstInGroup.find(key); inside != firstInGroup.end())
			expect::fail(at,
			             "'" + key + "' is a setting and a group of settings ('" + std::string(inside->second) + "')");
	}
}

std::optional<std::string> settingKey(std::string_view path) {
	constexpr std::string_view prefix = "settings.";
	if(!path.starts_with(prefix) || path.size() == prefix.size()) return std::nullopt;
	return std::string(path.substr(prefix.size()));
}

std::optional<json::json_pointer> settingPointer(const json& document, std::string_view path,
                                                 const std::string& where) {
	const auto key = settingKey(path);
	if(!key || !document.contains("settings")) return std::nullopt;
	checkSettingValue(document, *key, nullptr, where);
	return json::json_pointer("/settings") / *key;
}

void checkSettingValue(const json& document, const std::string& key, const json* value, const std::string& where) {
	const json* settings = document.contains("settings") ? &document["settings"] : nullptr;
	if(!settings || !settings->is_object() || !settings->contains(key))
		expect::fail(where, "there is no setting '" + key + "' in \"settings\"");
	if(!value) return; // removed: back to its default
	if(const auto problem = typeMismatch(*value, (*settings)[key], "settings." + key))
		expect::fail(where, problem->text());
}

void addSettingsVariables(Engine& engine) {
	engine.document.addVariable("settings", [&engine] { return settingsOf(engine); });
	engine.document.addVariable("settingsList", [&engine] { return settingsListOf(engine); });
}

} // namespace affresco::format
