/// @file
/// @brief Lists and trees on a headless window: the row window, fixed and measured; a list's clicks, selection
/// models, keyboard, type-to-find and drop targets; a tree's expanding by keyboard and chevron; reading items within
/// limits; and both as screen nodes, their selection and expanded items bound to state.
///
/// Rows are 24 pixels high, so row r spans y = 24 r to 24 (r + 1) in a view at the window's top.

#include "Headless.hpp"
#include "ListItems.hpp"

#include <affresco/affresco.hpp>
#include <affresco/render/ListView.hpp>
#include <affresco/render/RowWindow.hpp>
#include <affresco/render/TreeView.hpp>
#include <affresco/render/Window.hpp>

#include <doctest/doctest.h>

#include <memory>
#include <string>
#include <vector>

using affresco::Action;
using affresco::DropPlace;
using affresco::DropTarget;
using affresco::ExpandChange;
using affresco::ItemList;
using affresco::json;
using affresco::Key;
using affresco::ListItem;
using affresco::ListView;
using affresco::Mods;
using affresco::MouseButton;
using affresco::px;
using affresco::RowRange;
using affresco::RowWindow;
using affresco::SelectionMode;
using affresco::TreeView;
using affresco::Vec2;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;
using affresco::format::readItems;

namespace {

constexpr float rowHeight = 24.0f; ///< A row's default height.

/// @brief The middle of a row, away from a tree's chevron.
/// @param row The row.
/// @return The point.
[[nodiscard]] Vec2 rowPoint(std::size_t row) { return {100.0f, static_cast<float>(row) * rowHeight + 12.0f}; }

/// @brief A model of plain items, each its own id and label.
/// @param ids The ids.
/// @return The model.
[[nodiscard]] std::shared_ptr<const ItemList> plainItems(const std::vector<std::string>& ids) {
	std::vector<ListItem> items;
	for(const std::string& id : ids) items.push_back(ListItem{id, {id}, 0, items.size() + 1, false});
	return std::make_shared<const ItemList>(std::move(items));
}

/// @brief What a list or a tree fixture shares: the window, its clock, and the signals caught.
struct ViewFixture {
	ManualClock clock;                                ///< The window's clock.
	std::unique_ptr<affresco::Window> window;         ///< 200 by 120: five rows.
	std::vector<std::vector<std::string>> selections; ///< Every onSelectionChanged, oldest first.
	std::vector<std::string> opened;                  ///< Every onOpened.

	/// @brief Opens the window.
	ViewFixture() : window(createHeadlessWindow(200, 120, clock)) {}

	/// @brief Catches a view's signals.
	/// @param view The view.
	void listen(ListView& view) {
		view.setSize(px(200), px(120));
		view.onSelectionChanged.connect([this](const std::vector<std::string>& ids) { selections.push_back(ids); });
		view.onOpened.connect([this](const std::string& id) { opened.push_back(id); });
		window->render();
	}

	/// @brief Presses and releases the left button.
	/// @param position Where.
	/// @param mods The modifier keys held.
	void click(Vec2 position, Mods mods = {}) const {
		window->routeMouseButton({MouseButton::Left, Action::Press, mods, position});
		window->routeMouseButton({MouseButton::Left, Action::Release, mods, position});
		window->render();
	}

	/// @brief Presses a key on the focused panel.
	/// @param key The key.
	/// @param mods The modifier keys held.
	void press(Key key, Mods mods = {}) const {
		window->routeKey({key, 0, Action::Press, mods});
		window->render();
	}

	/// @brief Types a character on the focused panel.
	/// @param codepoint The character.
	void type(char32_t codepoint) const {
		window->routeChar({codepoint});
		window->render();
	}
};

/// @brief A list of ten fruits filling the window.
struct ListFixture : ViewFixture {
	ListView* list = nullptr; ///< The list.

