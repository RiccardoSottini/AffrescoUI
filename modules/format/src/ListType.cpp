/// @file
/// @brief The "list" and "tree" node types: virtualised ListView and TreeView rows from state, the selection as their
/// value, their open and expand events, and their part in drag and drop.

#include "ListItems.hpp"
#include "NodeKeys.hpp"
#include "TypeRegistry.hpp"

#include "affresco/render/TreeView.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace affresco::format {

namespace {

/// @brief Reads "selection": "none", "single" or "multiple".
/// @param value The value.
/// @param path Its place, for errors.
/// @return The mode.
SelectionMode readSelectionMode(const json& value, const std::string& path) {
	const std::string mode = string(value, path);
	if(mode == "none") return SelectionMode::None;
	if(mode == "single") return SelectionMode::Single;
	if(mode == "multiple") return SelectionMode::Multiple;
	fail(path, "expected \"none\", \"single\" or \"multiple\"");
}

/// @brief Sets a list's items, unless they are the ones it shows already.
/// @param list The list.
/// @param values The node's properties.
/// @param tree Whether the items nest ("children").
/// @param path The node's place, for errors.
void applyItems(ListView& list, const json& values, bool tree, const std::string& path) {
	const json& given = values.contains("items") ? values["items"] : emptyObject();
	// Re-reading 100,000 items on every update would cost more than comparing them
	if(const auto* shown = dynamic_cast<const SourcedItems*>(list.model().get()); shown && shown->source() == given)
		return;
	if(given.empty()) {
		list.setModel(nullptr);
		return;
	}
	list.setModel(std::make_shared<SourcedItems>(readItems(given, tree, path + ".items"), given));
}

/// @brief Sets a list's or a tree's items, open folders, selection mode, row height, font and colours.
/// @param panel The panel, a ListView or a TreeView.
/// @param values Its items, expanded, selection, rowHeight, font and colours, if given.
/// @param reader Reads colours and fonts.
/// @param path The node's place, for errors.
void applyList(Panel& panel, const json& values, const PropertyReader& reader, const std::string& path) {
	auto* list = dynamic_cast<ListView*>(&panel);
	if(!list) return;
	const auto colorOr = [&](const char* key, Color fallback) {
		return values.contains(key) ? reader.color(values[key], path + "." + key) : fallback;
	};
	auto* tree = dynamic_cast<TreeView*>(list);
	applyItems(*list, values, tree != nullptr, path);
	if(tree) tree->setExpanded(values.contains("expanded") ? readIds(values["expanded"], path + ".expanded") :
	                                                          std::vector<std::string>());
	list->setSelectionMode(values.contains("selection") ? readSelectionMode(values["selection"], path + ".selection") :
	                                                      SelectionMode::Single);
	list->setRowHeight(values.contains("rowHeight") ? static_cast<float>(number(values["rowHeight"], path + ".rowHeight")) :
	                                                  24.0f);
	list->setFont(values.contains("font") ? reader.font(values["font"], path + ".font") : nullptr);
	list->setColors(colorOr("textColor", colors::black), colorOr("selectedTextColor", colors::white),
	                colorOr("selectedBackground", colors::accent), colorOr("accentColor", colors::accent));
}

/// @brief Shows the selection: the bound value, else "value"; an id, or a list of ids.
/// @param panel The panel, a ListView.
/// @param bound The bound state value, or nullptr.
/// @param properties The node's evaluated properties.
/// @param lastText Not used.
void showSelection(Panel& panel, const json* bound, const json& properties, std::optional<std::string>& lastText) {
	static_cast<void>(lastText);
	auto* list = dynamic_cast<ListView*>(&panel);
	if(!list) return;
	const json& value = bound ? *bound : properties.contains("value") ? properties["value"] : emptyObject();
	std::vector<std::string> ids;
	if(value.is_string() || value.is_number()) ids.push_back(text(value));
	else if(value.is_array())
		for(std::size_t i = 0; i < value.size() && i < maxItems; ++i)
			if(value[i].is_string() || value[i].is_number()) ids.push_back(text(value[i]));
	list->setSelection(std::move(ids));
}

/// @brief Reports the selection the user made: an id (null for none), or with "multiple" a list of ids.
/// @param panel The panel, a ListView.
/// @param changed Gets the selection.
/// @return The connection.
Connection listenToSelection(Panel& panel, std::function<void(const json&)> changed) {
	auto* list = dynamic_cast<ListView*>(&panel);
	if(!list) return 0;
	return list->onSelectionChanged.connect(
	    [list, changed = std::move(changed)](const std::vector<std::string>& ids) {
		    if(list->selectionMode() == SelectionMode::Multiple) changed(json(ids));
		    else changed(ids.empty() ? json(nullptr) : json(ids.front()));
	    });
}

/// @brief Disconnects listenToSelection().
/// @param panel The panel.
/// @param connection The connection.
void stopListeningToSelection(Panel& panel, Connection connection) {
	if(auto* list = dynamic_cast<ListView*>(&panel)) list->onSelectionChanged.disconnect(connection);
}

/// @brief Runs "onOpen" for Enter or a double click, and a tree's "onExpand".
/// @param panel The panel, a ListView or a TreeView.
/// @param sink Runs a handler.
/// @return The connections.
EventConnections listenToListEvents(Panel& panel, EventSink sink) {
	auto* list = dynamic_cast<ListView*>(&panel);
	if(!list) return {};
	const Connection open =
	    list->onOpened.connect([sink](const std::string& id) { sink("onOpen", json{{"type", "open"}, {"id", id}}); });
	Connection expand = 0;
	if(auto* tree = dynamic_cast<TreeView*>(list))
		expand = tree->onExpandedChanged.connect([sink](const ExpandChange& change) {
			sink("onExpand", json{{"type", "expand"}, {"id", change.id}, {"expanded", change.expanded}});
		});
	return {open, expand, 0, 0};
}

/// @brief Disconnects listenToListEvents().
/// @param panel The panel.
/// @param connections The connections.
void stopListeningToListEvents(Panel& panel, const EventConnections& connections) {
	auto* list = dynamic_cast<ListView*>(&panel);
	if(!list) return;
	list->onOpened.disconnect(connections[0]);
	if(auto* tree = dynamic_cast<TreeView*>(list)) tree->onExpandedChanged.disconnect(connections[1]);
}

/// @brief The data of rows dragged out of a list: the selection when the pressed row is in it, else that row.
/// @param panel The panel, a ListView.
/// @param press Where the drag started.
/// @return {"id", "ids"}, or null off the rows.
json dragData(const Panel& panel, Vec2 press) {
	const auto* list = dynamic_cast<const ListView*>(&panel);
	if(!list) return nullptr;
	const std::size_t row = list->rowAt(press);
	if(row >= list->rowCount()) return nullptr;
	const std::string& id = list->model()->item(list->itemAt(row)).id;
	return {{"id", id}, {"ids", list->isSelected(id) ? json(list->selection()) : json::array({id})}};
}

/// @brief Shows where rows dragged over a list would land.
/// @param panel The panel, a ListView.
/// @param position The pointer, or nullptr to hide the indicator.
/// @return {"index", "id", "place"}: the row, its item's id (null past the last) and "before", "inside" or "after".
json dropAt(Panel& panel, const Vec2* position) {
	auto* list = dynamic_cast<ListView*>(&panel);
	if(!list) return json::object();
	if(!position) {
		list->showDropIndicator(std::nullopt);
		return json::object();
	}
	const DropTarget target = list->dropTargetAt(*position);
	list->showDropIndicator(target);
	const bool onRow = target.row < list->rowCount();
	const char* place = target.place == DropPlace::Inside ? "inside" : target.place == DropPlace::After ? "after" :
	                                                                                                     "before";
	return {{"index", static_cast<std::int64_t>(target.row)},
	        {"id", onRow ? json(list->model()->item(list->itemAt(target.row)).id) : json(nullptr)},
	        {"place", place}};
}

/// @brief The properties, hooks and events shared by lists and trees.
/// @param tree Whether it is the tree's, which has "expanded".
/// @return The set.
std::shared_ptr<PropertySet> listProperties(bool tree) {
	auto set = std::make_shared<PropertySet>();
	set->properties = {{"bind", ValueKind::StatePath},
	                   {"value", ValueKind::Any},
	                   {"items", ValueKind::Any},
	                   {"selection", ValueKind::Text},
	                   {"rowHeight", ValueKind::Number},
	                   {"font", ValueKind::Font},
	                   {"textColor", ValueKind::Color},
	                   {"selectedTextColor", ValueKind::Color},
	                   {"selectedBackground", ValueKind::Color},
	                   {"accentColor", ValueKind::Color}};
	if(tree) set->properties.insert(set->properties.begin() + 3, {"expanded", ValueKind::Any});
	set->apply = applyList;
	set->everyUpdate = true; // colours follow the theme; unchanged items are skipped
	set->keepsClicks = true;
	if(tree) set->appliesTo = [](const Panel& panel) { return dynamic_cast<const TreeView*>(&panel) != nullptr; };
	else
		set->appliesTo = [](const Panel& panel) {
			return dynamic_cast<const ListView*>(&panel) != nullptr && dynamic_cast<const TreeView*>(&panel) == nullptr;
		};
	set->value = {"value", showSelection, listenToSelection, stopListeningToSelection};
	set->events = {listenToListEvents, stopListeningToListEvents};
	set->drag = {dragData, dropAt, false};
	return set;
}

} // namespace

void registerListTypes(TypeRegistry& registry) {
	registry.add({"list", "A virtualised list: selection, keyboard, type-to-find and drag and drop, any length",
	              [] { return std::make_unique<ListView>(); }, listProperties(false), false});
	registry.add({"tree", "A virtualised tree: folders open and close, guides, keyboard, multi-select, drag and drop",
	              [] { return std::make_unique<TreeView>(); }, listProperties(true), false});
}

} // namespace affresco::format
