/// @file
/// @brief Snapping on a canvas (render's src/spatial/Snapping.cpp): moves and resizes pulled to the grid, to other items'
/// edges and centres and to guides, within a tolerance, with the smart guides to draw.

#include "spatial/Snapping.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cmath>

using affresco::Rect;
using affresco::SnapEdges;
using affresco::SnapLine;
using affresco::snapMove;
using affresco::snapResize;
using affresco::SnapTargets;

TEST_CASE("snapping: a move lands on the grid, without a smart guide") {
	const SnapTargets targets{.grid = 10.0f};
	const auto result = snapMove({12.0f, 27.0f, 52.0f, 47.0f}, targets, 4.0f);
	CHECK(result.rect == Rect{10.0f, 30.0f, 50.0f, 50.0f});
	CHECK_FALSE(result.lines[0]);
	CHECK_FALSE(result.lines[1]);
	// Beyond the tolerance: unchanged
	CHECK(snapMove({15.0f, 15.0f, 20.0f, 20.0f}, {.grid = 10.0f}, 1.0f).rect == Rect{15.0f, 15.0f, 20.0f, 20.0f});
}

TEST_CASE("snapping: a move lands on another item's edge or centre, and draws the guide over both") {
	const std::array<Rect, 1> others{Rect{100.0f, 0.0f, 200.0f, 50.0f}};
	const SnapTargets targets{.others = others};
	// Left edge 4 units from the other's right edge
	const auto result = snapMove({204.0f, 80.0f, 254.0f, 120.0f}, targets, 6.0f);
	CHECK(result.rect.left == doctest::Approx(200.0f));
	CHECK(result.rect.top == doctest::Approx(80.0f));
	REQUIRE(result.lines[0]);
	CHECK(result.lines[0]->vertical);
	CHECK(result.lines[0]->position == doctest::Approx(200.0f));
	CHECK(result.lines[0]->from == doctest::Approx(0.0f));
	CHECK(result.lines[0]->to == doctest::Approx(120.0f));
	CHECK_FALSE(result.lines[1]);

	// Centre to centre, on the y axis
	const auto centred = snapMove({300.0f, 2.0f, 320.0f, 52.0f}, targets, 6.0f);
	CHECK(centred.rect.top == doctest::Approx(0.0f));
	REQUIRE(centred.lines[1]);
	CHECK_FALSE(centred.lines[1]->vertical);
}

TEST_CASE("snapping: guides pull, and the nearest target wins") {
	const std::array<float, 2> vertical{50.0f, 57.0f};
	const std::array<float, 1> horizontal{100.0f};
	const SnapTargets targets{.grid = 0.0f, .vertical = vertical, .horizontal = horizontal};
	const auto result = snapMove({55.0f, 97.0f, 75.0f, 99.0f}, targets, 5.0f);
	CHECK(result.rect.left == doctest::Approx(57.0f));
	CHECK(result.rect.bottom == doctest::Approx(100.0f));
	REQUIRE(result.lines[0]);
	CHECK(result.lines[0]->position == doctest::Approx(57.0f));
}

TEST_CASE("snapping: a resize moves only the dragged edges, and never inside out") {
	const std::array<Rect, 1> others{Rect{0.0f, 200.0f, 10.0f, 230.0f}};
	const SnapTargets targets{.grid = 0.0f, .others = others};
	const SnapEdges bottomRight{.left = false, .top = false, .right = true, .bottom = true};
	const auto result = snapResize({0.0f, 0.0f, 97.0f, 203.0f}, bottomRight, targets, 5.0f);
	CHECK(result.rect == Rect{0.0f, 0.0f, 97.0f, 200.0f});
	REQUIRE(result.lines[1]);
	CHECK(result.lines[1]->position == doctest::Approx(200.0f));

	const std::array<float, 1> guide{5.0f};
	const SnapEdges left{.left = true, .top = false, .right = false, .bottom = false};
	const auto clamped = snapResize({8.0f, 0.0f, 6.0f, 10.0f}, left, {.vertical = guide}, 5.0f);
	CHECK(clamped.rect.left <= clamped.rect.right);

	// Not a number and no tolerance snap to nothing
	CHECK(snapMove({NAN, 0.0f, 1.0f, 1.0f}, {.grid = 10.0f}, 5.0f).rect.top == 0.0f);
	CHECK(snapMove({1.0f, 1.0f, 2.0f, 2.0f}, {.grid = 10.0f}, 0.0f).rect == Rect{1.0f, 1.0f, 2.0f, 2.0f});
}

