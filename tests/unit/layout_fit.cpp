/// @file
/// @brief Stacks that keep a padding and fit their children (Stack::setPadding(), Stack::setFit(), and the stack keys
/// "padding" and "fit"): their length follows the children as they change, within the stack's bounds.

#include "Headless.hpp"

#include <affresco/affresco.hpp>
#include <affresco/render/Stack.hpp>
#include <affresco/render/Window.hpp>

#include <doctest/doctest.h>

#include <vector>

using affresco::CrossAlign;
using affresco::Panel;
using affresco::px;
using affresco::Rect;
using affresco::Stack;
using affresco::Vec2;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief Adds a child of a given size.
/// @param parent The container.
/// @param width Its width.
/// @param height Its height.
/// @return The child.
Panel* child(Panel& parent, float width, float height) {
	Panel& panel = parent.add<Panel>();
	panel.setSize(px(width), px(height));
	return &panel;
}

/// @brief A column that fits its children, with a padding and a gap, stretching them across.
/// @param parent Where it goes.
/// @param padding Its padding.
/// @param gap Its gap.
/// @return The column.
Stack& fitColumn(Panel& parent, float padding, float gap) {
	Stack& column = parent.add<Stack>();
	column.setWidth(px(100));
	column.setFit(true);
	column.setPadding(padding);
	column.setGap(gap);
	column.setAlignItems(CrossAlign::Stretch);
	return column;
}

/// @brief A screen with a card that fits its rows, one of them shown only while the state says so.
constexpr const char* cardScreen = R"json({
	"format": "affrescoui-screen", "version": 2,
	"state": { "more": false },
	"actions": { "toggle": [{ "set": "state.more", "to": "{{!state.more}}" }] },
	"root": { "id": "root", "children": [
		{ "id": "card", "type": "column", "fit": true, "padding": 12, "gap": 4, "alignItems": "stretch", "width": 200,
		  "children": [
			{ "id": "first", "type": "label", "text": "First", "height": 20 },
			{ "id": "second", "type": "label", "text": "Second", "height": 30, "when": "{{state.more}}" }
		] }
	] }
})json";

} // namespace

TEST_CASE("layout: a fitting column is as tall as its children, its gaps and its padding") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(300, 300, clock);
	Stack& column = fitColumn(window->root(), 10.0f, 5.0f);
	const std::vector<Panel*> children{child(column, 0, 20), child(column, 0, 30)};
	window->render();
	CHECK(column.frame() == Rect{0, 0, 100, 75});
	CHECK(children[0]->frame() == Rect{10, 10, 90, 30}); // the padding on every side
	CHECK(children[1]->frame() == Rect{10, 35, 90, 65});

	// A child grows, then hides: the column follows
	children[0]->setHeight(px(40));
	window->render();
	CHECK(column.frame() == Rect{0, 0, 100, 95});
	children[1]->setVisible(false);
	window->render();
	CHECK(column.frame() == Rect{0, 0, 100, 60});

	// Without fit it keeps its own height again
	column.setFit(false);
	column.setHeight(px(50));
	window->render();
	CHECK(column.frame() == Rect{0, 0, 100, 50});
}

TEST_CASE("layout: a fitting stack stays within its bounds, and its children's minimums win") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(300, 300, clock);
	Stack& column = fitColumn(window->root(), 0.0f, 0.0f);
	Panel* first = child(column, 0, 20);
	first->setMinHeight(px(50));
	child(column, 0, 30);
	window->render();
	CHECK(column.frame().bottom == 80.0f);
	column.setMaxHeight(px(60));
	window->render();
	CHECK(column.frame().bottom == 60.0f);
	column.setMaxHeight(std::nullopt);
	column.setMinHeight(px(120));
	window->render();
	CHECK(column.frame().bottom == 120.0f);

	// A row fits along its width
	Stack& row = window->root().add<Stack>();
	row.setDirection(affresco::Direction::Row);
	row.setY(px(200));
	row.setHeight(px(20));
	row.setFit(true);
	row.setPadding(4.0f);
	row.setGap(6.0f);
	child(row, 30, 10);
	child(row, 50, 10);
	window->render();
	CHECK(row.frame().width() == 4.0f + 30.0f + 6.0f + 50.0f + 4.0f);
}

TEST_CASE("layout: fitting stacks inside each other all follow a change deep inside") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(300, 300, clock);
	Stack& outer = fitColumn(window->root(), 5.0f, 0.0f);
	Stack& inner = fitColumn(outer, 10.0f, 0.0f);
	Panel* leaf = child(inner, 0, 20);
	window->render();
	CHECK(inner.frame().height() == 40.0f);
	CHECK(outer.frame().height() == 50.0f);
	leaf->setHeight(px(50));
	window->render();
	CHECK(inner.frame().height() == 70.0f);
	CHECK(outer.frame().height() == 80.0f);
	CHECK(leaf->frame() == Rect{15, 15, 85, 65});
}

TEST_CASE("layout: a scrolling stack with a padding scrolls that far past its last child") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(300, 300, clock);
	Stack& column = fitColumn(window->root(), 10.0f, 10.0f);
	column.setMaxHeight(px(100));
	column.setScrollable(false, true);
	for(int i = 0; i < 3; ++i) child(column, 0, 40);
	window->render();
	CHECK(column.frame().height() == 100.0f);
	column.scrollBy(Vec2{0.0f, 1000.0f});
	CHECK(column.scrollOffset().y == 60.0f); // 10 + 3 × 40 + 2 × 10 + 10, less the 100 shown
}

TEST_CASE("format: a column with \"fit\" and \"padding\" grows with the rows it shows") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(400, 300, clock);
	affresco::Screen screen = affresco::Screen::fromString(cardScreen);
	screen.build(*window);
	window->render();
	Panel* card = screen.find("card");
	REQUIRE(card);
	CHECK(card->frame() == Rect{0, 0, 200, 44});
	CHECK(screen.find("first")->frame() == Rect{12, 12, 188, 32});
	screen.runAction("toggle", *card);
	window->render();
	CHECK(card->frame().height() == 12.0f + 20.0f + 4.0f + 30.0f + 12.0f);
	screen.runAction("toggle", *card);
	window->render();
	CHECK(card->frame().height() == 44.0f);
}
