/// @file
/// @brief Flexible and grid layout as pure functions (render's layout/Flex and layout/Grid): tables of sizes in,
/// tables of expected rectangles out.

#include "layout/Flex.hpp"
#include "layout/Grid.hpp"

#include <affresco/render/Geometry.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <chrono>
#include <initializer_list>
#include <limits>
#include <vector>

using affresco::CrossAlign;
using affresco::Justify;
using affresco::Rect;
using affresco::Track;
using affresco::TrackSize;
using affresco::TrackUnit;
using affresco::detail::FlexItem;
using affresco::detail::FlexOptions;
using affresco::detail::FlexPlacement;

namespace {

constexpr float none = std::numeric_limits<float>::infinity(); ///< No maximum.

/// @brief Lays a row out and turns each placement into the rectangle a Stack would give the child.
/// @param options The row.
/// @param items Its children.
/// @return Their rectangles, relative to the row; a child the row doesn't place across sits at the top.
std::vector<Rect> rowRects(const FlexOptions& options, const std::vector<FlexItem>& items) {
	std::vector<FlexPlacement> placements;
	affresco::detail::layoutFlex(options, items, placements);
	std::vector<Rect> rects;
	for(std::size_t i = 0; i < items.size(); ++i) {
		const FlexPlacement& p = placements[i];
		const float width = p.mainSize.value_or(std::max(std::min(items[i].base, items[i].maxMain), items[i].minMain));
		const float left = p.fromEnd ? options.main - p.main - width : p.main;
		const float height = p.crossSize.value_or(items[i].cross);
		const float top = p.crossOffset.value_or(0.0f);
		rects.push_back({left, top, left + width, top + height});
	}
	return rects;
}

/// @brief A child of a given width and height.
/// @param width Its own width.
/// @param height Its own height.
/// @return The child.
FlexItem item(float width, float height = 10.0f) { return {.base = width, .cross = height}; }

/// @brief One row of a table: the container, its children and the rectangles expected.
struct FlexCase {
	const char* name;            ///< What it shows.
	FlexOptions options;         ///< The row.
	std::vector<FlexItem> items; ///< Its children.
	std::vector<Rect> expected;  ///< Their rectangles.
};

/// @brief Checks each case of a table.
/// @param cases The table.
void checkFlex(const std::vector<FlexCase>& cases) {
	for(const FlexCase& c : cases) {
		CAPTURE(c.name);
		const std::vector<Rect> rects = rowRects(c.options, c.items);
		REQUIRE(rects.size() == c.expected.size());
		for(std::size_t i = 0; i < rects.size(); ++i) {
			CAPTURE(i);
			CHECK(rects[i].left == doctest::Approx(c.expected[i].left));
			CHECK(rects[i].top == doctest::Approx(c.expected[i].top));
			CHECK(rects[i].right == doctest::Approx(c.expected[i].right));
			CHECK(rects[i].bottom == doctest::Approx(c.expected[i].bottom));
		}
	}
}

/// @brief A child that grows.
/// @param width Its own width.
/// @param grow Its share.
/// @param maximum Its maximum width.
/// @return The child.
FlexItem growing(float width, float grow, float maximum = none) {
	return {.base = width, .maxMain = maximum, .cross = 10.0f, .grow = grow};
}

/// @brief A child that shrinks.
/// @param width Its own width.
/// @param shrink Its factor.
/// @param minimum Its minimum width.
/// @return The child.
FlexItem shrinking(float width, float shrink, float minimum = 0.0f) {
	return {.base = width, .minMain = minimum, .cross = 10.0f, .shrink = shrink};
}

/// @brief A child placed across.
/// @param width Its own width.
/// @param height Its own height.
/// @param align Where it sits.
/// @return The child.
FlexItem aligned(float width, float height, CrossAlign align) {
	return {.base = width, .cross = height, .align = align};
}

} // namespace