	/// @brief Builds it.
	ListFixture() {
		list = &window->root().add<ListView>();
		list->setModel(plainItems(
		    {"apple", "banana", "cherry", "date", "elder", "fig", "grape", "honeydew", "kiwi", "lemon"}));
		listen(*list);
	}
};

/// @brief A tree: src (main, lib (a, b)), README; all closed.
struct TreeFixture : ViewFixture {
	TreeView* tree = nullptr;          ///< The tree.
	std::vector<ExpandChange> changes; ///< Every onExpandedChanged.

	/// @brief Builds it.
	TreeFixture() {
		tree = &window->root().add<TreeView>();
		const json items = json::parse(R"([{ "id": "src", "children": ["main", { "id": "lib", "children": ["a", "b"] }] },
			"README"])");
		tree->setModel(std::make_shared<const ItemList>(readItems(items, true, "items")));
		tree->onExpandedChanged.connect([this](const ExpandChange& change) { changes.push_back(change); });
		listen(*tree);
	}

	/// @brief The id of the item under the cursor.
	[[nodiscard]] std::string cursorId() const { return tree->model()->item(tree->cursor()).id; }
};

} // namespace

TEST_CASE("row window: fixed rows cost nothing to place, and measured rows move the rows after them") {
	RowWindow rows;
	rows.reset(20, 20.0f);
	CHECK_FALSE(rows.measured());
	CHECK(rows.total() == 400.0f);
	CHECK(rows.offsetOf(7) == 140.0f);
	CHECK(rows.rowAt(139.0f) == 6);
	CHECK(rows.rowAt(1000.0f) == 19); // clamped to the last
	CHECK(rows.visible(100.0f, 60.0f, 2) == RowRange{3, 11});
	CHECK(rows.visible(0.0f, 60.0f, 2) == RowRange{0, 6});

	rows.reset(10, 20.0f);
	rows.setHeight(3, 50.0f);
	CHECK(rows.measured());
	CHECK(rows.heightOf(3) == 50.0f);
	CHECK(rows.offsetOf(4) == 110.0f);
	CHECK(rows.offsetOf(10) == 230.0f);
	CHECK(rows.rowAt(65.0f) == 3);
	CHECK(rows.rowAt(110.0f) == 4);
	rows.setHeight(10, 99.0f); // past the end: ignored
	CHECK(rows.total() == 230.0f);

	rows.reset(0, 20.0f);
	CHECK(rows.visible(0.0f, 100.0f, 2) == RowRange{});
}

TEST_CASE("list: a click selects, Shift selects a range, Cmd toggles, and a double click opens") {
	ListFixture fixture;
	fixture.click(rowPoint(1));
	CHECK(fixture.list->selection() == std::vector<std::string>{"banana"});
	fixture.clock.advance(1000.0);
	fixture.click(rowPoint(3), Mods{0x0001}); // single: Shift is a plain click
	CHECK(fixture.list->selection() == std::vector<std::string>{"date"});

	fixture.list->setSelectionMode(SelectionMode::Multiple);
	fixture.clock.advance(1000.0);
	fixture.click(rowPoint(0));
	fixture.clock.advance(1000.0);
	fixture.click(rowPoint(2), Mods{0x0001});
	CHECK(fixture.list->selection() == std::vector<std::string>{"apple", "banana", "cherry"});
	fixture.clock.advance(1000.0);
	fixture.click(rowPoint(1), Mods{0x0008});
	CHECK(fixture.list->selection() == std::vector<std::string>{"apple", "cherry"});
	CHECK(fixture.selections.size() == 5);

	fixture.clock.advance(1000.0);
	fixture.click(rowPoint(4));
	fixture.clock.advance(100.0);
	fixture.click(rowPoint(4));
	CHECK(fixture.opened == std::vector<std::string>{"elder"});
	CHECK(fixture.list->rowAt({100.0f, 130.0f}) == fixture.list->rowCount()); // outside the list

	// None: the cursor moves, nothing is selected
	fixture.list->setSelectionMode(SelectionMode::None);
	fixture.list->setSelection({});
	fixture.clock.advance(1000.0);
	fixture.click(rowPoint(2));
	CHECK(fixture.list->selection().empty());
	CHECK(fixture.list->cursor() == 2);
}

