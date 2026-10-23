/// @file
/// @brief Snapping on a canvas (render's src/spatial/Snapping.cpp): moves and resizes pulled to the grid, to other items'
/// edges and centres and to guides, within a tolerance, with the smart guides to draw.

#include "spatial/Snapping.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cmath>

using affresco::Rect;
using affresco::SnapEdges;
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