TEST_CASE("layout: a row packs its children by its justification") {
	const auto row = [](Justify justify) {
		return FlexOptions{.main = 100.0f, .cross = 20.0f, .gap = 10.0f, .justify = justify};
	};
	checkFlex({
	    {"start", row(Justify::Start), {item(20), item(30)}, {{0, 0, 20, 10}, {30, 0, 60, 10}}},
	    {"center", row(Justify::Center), {item(20), item(30)}, {{20, 0, 40, 10}, {50, 0, 80, 10}}},
	    {"end", row(Justify::End), {item(20), item(30)}, {{40, 0, 60, 10}, {70, 0, 100, 10}}},
	    {"space between",
		 row(Justify::SpaceBetween),
		 {item(20), item(20), item(20)},
		 {{0, 0, 20, 10}, {40, 0, 60, 10}, {80, 0, 100, 10}}},
	    {"space around",
		 row(Justify::SpaceAround),
		 {item(20), item(20)},
		 {{12.5f, 0, 32.5f, 10}, {67.5f, 0, 87.5f, 10}}},
	    {"space evenly",
		 row(Justify::SpaceEvenly),
		 {item(20), item(20)},
		 {{16.6667f, 0, 36.6667f, 10}, {63.3333f, 0, 83.3333f, 10}}},
	    {"centre that overflows starts at the start",
		 row(Justify::Center),
		 {item(80), item(80)},
		 {{0, 0, 80, 10}, {90, 0, 170, 10}}},
	    {"end that overflows is cut at the start",
		 row(Justify::End),
		 {item(80), item(80)},
		 {{-70, 0, 10, 10}, {20, 0, 100, 10}}},
	    {"one child between", row(Justify::SpaceBetween), {item(20)}, {{0, 0, 20, 10}}},
	    {"no children", row(Justify::Center), {}, {}},
	});
}

TEST_CASE("layout: growing children share the room left over, within their maximum") {
	const FlexOptions row{.main = 100.0f, .cross = 10.0f, .gap = 10.0f};
	checkFlex({
	    {"one grows", row, {item(20), growing(20, 1)}, {{0, 0, 20, 10}, {30, 0, 100, 10}}},
	    {"by shares", row, {growing(0, 1), growing(0, 3)}, {{0, 0, 22.5f, 10}, {32.5f, 0, 100, 10}}},
	    {"a maximum freezes it, the others take the rest",
		 row,
		 {growing(0, 1, 20), growing(0, 1)},
		 {{0, 0, 20, 10}, {30, 0, 100, 10}}},
	    {"every maximum reached leaves room",
		 row,
		 {growing(0, 1, 20), growing(0, 1, 30)},
		 {{0, 0, 20, 10}, {30, 0, 60, 10}}},
	    {"nothing to share when full", row, {growing(60, 1), item(40)}, {{0, 0, 60, 10}, {70, 0, 110, 10}}},
	});
}

TEST_CASE("layout: an overflow is taken from shrinking children in proportion to their size, within their minimum") {
	const FlexOptions row{.main = 100.0f, .cross = 10.0f};
	checkFlex({
	    {"by size", row, {shrinking(100, 1), shrinking(50, 1)}, {{0, 0, 66.6667f, 10}, {66.6667f, 0, 100, 10}}},
	    {"by factor", row, {shrinking(80, 1), shrinking(80, 3)}, {{0, 0, 65, 10}, {65, 0, 100, 10}}},
	    {"a minimum freezes it", row, {shrinking(80, 1, 70), shrinking(80, 1)}, {{0, 0, 70, 10}, {70, 0, 100, 10}}},
	    {"not shrinking: it overflows", row, {item(80), item(80)}, {{0, 0, 80, 10}, {80, 0, 160, 10}}},
	    {"bounds, then shrink",
		 row,
		 {{.base = 200, .maxMain = 150, .cross = 10, .shrink = 1}, shrinking(50, 1)},
		 {{0, 0, 75, 10}, {75, 0, 100, 10}}},
	});
}

TEST_CASE("layout: children are placed across a row by their alignment") {
	const FlexOptions row{.main = 100.0f, .cross = 40.0f};
	checkFlex({
	    {"start", row, {aligned(10, 10, CrossAlign::Start)}, {{0, 0, 10, 10}}},
	    {"center", row, {aligned(10, 10, CrossAlign::Center)}, {{0, 15, 10, 25}}},
	    {"end", row, {aligned(10, 10, CrossAlign::End)}, {{0, 30, 10, 40}}},
	    {"stretch", row, {aligned(10, 10, CrossAlign::Stretch)}, {{0, 0, 10, 40}}},
	    {"stretch within the maximum",
		 row,
		 {{.base = 10, .cross = 10, .maxCross = 25, .align = CrossAlign::Stretch}},
		 {{0, 0, 10, 25}}},
	    {"its own place", row, {item(10, 10)}, {{0, 0, 10, 10}}},
	});
	std::vector<FlexPlacement> placements;
	const std::vector<FlexItem> own{item(10)};
	affresco::detail::layoutFlex(row, own, placements);
	CHECK_FALSE(placements[0].crossOffset); // a child without an alignment keeps its own anchors across
}

