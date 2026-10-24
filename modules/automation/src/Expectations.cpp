/// @file
/// @brief The expectations of state values and nodes: what `expect` reads, and how it says what differs.

#include "Internal.hpp"
#include "Player.hpp"

#include <affresco/Screen.hpp>
#include <affresco/render/Accessibility.hpp>
#include <affresco/render/Controls.hpp>
#include <affresco/render/Label.hpp>
#include <affresco/render/TextEditor.hpp>
#include <affresco/render/Window.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <optional>
#include <utility>

namespace affresco::automation::detail {

namespace {

/// @brief A value as a problem shows it: JSON, cut short.
/// @param value The value.
/// @return At most 80 characters of it.
std::string shown(const json& value) {
	std::string text = value.dump();
	if(text.size() > 80) text = text.substr(0, 77) + "...";
	return text;
}

/// @brief Reads a path segment as an index into a list.
/// @param segment The segment.
/// @return The index, or std::nullopt unless it is one to nine digits.
std::optional<std::size_t> indexOf(const std::string& segment) {
	if(segment.empty() || segment.size() >= 10 ||
	   !std::ranges::all_of(segment, [](char c) { return c >= '0' && c <= '9'; }))
		return std::nullopt;
	return std::stoul(segment);
}

/// @brief Follows a path into a value.
/// @param value Where to start.
/// @param segments The path's segments.
/// @return The value there, or std::nullopt if there is none.
std::optional<json> follow(const json& value, const std::vector<std::string>& segments) {
	const json* at = &value;
	for(std::size_t i = 0; i < segments.size(); ++i) {
		const std::string& segment = segments[i];
		const std::optional<std::size_t> index = at->is_array() ? indexOf(segment) : std::nullopt;
		if(at->is_object() && at->contains(segment)) {
			at = &(*at)[segment];
		} else if(index) {
			if(*index >= at->size()) return std::nullopt;
			at = &(*at)[*index];
		} else if(segment == "length" && i + 1 == segments.size() && (at->is_array() || at->is_string())) {
			return json(at->is_array() ? at->size() : at->get<std::string>().size());
		} else {
			return std::nullopt;
		}
	}
	return *at;
}

/// @brief Finds the n-th panel with an id in tree order.
/// @param panel Where to search.
/// @param id The id.
/// @param n How many matches to skip; counts down.
/// @param shownOnly Whether hidden panels are skipped.
/// @return The panel, or nullptr.
Panel* findById(Panel& panel, std::string_view id, long long& n, bool shownOnly) {
	if((!shownOnly || panel.isShown()) && panel.id() == id && n-- == 0) return &panel;
	for(const auto& child : panel.children())
		if(Panel* found = findById(*child, id, n, shownOnly)) return found;
	return nullptr;
}

/// @brief A node's text: a label's, a text field's or an editor's.
/// @param panel The node.
/// @return The text, or std::nullopt if it has none.
std::optional<std::string> textOf(const Panel& panel) {
	if(const auto* editor = dynamic_cast<const TextEditor*>(&panel)) return editor->text();
	if(const auto* label = dynamic_cast<const Label*>(&panel)) return label->text();
	return std::nullopt;
}

/// @brief Tells whether a panel is the one under the pointer, or holds it.
/// @param panel The panel.
/// @return true if it is hovered.
bool isHovered(const Panel& panel) {
	for(const Panel* at = panel.window() ? panel.window()->hoveredPanel() : nullptr; at; at = at->parent())
		if(at == &panel) return true;
	return false;
}

/// @brief Reads a property every panel has.
using Reader = json (*)(const Panel&);

/// @brief The properties every panel has. Positions and sizes are the panel's whole area in window units, before
/// clipping, from the last layout.
constexpr std::array<std::pair<std::string_view, Reader>, 17> everyPanel = {{
    {"color", [](const Panel& panel) { return json(panel.color().toHex()); }},
    {"borderColor", [](const Panel& panel) { return json(panel.borderColor().toHex()); }},
    {"borderWidth", [](const Panel& panel) { return json(panel.borderWidth()); }},
    {"shown", [](const Panel& panel) { return json(panel.isShown()); }},
    {"visible", [](const Panel& panel) { return json(panel.isVisible()); }},
    {"focused", [](const Panel& panel) { return json(panel.hasFocus()); }},
    {"hovered", [](const Panel& panel) { return json(isHovered(panel)); }},
    {"tooltip", [](const Panel& panel) { return json(panel.tooltip()); }},
    {"x", [](const Panel& panel) { return json(panel.frame().left); }},
    {"y", [](const Panel& panel) { return json(panel.frame().top); }},
    {"width", [](const Panel& panel) { return json(panel.frame().width()); }},
    {"height", [](const Panel& panel) { return json(panel.frame().height()); }},
    {"scrollX", [](const Panel& panel) { return json(panel.scrollOffset().x); }},
    {"scrollY", [](const Panel& panel) { return json(panel.scrollOffset().y); }},
    {"role", [](const Panel& panel) { return json(accessibleRole(panel)); }},
    {"name", [](const Panel& panel) { return json(accessibleName(panel)); }},
    {"description", [](const Panel& panel) { return json(panel.accessibleDescription()); }},
}};

/// @brief Reads a property that every panel has, or that its kind of panel has.
/// @param panel The node.
/// @param name A name isReadableProperty() accepts.
/// @return The value, or std::nullopt if this node hasn't that property.
std::optional<json> propertyOf(const Panel& panel, std::string_view name) {
	if(name == "text") return textOf(panel).transform([](std::string text) { return json(std::move(text)); });
	for(const auto& [property, read] : everyPanel)
		if(property == name) return read(panel);
	if(const auto* label = dynamic_cast<const Label*>(&panel); label && name == "textColor")
		return label->textColor().toHex();
	if(const auto* toggle = dynamic_cast<const Toggle*>(&panel); toggle && name == "checked") return toggle->isChecked();
	if(const auto* slider = dynamic_cast<const Slider*>(&panel); slider && name == "value") return slider->value();
	return std::nullopt;
}

/// @brief Compares what a node has with what was expected: numbers within a hundredth (positions and sizes are
/// fractions of pixels), colours whatever their case, the rest exactly.
/// @param actual What it has.
/// @param expected What was expected.
/// @return Whether they match.
bool matches(const json& actual, const json& expected) {
	if(actual.is_number() && expected.is_number())
		return std::abs(actual.get<double>() - expected.get<double>()) < 0.01;
	if(actual.is_string() && expected.is_string() && actual.get<std::string>().starts_with('#')) {
		std::string want = expected.get<std::string>();
		std::ranges::transform(want, want.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return actual.get<std::string>() == want;
	}
	return actual == expected;
}

} // namespace

Panel* findTarget(Panel& root, const json& target, long long nth, bool shownOnly) {
	if(target.is_string()) return findById(root, std::string_view(target.get_ref<const std::string&>()).substr(1), nth,
	                                       shownOnly);
	const std::vector<Panel*> found = findByRole(root, target["role"].get<std::string>(), target.value("name", ""));
	return std::cmp_less(nth, found.size()) ? found[static_cast<std::size_t>(nth)] : nullptr;
}

std::string targetName(const json& target, long long nth) {
	std::string name = target.is_string() ? target.get<std::string>() : target["role"].get<std::string>();
	if(target.is_object() && target.contains("name")) name += " '" + target["name"].get<std::string>() + "'";
	return name + (nth ? " (nth " + std::to_string(nth) + ")" : "");
}

std::string checkState(const Screen& screen, const json& expect) {
	const std::string path = expect["state"].get<std::string>();
	const std::optional<std::vector<std::string>> segments = parseStatePath(path); // checked when the file was read
	if(!segments) return "state " + path + " isn't a path";
	const std::optional<json> value = follow(json::parse(screen.stateJson()), *segments);
	if(!value) return "state " + path + " doesn't exist, expected " + shown(expect["is"]);
	if(*value == expect["is"]) return {};
	return "state " + path + " is " + shown(*value) + ", expected " + shown(expect["is"]);
}

std::string checkData(Screen& screen, const json& expect) {
	const std::string path = expect["data"].get<std::string>();
	const std::optional<std::vector<std::string>> segments = parseStatePath(path); // checked when the file was read
	if(!segments) return "data " + path + " isn't a path";
	const std::optional<json> value = follow(json::parse(screen.dataJson()), *segments);
	if(!value) return "data " + path + " doesn't exist, expected " + shown(expect["is"]);
	if(*value == expect["is"]) return {};
	return "data " + path + " is " + shown(*value) + ", expected " + shown(expect["is"]);
}

std::string checkNode(Panel& root, const json& expect) {
	const long long nth = expect.value("nth", 0LL);
	const Panel* panel = findTarget(root, expect["node"], nth, false);
	const std::string name = targetName(expect["node"], nth);

	if(expect.contains("exists")) {
		const bool wanted = expect["exists"].get<bool>();
		if((panel != nullptr) == wanted) return {};
		return name + (wanted ? " doesn't exist" : " exists") + ", expected " + (wanted ? "it to" : "it not to");
	}
	if(!panel) return name + " doesn't exist";
	const std::string property = expect.contains("text") ? "text" : expect["property"].get<std::string>();
	const json& wanted = expect.contains("text") ? expect["text"] : expect["is"];
	const std::optional<json> value = propertyOf(*panel, property);
	if(!value) return name + " has no " + property;
	if(matches(*value, wanted)) return {};
	return name + " " + property + " is " + shown(*value) + ", expected " + shown(wanted);
}

} // namespace affresco::automation::detail
