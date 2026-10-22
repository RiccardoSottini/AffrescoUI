/// @file
/// @brief The "list", "tree" and "table" node types: virtualised ListView, TreeView and TableView rows from state, the
/// selection as their value, their open, expand, sort and resize events, and their part in drag and drop.

#include "ListItems.hpp"
#include "NodeKeys.hpp"
#include "TypeRegistry.hpp"

#include "affresco/render/TableView.hpp"
#include "affresco/render/TreeView.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace affresco::format {

namespace {

constexpr std::size_t maxColumns = 256;     ///< The most columns a table has.
constexpr double maxColumnWidth = 10'000.0; ///< The widest a column is, in pixels.
constexpr double maxStickyRows = 16.0;      ///< The most folders a tree pins at its top.

/// @brief Which of the three types a set of properties is for.
enum class ListKind : std::uint8_t {
	List,  ///< "list"
	Tree,  ///< "tree"
	Table, ///< "table"
};

/// @brief Reads "selection": "none", "single" or "multiple".
/// @param value The value.
/// @param path Its place, for errors.
/// @return The mode.
SelectionMode readSelectionMode(const json& value, const std::string& path) {
	const std::string mode = string(value, path);
	if(mode == "none") return SelectionMode::None;
	if(mode == "single") return SelectionMode::Single;
	if(mode == "multiple") return SelectionMode::Multiple;
	fail(path, R"(expected "none", "single" or "multiple")");
}

/// @brief Reads a table's "columns": texts (the id, also the title), or { id, title, width }.
/// @param value The value.
/// @param path Its place, for errors.
/// @return The columns.
std::vector<TableColumn> readColumns(const json& value, const std::string& path) {
	if(!value.is_array()) fail(path, "expected a list of columns");
	if(value.size() > maxColumns) fail(path, "too many columns (at most 256)");
	std::vector<TableColumn> columns;
	columns.reserve(value.size());
	for(std::size_t i = 0; i < value.size(); ++i) {
		const json& entry = value[i];
		const std::string place = path + "[" + std::to_string(i) + "]";
		if(entry.is_string() || entry.is_number()) {
			columns.push_back({text(entry), text(entry)});
			continue;
		}
		if(!entry.is_object() || !entry.contains("id")) fail(place, R"(expected a column: text, or {"id", "title"})");
		TableColumn column{text(entry["id"]), entry.contains("title") ? text(entry["title"]) : text(entry["id"])};
		if(entry.contains("width"))
			column.width = static_cast<float>(std::clamp(double{number(entry["width"], place + ".width")},
			                                             double{TableView::minimumColumnWidth()}, maxColumnWidth));
		columns.push_back(std::move(column));
	}
	return columns;
}

/// @brief Reads a table's "sort": { column, descending }, or null for the items' order.
/// @param value The value.
/// @param path Its place, for errors.
/// @return The order.
TableSort readSort(const json& value, const std::string& path) {
	if(value.is_null()) return {};
	if(!value.is_object() || !value.contains("column")) fail(path, R"(expected {"column", "descending"} or null)");
	return {text(value["column"]), value.contains("descending") && boolean(value["descending"], path + ".descending")};
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

/// @brief Sets what only trees and tables have: a tree's open folders and sticky rows, a table's columns and order.
/// @param list The list.
/// @param values The node's properties.
/// @param path The node's place, for errors.
void applyTreeOrTable(ListView& list, const json& values, const std::string& path) {
	if(auto* tree = dynamic_cast<TreeView*>(&list)) {
		tree->setExpanded(values.contains("expanded") ? readIds(values["expanded"], path + ".expanded")
		                                              : std::vector<std::string>());
		const double sticky = values.contains("stickyRows") ? number(values["stickyRows"], path + ".stickyRows") : 0.0;
		tree->setStickyRows(static_cast<std::size_t>(std::clamp(sticky, 0.0, maxStickyRows)));
	} else if(auto* table = dynamic_cast<TableView*>(&list)) {
		table->setColumns(values.contains("columns") ? readColumns(values["columns"], path + ".columns")
		                                             : std::vector<TableColumn>());
		table->setSort(values.contains("sort") ? readSort(values["sort"], path + ".sort") : TableSort());
	}
}

/// @brief Sets a list's, a tree's or a table's items, selection mode, row height, font and colours, and what only
/// trees and tables have (applyTreeOrTable()).
/// @param panel The panel, a ListView.
/// @param values Its items, selection, rowHeight, font, colours and the rest, if given.
/// @param reader Reads colours and fonts.
/// @param path The node's place, for errors.
void applyList(Panel& panel, const json& values, const PropertyReader& reader, const std::string& path) {
	auto* list = dynamic_cast<ListView*>(&panel);
	if(!list) return;
	const auto colorOr = [&](const char* key, Color fallback) {
		return values.contains(key) ? reader.color(values[key], path + "." + key) : fallback;
	};
	applyItems(*list, values, dynamic_cast<TreeView*>(list) != nullptr, path);
	applyTreeOrTable(*list, values, path);
	list->setSelectionMode(values.contains("selection") ? readSelectionMode(values["selection"], path + ".selection")
	                                                    : SelectionMode::Single);
	list->setRowHeight(values.contains("rowHeight") ? number(values["rowHeight"], path + ".rowHeight") : 24.0f);
	list->setFont(values.contains("font") ? reader.font(values["font"], path + ".font") : nullptr);
	list->setColors(colorOr("textColor", colors::black), colorOr("selectedTextColor", colors::white),
	                colorOr("selectedBackground", colors::accent), colorOr("accentColor", colors::accent));
}

/// @brief Shows the selection: the bound value, else "value"; an id, or a list of ids.
/// Its last text is not used.
/// @param panel The panel, a ListView.
/// @param bound The bound state value, or nullptr.
/// @param properties The node's evaluated properties.
void showSelection(Panel& panel, const json* bound, const json& properties, std::optional<std::string>& /*lastText*/) {
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
	return list->onSelectionChanged.connect([list, changed = std::move(changed)](const std::vector<std::string>& ids) {
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

/// @brief Runs "onOpen" for Enter or a double click, a tree's "onExpand", and a table's "onSort" and
/// "onColumnResize".
/// @param panel The panel, a ListView.
/// @param sink Runs a handler.
/// @return The connections.
EventConnections listenToListEvents(Panel& panel, EventSink sink) {
	auto* list = dynamic_cast<ListView*>(&panel);
	if(!list) return {};
	const Connection open =
	    list->onOpened.connect([sink](const std::string& id) { sink("onOpen", json{{"type", "open"}, {"id", id}}); });
	Connection expand = 0, sort = 0, resize = 0;
	if(auto* tree = dynamic_cast<TreeView*>(list))
		expand = tree->onExpandedChanged.connect([sink](const ExpandChange& change) {
			sink("onExpand", json{{"type", "expand"}, {"id", change.id}, {"expanded", change.expanded}});
		});
	if(auto* table = dynamic_cast<TableView*>(list)) {
		sort = table->onSortChanged.connect([sink](const TableSort& order) {
			sink("onSort", json{{"type", "sort"}, {"column", order.column}, {"descending", order.descending}});
		});
		resize = table->onColumnResized.connect([sink](const ColumnResize& change) {
			sink("onColumnResize", json{{"type", "columnResize"}, {"column", change.column}, {"width", change.width}});
		});
	}
	return {open, expand, sort, resize};
}

/// @brief Disconnects listenToListEvents().
/// @param panel The panel.
/// @param connections The connections.
void stopListeningToListEvents(Panel& panel, const EventConnections& connections) {
	auto* list = dynamic_cast<ListView*>(&panel);
	if(!list) return;
	list->onOpened.disconnect(connections[0]);
	if(auto* tree = dynamic_cast<TreeView*>(list)) tree->onExpandedChanged.disconnect(connections[1]);
	if(auto* table = dynamic_cast<TableView*>(list)) {
		table->onSortChanged.disconnect(connections[2]);
		table->onColumnResized.disconnect(connections[3]);
	}
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
	const char* place = target.place == DropPlace::Inside  ? "inside"
	                    : target.place == DropPlace::After ? "after"
	                                                       : "before";
	return {{"index", static_cast<std::int64_t>(target.row)},
	        {"id", onRow ? json(list->model()->item(list->itemAt(target.row)).id) : json(nullptr)},
	        {"place", place}};
}

/// @brief Whether a panel is a tree.
/// @param panel The panel.
/// @return true when it is.
bool isTree(const Panel& panel) { return dynamic_cast<const TreeView*>(&panel) != nullptr; }

/// @brief Whether a panel is a table.
/// @param panel The panel.
/// @return true when it is.
bool isTable(const Panel& panel) { return dynamic_cast<const TableView*>(&panel) != nullptr; }

/// @brief Whether a panel is a list that is neither a tree nor a table.
/// @param panel The panel.
/// @return true when it is.
bool isPlainList(const Panel& panel) {
	return dynamic_cast<const ListView*>(&panel) != nullptr && !isTree(panel) && !isTable(panel);
}

/// @brief The properties, hooks and events of lists, trees and tables.
/// @param kind Which: a tree adds "expanded" and "stickyRows", a table "columns" and "sort".
/// @return The set.
std::shared_ptr<PropertySet> listProperties(ListKind kind) {
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
	if(kind == ListKind::Tree)
		set->properties.insert(set->properties.begin() + 3,
		                       {{"expanded", ValueKind::Any}, {"stickyRows", ValueKind::Number}});
	else if(kind == ListKind::Table)
		set->properties.insert(set->properties.begin() + 3, {{"columns", ValueKind::Any}, {"sort", ValueKind::Any}});
	set->apply = applyList;
	set->everyUpdate = true; // colours follow the theme; unchanged items are skipped
	set->keepsClicks = true;
	set->appliesTo = kind == ListKind::Tree ? isTree : kind == ListKind::Table ? isTable : isPlainList;
	set->value = {"value", showSelection, listenToSelection, stopListeningToSelection};
	set->events = {listenToListEvents, stopListeningToListEvents};
	set->drag = {dragData, dropAt, false};
	return set;
}

} // namespace

void registerListTypes(TypeRegistry& registry) {
	registry.add({"list", "A virtualised list: selection, keyboard, type-to-find and drag and drop, any length",
	              [] { return std::make_unique<ListView>(); }, listProperties(ListKind::List), false});
	registry.add({"tree",
	              "A virtualised tree: folders open and close, guides, sticky folders, keyboard, multi-select, drag "
	              "and drop",
	              [] { return std::make_unique<TreeView>(); }, listProperties(ListKind::Tree), false});
	registry.add({"table", "A virtualised table: columns, sorting by a header click, resizable columns, any length",
	              [] { return std::make_unique<TableView>(); }, listProperties(ListKind::Table), false});
}

} // namespace affresco::format