TEST_CASE("snapping: a move takes the gap two items of its row already have, and shows both gaps") {
	// Two boxes 20 apart; a third dragged to 23 after the second lands 20 after it
	const std::array<Rect, 2> others{Rect{0.0f, 0.0f, 50.0f, 40.0f}, Rect{70.0f, 0.0f, 120.0f, 40.0f}};
	const SnapTargets targets{.others = others};
	const auto result = snapMove({143.0f, 5.0f, 173.0f, 35.0f}, targets, 6.0f);
	CHECK(result.rect.left == doctest::Approx(140.0f));
	REQUIRE(result.gaps[0]);
	CHECK_FALSE(result.gaps[0].value_or(SnapLine{}).vertical); // a row's gap is measured by a horizontal segment
	CHECK(result.gaps[0].value_or(SnapLine{}).from == doctest::Approx(120.0f));
	CHECK(result.gaps[0].value_or(SnapLine{}).to == doctest::Approx(140.0f));
	REQUIRE(result.gaps[1]);
	CHECK(result.gaps[1].value_or(SnapLine{}).from == doctest::Approx(50.0f));
	CHECK(result.gaps[1].value_or(SnapLine{}).to == doctest::Approx(70.0f));
	CHECK_FALSE(result.gaps[2]);
	CHECK_FALSE(result.gaps[3]);
}

TEST_CASE("snapping: a move between two items centres there, with an indicator on each side") {
	const std::array<Rect, 2> others{Rect{0.0f, 0.0f, 50.0f, 40.0f}, Rect{150.0f, 0.0f, 200.0f, 40.0f}};
	const SnapTargets targets{.others = others};
	// 40 wide in a room of 100: 30 on each side, from 80 (dragged to 83)
	const auto result = snapMove({83.0f, 10.0f, 123.0f, 30.0f}, targets, 6.0f);
	CHECK(result.rect.left == doctest::Approx(80.0f));
	REQUIRE(result.gaps[0]);
	REQUIRE(result.gaps[1]);
	CHECK(result.gaps[0].value_or(SnapLine{}).to - result.gaps[0].value_or(SnapLine{}).from == doctest::Approx(30.0f));
	CHECK(result.gaps[1].value_or(SnapLine{}).to - result.gaps[1].value_or(SnapLine{}).from == doctest::Approx(30.0f));
	// Out of the row (no overlap across), no gap is taken
	const auto apart = snapMove({120.0f, 100.0f, 140.0f, 130.0f}, {.others = others}, 2.0f);
	CHECK(apart.rect.left == doctest::Approx(120.0f));
	CHECK_FALSE(apart.gaps[0]);
}

TEST_CASE("snapping: an edge within reach wins over an equal gap") {
	// The gap of 20 would put the left edge at 140; the other's right edge at 143 is nearer reach and comes first
	const std::array<Rect, 3> others{Rect{0.0f, 0.0f, 50.0f, 40.0f}, Rect{70.0f, 0.0f, 120.0f, 40.0f},
	                                 Rect{100.0f, 60.0f, 143.0f, 90.0f}};
	const auto result = snapMove({141.0f, 5.0f, 171.0f, 35.0f}, {.others = others}, 6.0f);
	CHECK(result.rect.left == doctest::Approx(143.0f));
}