TEST_CASE("layout: a wrapping row starts a new line when a child doesn't fit, lines a line gap apart") {
	const FlexOptions wrap{.main = 100.0f, .cross = 200.0f, .gap = 10.0f, .lineGap = 5.0f, .wrap = true};
	checkFlex({
	    {"lines", wrap, {item(40, 10), item(40, 20), item(40, 10)}, {{0, 0, 40, 10}, {50, 0, 90, 20}, {0, 25, 40, 35}}},
	    {"exactly full stays on the line",
		 wrap,
		 {item(45), item(45), item(100)},
		 {{0, 0, 45, 10}, {55, 0, 100, 10}, {0, 15, 100, 25}}},
	    {"too wide alone takes its own line", wrap, {item(150), item(20)}, {{0, 0, 150, 10}, {0, 15, 20, 25}}},
	    {"each line grows",
		 wrap,
		 {growing(40, 1), item(40), growing(40, 1)},
		 {{0, 0, 50, 10}, {60, 0, 100, 10}, {0, 15, 100, 25}}},
	    {"centred in its line",
		 wrap,
		 {item(40, 30), aligned(40, 10, CrossAlign::Center)},
		 {{0, 0, 40, 30}, {50, 10, 90, 20}}},
	    {"centred lines",
		 {.main = 100, .cross = 50, .gap = 10, .lineGap = 5, .justify = Justify::Center, .wrap = true},
		 {item(60), item(60)},
		 {{20, 0, 80, 10}, {20, 15, 80, 25}}},
	});
}

TEST_CASE("layout: a basis gives the child its size, flexed or not") {
	std::vector<FlexPlacement> placements;
	const std::vector<FlexItem> items{{.base = 30, .cross = 10, .sized = true}, item(20)};
	affresco::detail::layoutFlex({.main = 100.0f, .cross = 10.0f}, items, placements);
	CHECK(placements[0].mainSize == 30.0f);
	CHECK_FALSE(placements[1].mainSize); // its own size: the row gives it nothing
}

TEST_CASE("layout: grid children take the next free cells, row after row, around those placed by hand") {
	using affresco::GridCell;
	using affresco::detail::GridArea;
	struct Case {
		const char* name;
		std::size_t columns;
		std::vector<GridCell> cells;
		std::vector<GridArea> expected;
		std::size_t rows;
	};
	const std::vector<Case> cases{
	    {"in order", 2, {{}, {}, {}}, {{0, 0, 1, 1}, {1, 0, 1, 1}, {0, 1, 1, 1}}, 2},
	    {"spans", 3, {{.columnSpan = 2}, {.columnSpan = 2}, {}}, {{0, 0, 2, 1}, {0, 1, 2, 1}, {2, 1, 1, 1}}, 2},
	    {"placed first, the others around", 2, {{}, {.column = 1, .row = 1}}, {{1, 0, 1, 1}, {0, 0, 1, 1}}, 1},
	    {"a row only", 3, {{.row = 2}, {}}, {{0, 1, 1, 1}, {0, 0, 1, 1}}, 2},
	    {"a column only", 2, {{.column = 2}, {.column = 2}}, {{1, 0, 1, 1}, {1, 1, 1, 1}}, 2},
	    {"a row span", 2, {{.rowSpan = 2}, {}, {}}, {{0, 0, 1, 2}, {1, 0, 1, 1}, {1, 1, 1, 1}}, 2},
	    {"too wide is cut to the grid", 2, {{.column = 2, .columnSpan = 5}}, {{0, 0, 2, 1}}, 1},
	    {"the cursor never goes back to a gap",
		 3,
		 {{}, {.columnSpan = 3}, {}},
		 {{0, 0, 1, 1}, {0, 1, 3, 1}, {0, 2, 1, 1}},
		 3},
	};
	for(const Case& c : cases) {
		CAPTURE(c.name);
		std::vector<GridArea> areas;
		CHECK(affresco::detail::placeGridItems(c.columns, c.cells, areas) == c.rows);
		CHECK(areas == c.expected);
	}
}

TEST_CASE("layout: a big grid child among taken rows is placed in a moment, not minutes") {
	using affresco::GridCell;
	using affresco::detail::GridArea;
	// Two full rows leave no room for a 500 by 500 child: every position is tried, and each try once took the
	// whole free window again (about 28 seconds for these three children).
	const std::vector<GridCell> cells{{.column = 1, .row = 500, .columnSpan = 1000},
	                                  {.column = 1, .row = 1000, .columnSpan = 1000},
	                                  {.columnSpan = 500, .rowSpan = 500}};
	std::vector<GridArea> areas;
	const auto start = std::chrono::steady_clock::now();
	CHECK(affresco::detail::placeGridItems(1000, cells, areas) == 1000);
	CHECK(std::chrono::steady_clock::now() - start < std::chrono::seconds(2));
	CHECK(areas[2] == GridArea{0, 0, 500, 500}); // no room: it overlaps where it would have started
}

