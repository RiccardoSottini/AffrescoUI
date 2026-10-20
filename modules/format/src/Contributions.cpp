/// @file
/// @brief Contributions: commands, keybindings and the `context` variable.

#include "Contributions.hpp"
#include "Engine.hpp"
#include "Shortcuts.hpp"

#include "affresco/render/TextEditor.hpp"

#include <algorithm>
#include <tuple>
#include <vector>

namespace affresco::format {
namespace {

/// @brief Tells whether a key belongs to commands.
/// @param key The key.
/// @return true for "title", "do", ...
bool commandKey(std::string_view key) { return std::ranges::find(commandKeys, key) != commandKeys.end(); }

/// @brief The platform, as `context.platform` names it.
/// @return "mac", "windows" or "linux".
std::string_view platform() {
#if defined(__APPLE__)
	return "mac";
#elif defined(_WIN32)
	return "windows";
#else
	return "linux";
#endif
}

/// @brief The `context` variable: where the focus is, a chord in progress, and the nodes' own "context" keys.
/// @param engine The runtime.
/// @return The variable.
json contextOf(Engine& engine) {
	Panel* focused = engine.window ? engine.window->focusedPanel() : nullptr;
	json context = json::object();
	json path = json::array();
	std::vector<Panel*> chain; // the focused panel's ancestors, the root first
	for(Panel* p = focused; p; p = p->parent()) chain.insert(chain.begin(), p);
	for(Panel* p : chain) {
		if(!p->id().empty()) path.push_back(p->id());
		const Record* record = engine.reconciler.recordOf(p);
		if(!record || !record->context.is_object()) continue;
		try { // an inner node's keys win
			context.update(engine.document.evaluate(record->context, *record->locals, json::object(),
			                                        record->path + ".context"));
		} catch(const DocumentError& error) {
			engine.report(error.what());
		}
	}
	bool overlay = false;
	for(const auto& [panel, record] : engine.reconciler.records())
		overlay = overlay || (record.overlayOpen && panel->isShown());
	context["focus"] = focused && !focused->id().empty() ? json(focused->id()) : json();
	context["focusPath"] = std::move(path);
	context["textInputFocus"] = dynamic_cast<const TextArea*>(focused) || dynamic_cast<const TextEditor*>(focused);
	context["overlay"] = overlay;
	context["chord"] = engine.events.chord().empty() ? json() : json(engine.events.chord());
	context["platform"] = platform();
	return context;
}

/// @brief Adds the bindings of one scope to the `keybindings` list.
/// @param list The list.
/// @param keys The scope's "keys".
/// @param scope "screen" or "#id".
/// @param actions The document's "actions".
void addBindings(json& list, const json& keys, const std::string& scope, const json& actions) {
	if(!keys.is_object()) return;
	for(const auto& [written, handler] : keys.items()) {
		const std::string key = normalizeChord(written);
		const json bindings = isBindingList(handler) ? handler : json::array({handler});
		for(const json& binding : bindings) {
			const json& run = isBinding(binding) ? binding["do"] : binding;
			json entry = {{"key", key},       {"keyLabel", keyLabel(key)},
			              {"command", nullptr}, {"title", nullptr},
			              {"when", isBinding(binding) ? binding.value("when", json()) : json()},
			              {"scope", scope}};
			if(run.is_string()) {
				const std::string name = run.get<std::string>();
				entry["command"] = name;
				const json action = actions.is_object() ? actions.value(name, json()) : json();
				entry["title"] = isCommand(action) ? action.value("title", name) : name;
			}
			list.push_back(std::move(entry));
		}
	}
}

/// @brief The `keybindings` variable: the document's "keys" and those of every node with an id.
/// @param engine The runtime.
/// @return The list, the screen's first, then by scope and key.
json keybindingsOf(Engine& engine) {
	const json& document = engine.versions.document();
	const json actions = document.value("actions", json::object());
	json list = json::array();
	addBindings(list, document.value("keys", json()), "screen", actions);
	json nodes = json::array();
	for(const auto& [panel, record] : engine.reconciler.records())
		if(!panel->id().empty()) addBindings(nodes, record.keys, "#" + panel->id(), actions);
	std::sort(nodes.begin(), nodes.end(), [](const json& a, const json& b) {
		return std::tie(a["scope"].get_ref<const std::string&>(), a["key"].get_ref<const std::string&>()) <
		       std::tie(b["scope"].get_ref<const std::string&>(), b["key"].get_ref<const std::string&>());
	});
	for(json& entry : nodes) list.push_back(std::move(entry));
	return list;
}

/// @brief The `commands` variable: every command of "actions", with its first key.
/// @param engine The runtime.
/// @return The list, sorted by category, then title.
json commandsOf(Engine& engine) {
	const json& document = engine.versions.document();
	json list = json::array();
	if(!document.contains("actions") || !document["actions"].is_object()) return list;
	const json* bindings = engine.document.variable("keybindings");
	for(const auto& [name, action] : document["actions"].items()) {
		if(!isCommand(action)) continue;
		json command = {{"name", name},
		                {"title", action.value("title", name)},
		                {"category", action.value("category", "")},
		                {"icon", action.value("icon", json())},
		                {"description", action.value("description", "")},
		                {"enabled", commandEnabled(engine, name)},
		                {"key", nullptr},
		                {"keyLabel", nullptr}};
		if(bindings)
			for(const json& binding : *bindings)
				if(binding["command"] == name) {
					command["key"] = binding["key"];
					command["keyLabel"] = binding["keyLabel"];
					break;
				}
		list.push_back(std::move(command));
	}
	std::stable_sort(list.begin(), list.end(), [](const json& a, const json& b) {
		return std::tie(a["category"].get_ref<const std::string&>(), a["title"].get_ref<const std::string&>()) <
		       std::tie(b["category"].get_ref<const std::string&>(), b["title"].get_ref<const std::string&>());
	});
	return list;
}

} // namespace

bool isCommand(const json& action) {
	if(!action.is_object() || action.empty()) return false;
	bool own = false; // a key only commands have
	for(const auto& [key, value] : action.items()) {
		if(!commandKey(key)) return false;
		own = own || key != "do";
	}
	return own || action.contains("do");
}

void checkCommand(const json& action, const std::string& where) {
	if(!action.is_object()) return;
	const bool command = std::ranges::any_of(action.items(), [](const auto& item) {
		return commandKey(item.key()) && item.key() != "do";
	});
	const bool steps = std::ranges::any_of(action.items(), [](const auto& item) { return !commandKey(item.key()); });
	if(command && steps)
		fail(where, "an action is a command (title, category, icon, enablement, description, do) or steps, not both");
}

bool isBinding(const json& binding) {
	if(!binding.is_object() || !binding.contains("do")) return false;
	return std::ranges::all_of(binding.items(), [](const auto& item) {
		return item.key() == "do" || item.key() == "when" || item.key() == "with";
	});
}

bool isBindingList(const json& handler) {
	return handler.is_array() && !handler.empty() && std::ranges::all_of(handler, isBinding);
}

bool commandEnabled(Engine& engine, std::string_view name) {
	const json& document = engine.versions.document();
	if(!document.contains("actions") || !document["actions"].is_object()) return true;
	const auto action = document["actions"].find(std::string(name));
	if(action == document["actions"].end() || !isCommand(*action) || !action->contains("enablement")) return true;
	try {
		const std::string where = "actions." + std::string(name) + ".enablement";
		return truthy(engine.document.evaluate((*action)["enablement"], *noVariables(), json::object(), where));
	} catch(const DocumentError& error) {
		engine.report(error.what());
		return false;
	}
}

void addContributionVariables(Engine& engine) {
	engine.document.addVariable("context", [&engine] { return contextOf(engine); });
	engine.document.addVariable("keybindings", [&engine] { return keybindingsOf(engine); });
	engine.document.addVariable("commands", [&engine] { return commandsOf(engine); });
}

} // namespace affresco::format
