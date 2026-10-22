/// @file
/// @brief Tables, sticky folders and scroll anchoring on a headless window: a table's sorting by a header click and
/// its resizable columns, the order and the widths given against the user's; a tree's folders pinned as it scrolls;
/// a list keeping its top row when items are added above it; reading cells within limits; and the table node, its
/// sort and resize events into state.
///
/// A table's header is 28 pixels high and its columns 120 wide by default, so column 0's right edge is at x = 120.

#include "Headless.hpp"
#include "ListItems.hpp"

#include <affresco/affresco.hpp>
#include <affresco/render/ListView.hpp>
#include <affresco/render/TableView.hpp>
#include <affresco/render/TreeView.hpp>
#include <affresco/render/Window.hpp>

#include <doctest/doctest.h>

#include <memory>
#include <string>
#include <vector>

using affresco::Action;
using affresco::ColumnResize;
using affresco::ItemList;
using affresco::json;
using affresco::ListItem;
using affresco::ListView;
using affresco::MouseButton;
using affresco::px;
using affresco::TableColumn;
using affresco::TableSort;
using affresco::TableView;
using affresco::TreeView;
using affresco::Vec2;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;
using affresco::format::readItems;

namespace {

/// @brief Presses the left button, moves to a point while holding it, and lets go there.
/// @param window The window.
/// @param from Where the press is.
/// @param to Where the release is.
void drag(affresco::Window& window, Vec2 from, Vec2 to) {
	window.routeMouseButton({MouseButton::Left, Action::Press, {}, from});
	window.routeMouseMove(to);
	window.routeMouseButton({MouseButton::Left, Action::Release, {}, to});
	window.render();
}

/// @brief Presses and releases the left button at a point.
/// @param window The window.
/// @param position Where.
void click(affresco::Window& window, Vec2 position) { drag(window, position, position); }

/// @brief The ids of the items a view shows, top to bottom.
/// @param view The view.
/// @return The ids.
[[nodiscard]] std::vector<std::string> shownIds(const ListView& view) {
	std::vector<std::string> ids;
	for(std::size_t row = 0; row < view.rowCount(); ++row) ids.push_back(view.model()->item(view.itemAt(row)).id);
	return ids;
}

/// @brief A table of four items, name and size, on a 240 by 148 window: the header and five rows.
struct TableFixture {
	ManualClock clock;                        ///< The window's clock.
	std::unique_ptr<affresco::Window> window; ///< The window.
	TableView* table = nullptr;               ///< The table.
	std::vector<TableSort> sorts;             ///< Every onSortChanged.
	std::vector<ColumnResize> resizes;        ///< Every onColumnResized.

	/// @brief Builds it.
	TableFixture() : window(createHeadlessWindow(240, 148, clock)) {
		table = &window->root().add<TableView>();
		table->setSize(px(240), px(148));
		const json items = json::parse(R"([{ "id": "b", "cells": ["beta", 10] }, { "id": "a", "cells": ["alpha", 9] },
			{ "id": "c", "cells": ["gamma", "x"] }, { "id": "d", "cells": ["delta", "9"] }])");
		table->setModel(std::make_shared<const ItemList>(readItems(items, false, "items")));
		table->setColumns({{"name", "Name"}, {"size", "Size"}});
		table->onSortChanged.connect([this](const TableSort& sort) { sorts.push_back(sort); });
		table->onColumnResized.connect([this](const ColumnResize& change) { resizes.push_back(change); });
		window->render();
	}
};

} // namespace