TEST_CASE("list: the keys move the cursor and scroll it into view, and Enter opens") {
	ListFixture fixture;
	fixture.window->setFocus(fixture.list);
	fixture.press(Key::Down); // no cursor yet: the first row
	CHECK(fixture.list->cursor() == 0);
	fixture.press(Key::End);
	CHECK(fixture.list->cursor() == 9);
	CHECK(fixture.list->scrollPosition() == 10.0f * rowHeight - 120.0f);
	fixture.press(Key::Home);
	CHECK(fixture.list->scrollPosition() == 0.0f);
	fixture.press(Key::Down);
	fixture.press(Key::PageDown); // a page is five rows
	CHECK(fixture.list->cursor() == 6);
	CHECK(fixture.list->scrollPosition() == 7.0f * rowHeight - 120.0f);
	CHECK(fixture.list->rowAt({100.0f, 119.0f}) == 6);
	fixture.press(Key::Enter);
	CHECK(fixture.opened == std::vector<std::string>{"grape"});
	CHECK(fixture.list->selection() == std::vector<std::string>{"grape"});

	fixture.list->setSelectionMode(SelectionMode::Multiple);
	fixture.press(Key::Up, Mods{0x0001});
	fixture.press(Key::Up, Mods{0x0001});
	CHECK(fixture.list->selection() == std::vector<std::string>{"elder", "fig", "grape"});
}

TEST_CASE("list: typing jumps to the next label that starts with what was typed in the last second") {
	ListFixture fixture;
	fixture.window->setFocus(fixture.list);
	fixture.type(U'C');
	CHECK(fixture.list->cursor() == 2);
	fixture.type(U'h');
	CHECK(fixture.list->cursor() == 2); // "ch": still cherry
	fixture.clock.advance(1100.0);
	fixture.type(U'h'); // a new search, after the cursor
	CHECK(fixture.list->cursor() == 7);
	fixture.clock.advance(1100.0);
	fixture.type(U'c'); // wraps around
	CHECK(fixture.list->cursor() == 2);
	fixture.clock.advance(1100.0);
	fixture.type(U'z'); // nothing: the cursor stays
	CHECK(fixture.list->cursor() == 2);
}

TEST_CASE("list: new items keep the selection and the cursor by id, and drops land between rows") {
	ListFixture fixture;
	fixture.list->setSelection({"cherry", "banana", "cherry"});
	CHECK(fixture.list->selection() == std::vector<std::string>{"banana", "cherry"});
	CHECK(fixture.list->isSelected("cherry"));
	fixture.window->setFocus(fixture.list);
	fixture.type(U'f');
	REQUIRE(fixture.list->cursor() == 5);

	fixture.list->setModel(plainItems({"fig", "banana", "kiwi"}));
	fixture.window->render();
	CHECK(fixture.list->selection() == std::vector<std::string>{"banana"});
	CHECK(fixture.list->cursor() == 0);
	CHECK(fixture.list->rowCount() == 3);
	CHECK(fixture.selections.empty()); // only the user's changes are reported

	CHECK(fixture.list->dropTargetAt({100.0f, 26.0f}) == DropTarget{1, DropPlace::Before});
	CHECK(fixture.list->dropTargetAt({100.0f, 44.0f}) == DropTarget{1, DropPlace::After});
	CHECK(fixture.list->dropTargetAt({100.0f, 110.0f}) == DropTarget{3, DropPlace::Before}); // past the last

	fixture.list->setModel(nullptr);
	fixture.window->render();
	CHECK(fixture.list->rowCount() == 0);
	fixture.press(Key::Down); // nothing to move to
	CHECK(fixture.list->selection().empty());
}

