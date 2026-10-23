/// @file
/// @brief Contributions: commands, keybindings, menus and the `context` variable.

#include "Contributions.hpp"
#include "Engine.hpp"
#include "Shortcuts.hpp"
#include "KeyNotation.hpp"
#include "NodeKeys.hpp"

#include "affresco/render/TextEditor.hpp"

#include <format>
#include <algorithm>
#include <array>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>
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
#ifdef __APPLE__
	return "mac";
#elifdef _WIN32
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
	for(const Panel* p : chain) {
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
/// @param notation How the keys are labelled: "window.keyLabels".
void addBindings(json& list, const json& keys, const std::string& scope, const json& actions,
                 std::string_view notation) {
	if(!keys.is_object()) return;
	for(const auto& [written, handler] : keys.items()) {
		const std::string key = normalizeChord(written);
		const json bindings = isBindingList(handler) ? handler : json::array({handler});
		for(const json& binding : bindings) {
			const json& run = isBinding(binding) ? binding["do"] : binding;
			json entry = {{"key", key},       {"keyLabel", keyLabel(key, notation)},
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
	const json window = document.value("window", json::object());
	const std::string notation =
	    window.is_object() && window.contains("keyLabels") && window["keyLabels"].is_string() ? window["keyLabels"]
	                                                                                           : "platform";
	json list = json::array();
	addBindings(list, document.value("keys", json()), "screen", actions, notation);
	json nodes = json::array();
	for(const auto& [panel, record] : engine.reconciler.records())
		if(!panel->id().empty()) addBindings(nodes, record.keys, "#" + panel->id(), actions, notation);
	// NOLINTNEXTLINE(modernize-use-ranges): json's iterators don't model the ranges concepts
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
	// NOLINTNEXTLINE(modernize-use-ranges): json's iterators don't model the ranges concepts
	std::stable_sort(list.begin(), list.end(), [](const json& a, const json& b) {
		return std::tie(a["category"].get_ref<const std::string&>(), a["title"].get_ref<const std::string&>()) <
		       std::tie(b["category"].get_ref<const std::string&>(), b["title"].get_ref<const std::string&>());
	});
	return list;
}

/// Menus open submenus at most this deep, so a menu that contains itself ends.
constexpr int maxMenuDepth = 8;
/// The menus resolve to at most this many items in all: items that open their own menu again would otherwise multiply
/// at every level.
constexpr std::size_t maxMenuItems = 10000;

/// @brief Tells whether a menu item's "when" holds.
/// @param engine The runtime.
/// @param when The expression.
/// @param where Its place.
/// @return Its truth; false (reported) if it fails.
bool shown(Engine& engine, const json& when, const std::string& where) {
	try {
		return truthy(engine.document.evaluate(when, *noVariables(), json::object(), where));
	} catch(const DocumentError& error) {
		engine.report(error.what());
		return false;
	}
}

/// How many more menu items may be made, submenus' included.
struct MenuBudget {
	std::size_t left = maxMenuItems; ///< Items still allowed.
	bool full = false;               ///< Whether an item was left out (reported once).
};

/// @brief Takes one item from a budget.
/// @param engine The runtime, which hears once that the budget ran out.
/// @param budget The budget.
/// @param where What is being made, for the report.
/// @return false if the budget is spent: the item and the rest are left out.
bool take(Engine& engine, MenuBudget& budget, std::string_view where) {
	if(budget.left == 0) {
		if(!std::exchange(budget.full, true))
			engine.report(std::string(where) + ": more than " + std::to_string(maxMenuItems) +
			              " items in all; the rest are left out");
		return false;
	}
	--budget.left;
	return true;
}

/// What the menus resolve from, and how many items they may still resolve to.
struct MenuSources {
	const json& menus;                                     ///< The "menus" section.
	std::unordered_map<std::string, const json*> commands; ///< The `commands` variable's commands, by name.
	MenuBudget budget;                                     ///< Items still allowed.
};

/// @brief Gives a menu item what its command says: its title and icon unless the item has its own, whether it is
/// enabled, and its key.
/// @param entry The resolved item.
/// @param item The item as written.
/// @param commands The commands, by name.
void describeCommand(json& entry, const json& item, const std::unordered_map<std::string, const json*>& commands) {
	const auto run = item.find("run");
	if(run == item.end() || !run->is_string()) return;
	const auto command = commands.find(run->get<std::string>());
	if(command == commands.end()) return;
	const json& c = *command->second;
	if(!item.contains("title")) entry["title"] = c["title"];
	if(!item.contains("icon")) entry["icon"] = c["icon"];
	entry["enabled"] = c["enabled"];
	entry["key"] = c["key"];
	entry["keyLabel"] = c["keyLabel"];
}

/// @brief A menu's item as the `menus` variable shows it, before its submenu is resolved.
/// @param key The item's name.
/// @param item Its declaration.
/// @param commands The `commands` variable's commands, by name.
/// @return `{ name, title, icon, enabled, key, keyLabel, group, separator, order, run, with, submenu }`.
json menuEntry(const std::string& key, const json& item, const std::unordered_map<std::string, const json*>& commands) {
	json entry = {{"name", key},
	              {"title", item.value("title", key)},
	              {"icon", item.value("icon", json())},
	              {"enabled", true},
	              {"key", nullptr},
	              {"keyLabel", nullptr},
	              {"group", item.value("group", "")},
	              {"separator", false},
	              {"order", item.value("order", 0.0)}};
	if(item.contains("run")) {
		entry["run"] = item["run"];
		if(item.contains("with")) entry["with"] = item["with"];
	}
	describeCommand(entry, item, commands);
	if(const auto submenu = item.find("submenu"); submenu != item.end() && submenu->is_string())
		entry["submenu"] = *submenu;
	return entry;
}

/// @brief Orders a menu's items by group, order and name, and puts a separator between groups.
/// @param kept The items, each with its "order", which it drops.
/// @return The menu.
json grouped(json kept) {
	// NOLINTNEXTLINE(modernize-use-ranges): json's iterators don't model the ranges concepts
	std::stable_sort(kept.begin(), kept.end(), [](const json& a, const json& b) {
		return std::tuple(a["group"].get_ref<const std::string&>(), a["order"].get<double>(),
		                  a["name"].get_ref<const std::string&>()) <
		       std::tuple(b["group"].get_ref<const std::string&>(), b["order"].get<double>(),
		                  b["name"].get_ref<const std::string&>());
	});
	json items = json::array();
	for(json& entry : kept) {
		if(!items.empty() && items.back()["group"] != entry["group"])
			items.push_back({{"separator", true}, {"group", entry["group"]}});
		entry.erase("order");
		items.push_back(std::move(entry));
	}
	return items;
}

/// @brief One menu of the `menus` variable: its items that pass their "when", by group, order and key, with a
/// separator between groups, and what their commands say.
/// @param engine The runtime.
/// @param sources The "menus" section, the commands, and the items still allowed.
/// @param name The menu.
/// @param depth How deep in submenus.
/// @return The items: `{ name, title, icon, enabled, key, keyLabel, group, separator, submenu, run, with }`.
json menuOf(Engine& engine, MenuSources& sources, const std::string& name, int depth) {
	const json& menus = sources.menus;
	if(depth > maxMenuDepth || !menus.contains(name) || !menus[name].is_object()) return json::array();
	json kept = json::array();
	for(const auto& [key, item] : menus[name].items()) {
		if(!item.is_object()) continue;
		if(item.contains("when") && !shown(engine, item["when"], std::format("menus.{}.{}.when", name, key))) continue;
		if(!take(engine, sources.budget, "menus")) break;
		kept.push_back(menuEntry(key, item, sources.commands));
	}
	// Submenus once this menu's own items are counted, so the budget keeps the items nearest the top
	for(json& entry : kept)
		if(const auto submenu = entry.find("submenu"); submenu != entry.end())
			*submenu = menuOf(engine, sources, submenu->get<std::string>(), depth + 1);
	return grouped(std::move(kept));
}

/// @brief The `menus` variable: each menu of "menus", resolved (menuOf()).
/// @param engine The runtime.
/// @return `{ "<menu>": [items] }`.
json menusOf(Engine& engine) {
	const json& document = engine.versions.document();
	json resolved = json::object();
	if(!document.contains("menus") || !document["menus"].is_object()) return resolved;
	MenuSources sources{document["menus"], {}, {}};
	if(const json* commands = engine.document.variable("commands"))
		for(const json& command : *commands)
			if(const auto name = command.find("name"); name != command.end() && name->is_string())
				sources.commands.try_emplace(name->get<std::string>(), &command); // the first of a name, as before
	for(const auto& [name, menu] : document["menus"].items()) resolved[name] = menuOf(engine, sources, name, 0);
	return resolved;
}

/// @brief A native menu item from a resolved one (menuOf()); choosing it runs its action, like the drawn menus do.
/// @param engine The runtime.
/// @param entry The resolved item.
/// @param budget Native items still allowed in the bar.
/// @return The item.
MenuItem nativeItem(Engine& engine, const json& entry, MenuBudget& budget) {
	MenuItem item;
	item.separator = entry.value("separator", false);
	if(item.separator) return item;
	if(const auto title = entry.find("title"); title != entry.end() && title->is_string()) item.title = *title;
	if(const auto label = entry.find("keyLabel"); label != entry.end() && label->is_string()) item.keyLabel = *label;
	item.enabled = entry.value("enabled", true);
	if(const auto submenu = entry.find("submenu"); submenu != entry.end() && submenu->is_array())
		for(const json& child : *submenu) {
			if(!take(engine, budget, "window.menuBar")) break;
			item.submenu.push_back(nativeItem(engine, child, budget));
		}
	if(const auto run = entry.find("run"); run != entry.end()) {
		json step = {{"run", *run}};
		if(const auto with = entry.find("with"); with != entry.end() && !with->is_null()) step["with"] = *with;
		item.run = [&engine, step = std::move(step)] {
			if(engine.window)
				engine.runner.handle(json::array({step}), engine.window->root(), {{"type", "menu"}}, *noVariables(),
				                     "window.menuBar");
		};
	}
	return item;
}

} // namespace

void showMenuBar(Engine& engine) {
	Window* window = engine.window;
	const json& document = engine.versions.document();
	if(!window) return;
	try {
		json bar = json::array();
		if(document.contains("window") && document["window"].is_object() && document["window"].contains("menuBar")) {
			const json& menuBar = document["window"]["menuBar"];
			if(!menuBar.is_array()) expect::fail("window.menuBar", "expected a list of menus");
			const json* menus = engine.document.variable("menus");
			for(std::size_t i = 0; i < menuBar.size(); ++i) {
				const std::string at = "window.menuBar[" + std::to_string(i) + "]";
				const json& top = expect::object(menuBar[i], at);
				const std::string& name = expect::string(top.contains("menu") ? top["menu"] : json(), at + ".menu");
				if(!menus || !menus->contains(name)) expect::fail(at + ".menu", "there is no menu '" + name + "' in \"menus\"");
				bar.push_back({{"title", top.contains("title") ? expect::string(top["title"], at + ".title") : name},
				               {"submenu", (*menus)[name]}});
			}
		}
		if(bar == engine.menuBar) return;
		// The bar may list a menu many times; Win32 numbers its items with 16 bits, so the bar keeps to one budget
		std::vector<MenuItem> items;
		MenuBudget budget;
		for(const json& entry : bar) {
			if(!take(engine, budget, "window.menuBar")) break;
			items.push_back(nativeItem(engine, entry, budget));
		}
		engine.menuBar = std::move(bar);
		window->setMenuBar(std::move(items));
	} catch(const DocumentError& error) {
		engine.report(error.what());
	}
}

void hideMenuBar(Engine& engine) {
	if(!engine.window || engine.menuBar.empty()) return;
	engine.menuBar = json::array();
	engine.window->setMenuBar({});
}

void checkMenus(const json& section, const std::string& name) {
	static constexpr std::array keys = {"run", "with", "submenu", "group", "order", "when", "title", "icon"};
	for(const auto& [menu, items] : expect::object(section, name).items())
		for(const auto& [key, item] : expect::object(items, concatenate(name, ".", menu)).items()) {
			const std::string at = std::format("{}.{}.{}", name, menu, key);
			for(const auto& [property, value] : expect::object(item, at).items())
				if(std::ranges::find(keys, property) == keys.end())
					expect::fail(at, "unknown key '" + property + "' (a menu item has run, with, submenu, group, order, "
					                 "when, title, icon)");
			if(!item.contains("run") && !item.contains("submenu"))
				expect::fail(at, R"(a menu item needs "run" or "submenu")");
			if(item.contains("submenu") && !section.contains(expect::string(item["submenu"], at + ".submenu")))
				expect::fail(at + ".submenu", "there is no menu '" + item["submenu"].get<std::string>() + "' in \"menus\"");
			if(item.contains("order") && !item["order"].is_number()) expect::fail(at + ".order", "expected a number");
			if(item.contains("group")) (void)expect::string(item["group"], at + ".group");
		}
}

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
	engine.document.addVariable("menus", [&engine] { return menusOf(engine); });
}

} // namespace affresco::format