TEST_CASE("table: a header click sorts numbers before text, keeps equal cells in order, and a second click reverses") {
	TableFixture fixture;
	CHECK(shownIds(*fixture.table) == std::vector<std::string>{"b", "a", "c", "d"}); // the model's order
	click(*fixture.window, {180.0f, 14.0f});
	CHECK(shownIds(*fixture.table) == std::vector<std::string>{"a", "d", "b", "c"});
	CHECK(fixture.table->rowOf(2) == 3);
	for(std::size_t item = 0; item < 4; ++item) CHECK(fixture.table->itemAt(fixture.table->rowOf(item)) == item);
	fixture.clock.advance(1000.0);
	click(*fixture.window, {180.0f, 14.0f});
	CHECK(shownIds(*fixture.table) == std::vector<std::string>{"c", "b", "a", "d"});
	fixture.clock.advance(1000.0);
	click(*fixture.window, {60.0f, 14.0f}); // another column: ascending again
	CHECK(shownIds(*fixture.table) == std::vector<std::string>{"a", "b", "d", "c"});
	REQUIRE(fixture.sorts.size() == 3);
	CHECK(fixture.sorts[1] == TableSort{"size", true});
	CHECK(fixture.sorts[2] == TableSort{"name", false});
	CHECK(fixture.table->selection().empty()); // a header click selects nothing

	// A new order given replaces the user's; the same one again keeps what the user clicked since
	fixture.table->setSort({"size", true});
	fixture.window->render();
	CHECK(shownIds(*fixture.table).front() == "c");
	fixture.clock.advance(1000.0);
	click(*fixture.window, {60.0f, 14.0f});
	fixture.table->setSort({"size", true});
	CHECK(fixture.table->sort() == TableSort{"name", false});
	fixture.table->setSort({"missing", false}); // an unknown column: the model's order
	fixture.window->render();
	CHECK(shownIds(*fixture.table) == std::vector<std::string>{"b", "a", "c", "d"});
}

TEST_CASE("table: dragging a header's right edge resizes its column, no narrower than the minimum") {
	TableFixture fixture;
	drag(*fixture.window, {120.0f, 14.0f}, {200.0f, 14.0f});
	CHECK(fixture.table->columns()[0].width == 200.0f);
	REQUIRE(fixture.resizes.size() == 1);
	CHECK(fixture.resizes[0].column == "name");
	CHECK(fixture.resizes[0].width == 200.0f);
	CHECK(fixture.sorts.empty()); // an edge press doesn't sort

	// The same columns given again keep the width dragged to; different ones replace it
	fixture.table->setColumns({{"name", "Name"}, {"size", "Size"}});
	CHECK(fixture.table->columns()[0].width == 200.0f);
	fixture.clock.advance(1000.0);
	drag(*fixture.window, {201.0f, 14.0f}, {5.0f, 14.0f});
	CHECK(fixture.table->columns()[0].width == TableView::minimumColumnWidth());
	REQUIRE(fixture.resizes.size() == 2);
	CHECK(fixture.resizes[1].width == TableView::minimumColumnWidth());
	fixture.table->setColumns({{"name", "Name", 80.0f}, {"size", "Size"}});
	CHECK(fixture.table->columns()[0].width == 80.0f);
}

TEST_CASE("tree: with sticky rows, the open folders the top row is in stay pinned as it scrolls") {
	ManualClock clock;
	const auto window = createHeadlessWindow(200, 120, clock);
	auto& tree = window->root().add<TreeView>();
	tree.setSize(px(200), px(120));
	json children = json::array();
	for(int i = 0; i < 20; ++i) children.push_back("leaf" + std::to_string(i));
	const json items = json::array({json{{"id", "big"}, {"children", children}}, "after"});
	tree.setModel(std::make_shared<const ItemList>(readItems(items, true, "items")));
	tree.setExpanded({"big"});
	tree.setStickyRows(2);
	window->render();
	CHECK(tree.stuck().empty()); // not scrolled
	tree.scrollToRow(15);
	window->render();
	REQUIRE(tree.scrollPosition() > 0.0f);
	CHECK(tree.stuck() == std::vector<std::size_t>{0});
	tree.setStickyRows(0);
	CHECK(tree.stuck().empty());
	tree.setStickyRows(2);
	CHECK(tree.stuck() == std::vector<std::size_t>{0});
	tree.scrollToRow(0); // back at the top: nothing pinned
	window->render();
	CHECK(tree.stuck().empty());
}

