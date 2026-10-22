/// @file
/// @brief Rules::behaviourLines(): what a change does to a screen's behaviour, as lines a reviewer reads. A review
/// policy may ask more approvals for such a change, and a proposal's evidence lists them (VERSION_CONTROL.md).

#include "affresco/format/NodeIds.hpp"
#include "affresco/format/Rules.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <string>
#include <string_view>

namespace affresco::format {

namespace {

/// @brief The parts of a screen that do something, each with what it holds.
using Parts = std::map<std::string, json>;

/// Most lines given; the rest are counted in a last line.
constexpr std::size_t maxLines = 40;

/// The node keys that run steps or decide what is built, besides the "on…" handlers.
constexpr std::array nodeBehaviourKeys = {"keys", "every", "check", "when", "repeat", "drag"};

/// @brief Tells whether a node key holds behaviour.
/// @param key The key.
/// @return true for "onClick" and the like, and for nodeBehaviourKeys.
bool isBehaviourKey(std::string_view key) {
	if(key.size() > 2 && key.starts_with("on") && key[2] >= 'A' && key[2] <= 'Z') return true;
	return std::ranges::find(nodeBehaviourKeys, key) != nodeBehaviourKeys.end();
}

/// @brief Collects the bindings of a value: its strings that hold "{{".
/// @param value The value.
/// @param into Gets them, in order.
void collectBindings(const json& value, json& into) {
	if(value.is_string()) {
		if(value.get_ref<const std::string&>().find("{{") != std::string::npos) into.push_back(value);
	} else if(value.is_structured())
		for(const json& item : value) collectBindings(item, into);
}

/// @brief Collects a node's behaviour: its handlers and steps, and its bindings (its children have their own).
/// @param node The node.
/// @param pointer Where it is.
/// @param parts Gets them.
void collectNode(const json& node, const std::string& pointer, Parts& parts) {
	if(!node.is_object()) return;
	const std::string name =
	    node.contains("id") && node["id"].is_string() ? "#" + node["id"].get<std::string>() : "the node at " + pointer;
	json bindings = json::array();
	for(const auto& [key, value] : node.items()) {
		if(key == "children") continue;
		if(isBehaviourKey(key)) parts[key + " of " + name] = value;
		else collectBindings(value, bindings);
	}
	if(!bindings.empty()) parts["the bindings of " + name] = std::move(bindings);
}

/// @brief Collects the behaviour of a whole screen.
/// @param document The screen.
/// @return Its parts, by the name a reviewer reads.
Parts partsOf(const json& document) {
	Parts parts;
	if(!document.is_object()) return parts;
	const std::array<std::pair<const char*, const char*>, 4> sections = {
	    {{"actions", "the action"}, {"keys", "the shortcut"}, {"watch", "the watch on"}, {"computed", "the value"}}};
	for(const auto& [section, label] : sections)
		if(const auto it = document.find(section); it != document.end() && it->is_object())
			for(const auto& [key, value] : it->items()) parts[std::string(label) + " '" + key + "'"] = value;
	if(document.contains("onLoad")) parts["onLoad"] = document["onLoad"];
	forEachNode(document, [&](const json& node, const std::string& pointer) { collectNode(node, pointer, parts); });
	return parts;
}

} // namespace

// Compared part by part, so moving a node or editing its text is not a change in behaviour
std::vector<std::string> Rules::behaviourLines(const json& before, const json& after) const {
	const Parts was = partsOf(before), is = partsOf(after);
	std::vector<std::string> lines;
	std::size_t more = 0;
	const auto line = [&](std::string text) {
		if(lines.size() < maxLines) lines.push_back(std::move(text));
		else ++more;
	};
	for(const auto& [name, value] : is) {
		const auto it = was.find(name);
		if(it == was.end()) line("adds " + name);
		else if(it->second != value) line("changes " + name);
	}
	for(const auto& [name, value] : was)
		if(!is.contains(name)) line("removes " + name);
	if(more) lines.push_back("and " + std::to_string(more) + " more");
	return lines;
}

} // namespace affresco::format
