/// @file
/// @brief The "tabs" node type: a Tabs strip, its tabs and colours, the active tab as its value, its close and move
/// events, and its part in drag and drop (reordering, and tabs dragged between strips).

#include "NodeKeys.hpp"
#include "TypeRegistry.hpp"

#include "affresco/render/Font.hpp"
#include "affresco/render/Tabs.hpp"
#include "affresco/render/Window.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace affresco::format {

namespace {

constexpr std::size_t maxTabs = 10'000; ///< The most tabs a strip reads (untrusted input).

/// @brief Reads one tab: a string (its id and label) or {id, label, marker, closable, icon, iconColor, italic}.
/// @param value The tab.
/// @param path Its place, for errors.
/// @return The tab.
Tab readTab(const json& value, const std::string& path) {
	if(value.is_string()) {
		Tab tab;
		tab.id = tab.label = value.get<std::string>();
		return tab;
	}
	object(value, path);
	Tab tab;
	tab.id = value.contains("id") ? text(value["id"]) : std::string();
	if(tab.id.empty()) fail(path + ".id", "a tab needs an id");
	tab.label = value.contains("label") ? text(value["label"]) : tab.id;
	tab.marker = value.contains("marker") && boolean(value["marker"], path + ".marker");
	tab.closable = !value.contains("closable") || boolean(value["closable"], path + ".closable");
	if(value.contains("icon") && !value["icon"].is_null()) tab.icon = text(value["icon"]);
	if(value.contains("iconColor") && !value["iconColor"].is_null()) {
		tab.iconColor =
		    value["iconColor"].is_string() ? Color::parseHex(value["iconColor"].get<std::string>()) : std::nullopt;
		if(!tab.iconColor) fail(path + ".iconColor", "expected a hex colour: #rgb, #rrggbb or #rrggbbaa");
	}
	tab.italic = value.contains("italic") && boolean(value["italic"], path + ".italic");
	return tab;
}

/// @brief Sets a strip's tabs, font and colours: those the node gives, the defaults for the others.
/// @param panel The panel, a Tabs.
/// @param values Its tabs (and the slanted font of those marked italic), font, textColor, activeTextColor,
/// activeBackground, accentColor, accentEdge, markerColor and keepMarker, if given.
/// @param reader Reads colours and fonts.
/// @param path The node's place, for errors.
void applyTabs(Panel& panel, const json& values, const PropertyReader& reader, const std::string& path) {
	auto* tabs = dynamic_cast<Tabs*>(&panel);
	if(!tabs) return;
	const auto colorOr = [&](const char* key, Color fallback) {
		return values.contains(key) ? reader.color(values[key], path + "." + key) : fallback;
	};
	std::vector<Tab> list;
	if(values.contains("tabs")) {
		const json& given = values["tabs"];
		if(!given.is_array()) fail(path + ".tabs", "expected a list of tabs");
		list.reserve(std::min(given.size(), maxTabs));
		for(std::size_t i = 0; i < given.size() && i < maxTabs; ++i)
			list.push_back(readTab(given[i], concatenate(path, ".tabs[", std::to_string(i) + "]")));
	}
	const bool italic = std::ranges::any_of(list, &Tab::italic);
	tabs->setTabs(std::move(list));
	const std::shared_ptr<const Font> named =
	    values.contains("font") ? reader.font(values["font"], path + ".font") : nullptr;
	tabs->setFont(named);
	// Italic tabs: the strip's font slanted, as a rich text run's (none while the strip has no font yet)
	const Font* base = named ? named.get() : tabs->window() ? tabs->window()->defaultFont() : nullptr;
	tabs->setItalicFont(italic && base ? reader.fontVariant(named.get(), base->pixelHeight(),
	                                                        FontStyle{base->style().bold, true}, path + ".tabs")
	                                   : nullptr);
	tabs->setColors(colorOr("textColor", colors::muted), colorOr("activeTextColor", colors::black),
	                colorOr("activeBackground", colors::white), colorOr("accentColor", colors::accent));
	const std::string edge =
	    values.contains("accentEdge") ? string(values["accentEdge"], path + ".accentEdge") : "bottom";
	if(edge != "top" && edge != "bottom") fail(path + ".accentEdge", R"(expected "top" or "bottom")");
	tabs->setAccentOnTop(edge == "top");
	tabs->setMarkerColor(values.contains("markerColor")
	                         ? std::optional<Color>(reader.color(values["markerColor"], path + ".markerColor"))
							 : std::nullopt);
	tabs->setKeepMarker(values.contains("keepMarker") && boolean(values["keepMarker"], path + ".keepMarker"));
}

/// @brief Shows the active tab: the bound value, else "value".
/// Its last text is not used.
/// @param panel The panel, a Tabs.
/// @param bound The bound state value, or nullptr.
/// @param properties The node's evaluated properties.
void showActive(Panel& panel, const json* bound, const json& properties, std::optional<std::string>& /*lastText*/) {
	auto* tabs = dynamic_cast<Tabs*>(&panel);
	if(!tabs) return;
	const json& value = bound ? *bound : properties.contains("value") ? properties["value"] : emptyObject();
	tabs->setActive(value.is_string() || value.is_number() ? text(value) : std::string());
}

/// @brief Reports the tab the user picked.
/// @param panel The panel, a Tabs.
/// @param changed Gets its id.
/// @return The connection.
Connection listenToTabs(Panel& panel, std::function<void(const json&)> changed) {
	auto* tabs = dynamic_cast<Tabs*>(&panel);
	if(!tabs) return 0;
	return tabs->onActivated.connect([changed = std::move(changed)](const std::string& id) { changed(json(id)); });
}

/// @brief Disconnects listenToTabs().
/// @param panel The panel.
/// @param connection The connection.
void stopListeningToTabs(Panel& panel, Connection connection) {
	if(auto* tabs = dynamic_cast<Tabs*>(&panel)) tabs->onActivated.disconnect(connection);
}

/// @brief Runs "onClose" and "onMove" for the strip's close and move signals.
/// @param panel The panel, a Tabs.
/// @param sink Runs a handler.
/// @return The connections.
EventConnections listenToTabEvents(Panel& panel, EventSink sink) {
	auto* tabs = dynamic_cast<Tabs*>(&panel);
	if(!tabs) return {};
	const Connection close = tabs->onCloseRequested.connect(
	    [sink](const std::string& id) { sink("onClose", json{{"type", "close"}, {"id", id}}); });
	const Connection move = tabs->onMoved.connect([sink = std::move(sink)](const TabMove& moved) {
		sink("onMove", json{{"type", "move"},
		                    {"id", moved.id},
		                    {"from", static_cast<std::int64_t>(moved.from)},
		                    {"to", static_cast<std::int64_t>(moved.to)}});
	});
	return {close, move, 0, 0};
}

/// @brief Disconnects listenToTabEvents().
/// @param panel The panel.
/// @param connections The connections.
void stopListeningToTabEvents(Panel& panel, const EventConnections& connections) {
	auto* tabs = dynamic_cast<Tabs*>(&panel);
	if(!tabs) return;
	tabs->onCloseRequested.disconnect(connections[0]);
	tabs->onMoved.disconnect(connections[1]);
}

/// @brief The data of a tab dragged out of its strip: its id and place.
/// @param panel The panel, a Tabs.
/// @param press Where the drag started.
/// @return {"id", "index"}, or null off the tabs.
json dragData(const Panel& panel, Vec2 press) {
	const auto* tabs = dynamic_cast<const Tabs*>(&panel);
	if(!tabs) return nullptr;
	const std::size_t index = tabs->tabAt(press);
	if(index >= tabs->tabs().size()) return nullptr;
	return {{"id", tabs->tabs()[index].id}, {"index", static_cast<std::int64_t>(index)}};
}

/// @brief Shows where a tab dragged from elsewhere would go.
/// @param panel The panel, a Tabs.
/// @param position The pointer, or nullptr to hide the indicator.
/// @return {"index"}: the place in the strip.
json dropAt(Panel& panel, const Vec2* position) {
	auto* tabs = dynamic_cast<Tabs*>(&panel);
	if(!tabs) return json::object();
	if(!position) {
		tabs->showDropIndicator(std::nullopt);
		return json::object();
	}
	const std::size_t index = tabs->insertionAt(*position);
	tabs->showDropIndicator(index);
	return {{"index", static_cast<std::int64_t>(index)}};
}

} // namespace