TEST_CASE("tree: Right opens and goes in, Left closes and goes up, and a chevron toggles") {
	TreeFixture fixture;
	CHECK(fixture.tree->rowCount() == 2);
	fixture.window->setFocus(fixture.tree);
	fixture.press(Key::Down);
	fixture.press(Key::Right);
	CHECK(fixture.tree->rowCount() == 4);
	REQUIRE(fixture.changes.size() == 1);
	CHECK(fixture.changes[0].id == "src");
	CHECK(fixture.changes[0].expanded);
	fixture.press(Key::Right);
	CHECK(fixture.cursorId() == "main");
	fixture.press(Key::Down);
	fixture.press(Key::Right);
	CHECK(fixture.tree->rowCount() == 6);
	fixture.press(Key::Right);
	CHECK(fixture.cursorId() == "a");
	fixture.press(Key::Left); // a leaf: up to its parent
	CHECK(fixture.cursorId() == "lib");
	fixture.press(Key::Left);
	CHECK(fixture.tree->rowCount() == 4);
	fixture.press(Key::Left);
	CHECK(fixture.cursorId() == "src");
	CHECK(fixture.changes.size() == 3);

	// The chevron of src (depth 0) is at the row's padding; a click there toggles without selecting
	fixture.clock.advance(1000.0);
	fixture.click({12.0f, 12.0f});
	CHECK(fixture.tree->rowCount() == 2);
	CHECK_FALSE(fixture.tree->isExpanded(0));
	fixture.clock.advance(1000.0);
	fixture.click(rowPoint(1));
	CHECK(fixture.tree->selection() == std::vector<std::string>{"README"});
	CHECK(fixture.tree->dropTargetAt({100.0f, 36.0f}) == DropTarget{1, DropPlace::Inside});
}

TEST_CASE("tree: the expanded ids given show those subtrees, and new items keep them") {
	TreeFixture fixture;
	fixture.tree->setExpanded({"src", "lib"});
	fixture.window->render();
	CHECK(fixture.tree->rowCount() == 6);
	fixture.tree->setExpanded({"lib"}); // lib is open, but inside closed src
	fixture.window->render();
	CHECK(fixture.tree->rowCount() == 2);
	CHECK(fixture.tree->rowOf(3) == fixture.tree->rowCount()); // a is hidden
	fixture.tree->setExpanded({"src", "lib"});
	fixture.tree->expand(1, true); // main is a leaf: nothing
	fixture.window->render();
	CHECK(fixture.tree->rowCount() == 6);
	CHECK(fixture.changes.empty());
}

TEST_CASE("items: a tree is read in pre-order, within limits, and bad items are refused") {
	const json items = json::parse(R"([{ "id": "src", "label": "Sources", "children": ["main", { "id": "lib",
		"children": [1, 2] }] }, { "id": "empty", "children": [], "expandable": true }, "README"])");
	const auto tree = readItems(items, true, "items");
	REQUIRE(tree.size() == 7);
	CHECK(tree[0] == ListItem{"src", {"Sources"}, 0, 5, true});
	CHECK(tree[2] == ListItem{"lib", {"lib"}, 1, 5, true});
	CHECK(tree[3] == ListItem{"1", {"1"}, 2, 4, false});
	CHECK(tree[5] == ListItem{"empty", {"empty"}, 0, 6, true});
	CHECK(readItems(items, false, "items").size() == 3); // a list ignores children

	CHECK_THROWS(static_cast<void>(readItems(json("x"), false, "items")));
	CHECK_THROWS(static_cast<void>(readItems(json::parse(R"([{ "label": "no id" }])"), false, "items")));
	CHECK_THROWS(static_cast<void>(readItems(json::parse(R"([true])"), false, "items")));
	CHECK_THROWS(static_cast<void>(readItems(json::parse(R"([{ "id": "a", "children": "b" }])"), true, "items")));

	// Deeper than 256 levels is refused, without recursing
	json deep = json{{"id", "leaf"}};
	for(int level = 0; level < 300; ++level) deep = json{{"id", "n" + std::to_string(level)}, {"children", json::array({deep})}};
	CHECK_THROWS(static_cast<void>(readItems(json::array({deep}), true, "items")));
	CHECK(readItems(json::array({deep}), false, "items").size() == 1);

	CHECK(affresco::format::readIds(json::parse(R"(["a", 2, null, true])"), "ids") == std::vector<std::string>{"a", "2"});
	CHECK(affresco::format::readIds(json(nullptr), "ids").empty());
	CHECK_THROWS(static_cast<void>(affresco::format::readIds(json("a"), "ids")));
}

