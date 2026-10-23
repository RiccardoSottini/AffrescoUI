/// @file
/// @brief Rows, columns and grids laid out in a headless window: the flexible and grid properties of Stack, Grid and
/// Panel, with tables of the frames expected.

#include "Headless.hpp"

#include <affresco/render/Stack.hpp>
#include <affresco/render/Window.hpp>

#include <doctest/doctest.h>

#include <memory>
#include <vector>

using affresco::CrossAlign;
using affresco::Justify;
using affresco::Panel;
using affresco::px;
using affresco::Rect;
using affresco::Stack;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief Checks the frames of some panels.
/// @param panels The panels.
/// @param expected Their frames, in window units.
void checkFrames(const std::vector<Panel*>& panels, const std::vector<Rect>& expected) {
	REQUIRE(panels.size() == expected.size());
	for(std::size_t i = 0; i < panels.size(); ++i) {
		CAPTURE(i);
		CHECK(panels[i]->frame() == expected[i]);
	}
}

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

} // namespace

TEST_CASE("layout: a row spreads, aligns, shrinks and bounds its children") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(200, 100, clock);
	Stack& row = window->root().add<Stack>();
	row.setDirection(affresco::Direction::Row);
	row.setSize(px(200), px(40));
	row.setJustify(Justify::SpaceBetween);
	row.setAlignItems(CrossAlign::Center);
	const std::vector<Panel*> children{child(row, 40, 20), child(row, 40, 10), child(row, 40, 30)};
	children[1]->setAlignSelf(CrossAlign::Stretch);
	window->render();
	checkFrames(children, {{0, 10, 40, 30}, {80, 0, 120, 40}, {160, 5, 200, 35}});

	// Too wide: the shrinking ones give up the overflow, the bounded one stops at its maximum
	row.setJustify(Justify::Start);
	row.setGap(10.0f);
	for(Panel* panel : children) {
		panel->setWidth(px(100));
		panel->setShrink(1.0f);
	}
	children[0]->setMinWidth(px(80));
	window->render();
	checkFrames(children, {{0, 10, 80, 30}, {90, 0, 140, 40}, {150, 5, 200, 35}});
}

TEST_CASE("layout: a wrapping column starts new columns, a column gap apart") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(300, 100, clock);
	Stack& column = window->root().add<Stack>();
	column.setSize(px(300), px(100));
	column.setWrap(true);
	column.setRowGap(10.0f);
	column.setColumnGap(20.0f);
	const std::vector<Panel*> children{child(column, 50, 40), child(column, 70, 40), child(column, 50, 40)};
	window->render();
	checkFrames(children, {{0, 0, 50, 40}, {0, 50, 70, 90}, {90, 0, 140, 40}});
}

TEST_CASE("layout: a basis and a maximum size, and an aspect ratio") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(300, 300, clock);
	Stack& row = window->root().add<Stack>();
	row.setDirection(affresco::Direction::Row);
	row.setSize(px(300), px(100));
	Panel* based = child(row, 10, 10);
	based->setBasis(px(60));
	Panel* growing = child(row, 0, 10);
	growing->setGrow(1.0f);
	growing->setMaxWidth(px(100));
	Panel* square = child(row, 0, 0);
	square->setAspectRatio(2.0f);
	square->setAlignSelf(CrossAlign::Stretch); // as tall as the row, so twice as wide
	window->render();
	checkFrames({based, growing, square}, {{0, 0, 60, 10}, {60, 0, 100, 10}, {100, 0, 300, 100}});

	// Outside containers: the height follows the width, and a stretched width leads too
	Panel& free = window->root().add<Panel>();
	free.setPosition(px(0), px(120));
	free.setWidth(px(90));
	free.setAspectRatio(1.5f);
	Panel& stretched = window->root().add<Panel>();
	stretched.setAnchor(affresco::Edge::Left, px(10));
	stretched.setAnchor(affresco::Edge::Right, px(10));
	stretched.setY(px(200));
	stretched.setAspectRatio(28.0f);
	free.setMaxHeight(px(50));
	window->render();
	CHECK(free.frame() == Rect{0, 120, 90, 170});
	CHECK(stretched.frame() == Rect{10, 200, 290, 210});
}

TEST_CASE("layout: a grid with tracks, cells and spans") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(320, 300, clock);
	affresco::Grid& grid = window->root().add<affresco::Grid>();
	grid.setSize(px(320), px(300));
	grid.setColumns({affresco::Track::pixels(100), affresco::Track::fraction(1), affresco::Track::fraction(1)});
	grid.setRows({affresco::Track::pixels(50)});
	grid.setGap(10.0f);
	const std::vector<Panel*> children{child(grid, 0, 20), child(grid, 0, 30), child(grid, 0, 40), child(grid, 0, 60)};
	children[1]->setGridCell({.columnSpan = 2});
	children[3]->setGridCell({.column = 2, .row = 3});
	children[3]->setAlignSelf(CrossAlign::Stretch);
	window->render();
	// Row 1 is 50 px; row 2 as tall as its child (40); row 3 stretches its child to its own height (60)
	checkFrames(children, {{0, 0, 100, 20}, {110, 0, 320, 30}, {0, 60, 100, 100}, {110, 110, 210, 170}});

	grid.setColumns(2); // equal columns, as before tracks
	grid.setRows({});
	for(Panel* panel : children) panel->setGridCell({});
	children[3]->setAlignSelf(std::nullopt);
	window->render();
	checkFrames(children, {{0, 0, 155, 20}, {165, 0, 320, 30}, {0, 40, 155, 80}, {165, 40, 320, 100}});
}

TEST_CASE("layout: a stack fitted to its children takes their size, and follows them when they change") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(300, 100, clock);
	Stack& row = window->root().add<Stack>();
	row.setDirection(affresco::Direction::Row);
	row.setGap(5.0f);
	row.setHeight(px(50));
	row.setFitContent(true, false);
	Stack& column = row.add<Stack>();
	column.setFitContent(true, true);
	Panel* inner = child(column, 30, 10);
	Panel* last = child(column, 20, 15);
	Panel* side = child(row, 40, 20);
	window->render();
	checkFrames({&row, &column, inner, last, side},
	            {{0, 0, 75, 50}, {0, 0, 30, 25}, {0, 0, 30, 10}, {0, 10, 20, 25}, {35, 0, 75, 20}});

	// A grandchild grows: the column and the row around it grow with it, within the row's maximum
	inner->setWidth(px(60));
	row.setMaxWidth(px(100));
	window->render();
	checkFrames({&row, &column, side}, {{0, 0, 100, 50}, {0, 0, 60, 25}, {65, 0, 105, 20}});

	// A hidden child takes no room
	inner->setVisible(false);
	window->render();
	checkFrames({&row, &column}, {{0, 0, 65, 50}, {0, 0, 20, 15}});
}