TEST_CASE("list: once scrolled, items added above the top row keep it in place") {
	ManualClock clock;
	const auto window = createHeadlessWindow(200, 120, clock);
	auto& list = window->root().add<ListView>();
	list.setSize(px(200), px(120));
	const auto itemsFrom = [](int first, int last) {
		std::vector<ListItem> items;
		for(int i = first; i < last; ++i) {
			const std::string id = "i" + std::to_string(i);
			items.push_back(ListItem{id, {id}, 0, items.size() + 1, false});
		}
		return std::make_shared<const ItemList>(std::move(items));
	};
	list.setModel(itemsFrom(0, 20));
	window->render();
	list.scrollToRow(19);
	window->render();
	const std::string top = list.model()->item(list.itemAt(list.rowAt({100.0f, 1.0f}))).id;
	const float before = list.scrollPosition();
	list.setModel(itemsFrom(-3, 20));
	window->render();
	CHECK(list.model()->item(list.itemAt(list.rowAt({100.0f, 1.0f}))).id == top);
	CHECK(list.scrollPosition() == before + 3.0f * 24.0f);

	// Not scrolled: the top stays at the first item
	list.scrollToRow(0);
	window->render();
	list.setModel(itemsFrom(-6, 20));
	window->render();
	CHECK(list.scrollPosition() == 0.0f);
}

TEST_CASE("items: cells are read as text, empty for null, and too many are refused") {
	const auto items = readItems(
	    json::parse(R"([{ "id": "a", "cells": ["x", null, 3] }, { "id": "b", "label": "B" }])"), false, "items");
	REQUIRE(items.size() == 2);
	CHECK(items[0] == ListItem{"a", {"x", "", "3"}, 0, 1, false});
	CHECK(items[1].cells == std::vector<std::string>{"B"});
	CHECK_THROWS(static_cast<void>(readItems(json::parse(R"([{ "id": "a", "cells": [] }])"), false, "items")));
	CHECK_THROWS(static_cast<void>(readItems(json::parse(R"([{ "id": "a", "cells": "x" }])"), false, "items")));
	json many = json::array();
	for(int i = 0; i < 257; ++i) many.push_back(i);
	CHECK_THROWS(static_cast<void>(readItems(json::array({json{{"id", "a"}, {"cells", many}}}), false, "items")));
}

TEST_CASE("table node: columns and sort read, sort and resize events into state, bad columns and sort refused") {
	ManualClock clock;
	const auto window = createHeadlessWindow(240, 148, clock);
	auto screen = affresco::Screen::fromString(R"({
		"format": "affrescoui-screen", "version": 2,
		"state": { "sorted": "", "width": 0 },
		"root": { "id": "root", "children": [
			{ "type": "table", "id": "files", "width": 240, "height": 148,
			  "columns": ["Name", { "id": "size", "title": "Size", "width": 80 }],
			  "items": [{ "id": "b", "cells": ["beta", 2] }, { "id": "a", "cells": ["alpha", 1] }],
			  "sort": { "column": "size" },
			  "onSort": [{ "set": "state.sorted", "to": "{{event.column}}" }],
			  "onColumnResize": [{ "set": "state.width", "to": "{{event.width}}" }] }
		] }
	})");
	screen.build(*window);
	window->render();
	const auto state = [&screen] { return json::parse(screen.stateJson()); };

	auto* table = dynamic_cast<TableView*>(screen.find("files"));
	REQUIRE(table);
	CHECK(table->columns() == std::vector<TableColumn>{{"Name", "Name"}, {"size", "Size", 80.0f}});
	CHECK(shownIds(*table) == std::vector<std::string>{"a", "b"});
	click(*window, {60.0f, 14.0f});
	CHECK(state()["sorted"] == "Name");
	drag(*window, {120.0f, 14.0f}, {150.0f, 14.0f});
	CHECK(state()["width"] == 150);

	std::string error;
	CHECK_FALSE(screen.applyRevision(R"({"set": {"#files/columns": [{ "title": "no id" }]}})", &error));
	CHECK(error.find("id") != std::string::npos);
	CHECK_FALSE(screen.applyRevision(R"({"set": {"#files/sort": { "descending": true }}})", &error));
	CHECK(error.find("column") != std::string::npos);
	REQUIRE(screen.applyRevision(R"({"set": {"#files/sort": { "column": "size", "descending": true }}})", &error));
	window->render();
	CHECK(shownIds(*table) == std::vector<std::string>{"b", "a"});
}