TEST_CASE("layout: grid tracks: pixels, percentages, fractions, content and minmax") {
	using affresco::detail::TrackContent;
	const auto px = [](float v) { return TrackSize{v, TrackUnit::Pixels}; };
	const auto fr = [](float v) { return TrackSize{v, TrackUnit::Fraction}; };
	const TrackSize content{0.0f, TrackUnit::Content};
	struct Case {
		const char* name;
		std::vector<Track> tracks;
		std::size_t count;
		std::vector<TrackContent> contents;
		float available;
		std::vector<float> expected;
	};
	const std::vector<Case> cases{
	    {"equal fractions", {Track::fraction(1), Track::fraction(1), Track::fraction(1)}, 3, {}, 320, {100, 100, 100}},
	    {"fractions by share", {Track::fraction(1), Track::fraction(3)}, 2, {}, 410, {100, 300}},
	    {"pixels and a fraction", {Track::pixels(100), Track::fraction(1)}, 2, {}, 310, {100, 200}},
	    {"a percentage", {Track::percent(25), Track::fraction(1)}, 2, {}, 410, {102.5f, 297.5f}},
	    {"content is its largest child",
		 {Track::content(), Track::fraction(1)},
		 2,
		 {{0, 1, 40}, {0, 1, 70}, {1, 1, 500}},
		 210,
		 {70, 130}},
	    {"implicit tracks are content", {}, 2, {{0, 1, 30}, {1, 1, 50}}, 1000, {30, 50}},
	    {"a fraction is minmax(0, fr): content doesn't widen it",
		 {Track::fraction(1), Track::fraction(1)},
		 2,
		 {{0, 1, 150}},
		 210,
		 {100, 100}},
	    {"minmax(auto, 1fr) keeps its content",
		 {Track::between(content, fr(1)), Track::fraction(1)},
		 2,
		 {{0, 1, 150}},
		 210,
		 {150, 50}},
	    {"minmax(px, px) grows to its maximum",
		 {Track::between(px(50), px(80)), Track::fraction(1)},
		 2,
		 {},
		 210,
		 {80, 120}},
	    {"minmax(px, content) grows towards its content",
		 {Track::between(px(20), content)},
		 1,
		 {{0, 1, 60}},
		 100,
		 {60}},
	    {"a span widens the content tracks it spans",
		 {Track::content(), Track::content()},
		 2,
		 {{0, 1, 10}, {1, 1, 10}, {0, 2, 70}},
		 1000,
		 {30, 30}},
	    {"fractions never shrink below their minimum",
		 {Track::between(px(100), fr(1)), Track::fraction(1)},
		 2,
		 {},
		 110,
		 {100, 0}},
	    {"overflowing fixed tracks", {Track::pixels(80), Track::pixels(80)}, 2, {}, 100, {80, 80}},
	};
	for(const Case& c : cases) {
		CAPTURE(c.name);
		std::vector<float> sizes;
		affresco::detail::sizeTracks(c.tracks, c.count, c.contents, c.available, 10.0f, sizes);
		REQUIRE(sizes.size() == c.expected.size());
		for(std::size_t i = 0; i < sizes.size(); ++i) CHECK(sizes[i] == doctest::Approx(c.expected[i]));
	}

	std::vector<float> starts;
	affresco::detail::trackStarts(std::vector<float>{100, 50, 25}, 10.0f, starts);
	CHECK(starts == std::vector<float>{0, 110, 170});
}

TEST_CASE("layout: equal fractions give exactly the Grid's equal columns") {
	// The Grid computed (width - gaps) / columns, then i * (cell + gap): the tracks must match bit for bit
	for(const float width : {317.0f, 1000.0f, 64.5f}) {
		std::vector<float> sizes, starts;
		const std::vector<Track> tracks(3, Track::fraction(1));
		affresco::detail::sizeTracks(tracks, 3, {}, width, 7.0f, sizes);
		affresco::detail::trackStarts(sizes, 7.0f, starts);
		const float cell = std::max((width - 7.0f * 2.0f) / 3.0f, 0.0f);
		for(std::size_t i = 0; i < 3; ++i) {
			CHECK(sizes[i] == cell);
			CHECK(starts[i] == static_cast<float>(i) * (cell + 7.0f));
		}
	}
}