TEST_CASE("list and tree nodes: selection and expanded items bound to state, open and expand events, bad keys refused") {
	ManualClock clock;
	const auto window = createHeadlessWindow(200, 210, clock);
	auto screen = affresco::Screen::fromString(R"({
		"format": "affrescoui-screen", "version": 1,
		"state": { "files": [{ "id": "src", "children": ["main", "lib"] }, "README"], "open": [], "picked": null,
		           "opened": "", "many": ["y"] },
		"root": { "id": "root", "children": [
			{ "type": "tree", "id": "files", "width": 200, "height": 120, "items": "{{state.files}}",
			  "expanded": "{{state.open}}", "bind": "state.picked",
			  "onExpand": [{ "set": "state.open",
			                 "to": "{{event.expanded ? concat(state.open, [event.id]) : filter(state.open, it != event.id)}}" }],
			  "onOpen": [{ "set": "state.opened", "to": "{{event.id}}" }] },
			{ "type": "list", "id": "picks", "y": 130, "width": 200, "height": 72, "items": ["x", "y", "z"],
			  "selection": "multiple", "bind": "state.many" }
		] }
	})");
	screen.build(*window);
	window->render();
	const auto state = [&screen] { return json::parse(screen.stateJson()); };

	auto* tree = dynamic_cast<TreeView*>(screen.find("files"));
	REQUIRE(tree);
	CHECK(tree->rowCount() == 2);
	window->routeMouseButton({MouseButton::Left, Action::Press, {}, {12.0f, 12.0f}});
	window->routeMouseButton({MouseButton::Left, Action::Release, {}, {12.0f, 12.0f}});
	window->render();
	CHECK(state()["open"] == json{"src"});
	CHECK(tree->rowCount() == 4);

	clock.advance(1000.0);
	window->routeMouseButton({MouseButton::Left, Action::Press, {}, rowPoint(1)});
	window->routeMouseButton({MouseButton::Left, Action::Release, {}, rowPoint(1)});
	window->render();
	CHECK(state()["picked"] == "main");
	window->setFocus(tree);
	window->routeKey({Key::Enter, 0, Action::Press, {}});
	window->render();
	CHECK(state()["opened"] == "main");

	auto* list = dynamic_cast<ListView*>(screen.find("picks"));
	REQUIRE(list);
	CHECK(list->selection() == std::vector<std::string>{"y"});
	window->routeMouseButton({MouseButton::Left, Action::Press, Mods{0x0008}, {50.0f, 142.0f}});
	window->routeMouseButton({MouseButton::Left, Action::Release, Mods{0x0008}, {50.0f, 142.0f}});
	window->render();
	CHECK(state()["many"] == json{"x", "y"});

	std::string error;
	CHECK_FALSE(screen.applyRevision(R"({"set": {"#files/selection": "some"}})", &error));
	CHECK(error.find("selection") != std::string::npos);
	CHECK_FALSE(screen.applyRevision(R"({"set": {"#picks/items": [{ "label": "no id" }]}})", &error));
	CHECK(error.find("id") != std::string::npos);
	REQUIRE(screen.applyRevision(R"({"set": {"#picks/items": ["x", "z"]}})", &error));
	window->render();
	CHECK(list->rowCount() == 2);
}