void registerTabsType(TypeRegistry& registry) {
	const auto set = std::make_shared<PropertySet>();
	set->properties = {{"bind", ValueKind::StatePath},
	                   {"value", ValueKind::Text},
	                   {"tabs", ValueKind::Any},
	                   {"font", ValueKind::Font},
	                   {"textColor", ValueKind::Color},
	                   {"activeTextColor", ValueKind::Color},
	                   {"activeBackground", ValueKind::Color},
	                   {"accentColor", ValueKind::Color},
	                   {"accentEdge", ValueKind::Text},
	                   {"markerColor", ValueKind::Color},
	                   {"keepMarker", ValueKind::Boolean}};
	set->apply = applyTabs;
	set->everyUpdate = true; // colours follow the theme
	set->keepsClicks = true;
	set->appliesTo = [](const Panel& panel) { return dynamic_cast<const Tabs*>(&panel) != nullptr; };
	set->value = {"value", showActive, listenToTabs, stopListeningToTabs};
	set->events = {listenToTabEvents, stopListeningToTabEvents};
	set->drag = {dragData, dropAt, true};

	registry.add({"tabs", "A row of tabs, one active; they scroll, close, reorder and move between strips",
	              [] { return std::make_unique<Tabs>(); }, set, false});
}

} // namespace affresco::format
