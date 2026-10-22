/// @file
/// @brief Transforms (render/Transform.hpp, Panel::setTransform, the format's "rotate", "scale", "skew", "translate"
/// and "transformOrigin"): the matrix maths, hit-testing through nested transforms, the events a window routes
/// through them, and the damage they make. Headless: no OpenGL.

#include "GeometryCache.hpp"
#include "Headless.hpp"

#include <affresco/affresco.hpp>
#include <affresco/render/Transform.hpp>

#include <doctest/doctest.h>

#include <cmath>
#include <optional>

using affresco::Affine2;
using affresco::Panel;
using affresco::px;
using affresco::Rect;
using affresco::Vec2;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief Whether two points are the same, to rounding.
/// @param a One.
/// @param b The other.
/// @return true if each coordinate is within 1e-4.
bool near(Vec2 a, Vec2 b) { return std::abs(a.x - b.x) < 1e-4f && std::abs(a.y - b.y) < 1e-4f; }

/// @brief Whether two transforms are the same, to rounding.
/// @param a One.
/// @param b The other.
/// @return true if each member is within 1e-4.
bool nearTransform(const Affine2& a, const Affine2& b) {
	return near(Vec2{a.a, a.b}, Vec2{b.a, b.b}) && near(Vec2{a.c, a.d}, Vec2{b.c, b.d}) &&
	       near(Vec2{a.tx, a.ty}, Vec2{b.tx, b.ty});
}

/// @brief Adds a panel at a place.
/// @param parent Its parent.
/// @param x Its left edge.
/// @param y Its top edge.
/// @param width Its width.
/// @param height Its height.
/// @return The panel.
Panel& box(Panel& parent, float x, float y, float width, float height) {
	Panel& panel = parent.add<Panel>();
	panel.setPosition(px(x), px(y));
	panel.setSize(px(width), px(height));
	return panel;
}

} // namespace

TEST_CASE("transforms: the matrix maths: compose, apply, inverse, bounding box") {
	CHECK(Affine2{}.isIdentity());
	CHECK(Affine2::compose({}, 0.0f, {1.0f, 1.0f}, {}).isIdentity()); // the keys' defaults change nothing
	CHECK(Affine2::rotation(360.0f).isIdentity());

	// Clockwise on screen, exact on quarter turns
	CHECK(Affine2::rotation(90.0f).apply({1.0f, 0.0f}) == Vec2{0.0f, 1.0f});
	CHECK(Affine2::rotation(-90.0f) == Affine2::rotation(270.0f));
	CHECK(Affine2::rotation(90.0f).isAxisAligned());
	CHECK_FALSE(Affine2::rotation(30.0f).isAxisAligned());
	CHECK(near(Affine2::rotation(45.0f).apply({1.0f, 0.0f}), {std::sqrt(0.5f), std::sqrt(0.5f)}));

	// Scale, skew and translate
	CHECK(Affine2::scaling({2.0f, 3.0f}).apply({1.0f, 1.0f}) == Vec2{2.0f, 3.0f});
	CHECK(near(Affine2::skewing({45.0f, 0.0f}).apply({0.0f, 10.0f}), {10.0f, 10.0f})); // x leans with y
	CHECK(Affine2::translation({5.0f, -5.0f}).apply({1.0f, 1.0f}) == Vec2{6.0f, -4.0f});

	// Products apply right to left: scale, then move
	const Affine2 moveThenScale = Affine2::translation({10.0f, 0.0f}) * Affine2::scaling({2.0f, 2.0f});
	CHECK(moveThenScale.apply({1.0f, 1.0f}) == Vec2{12.0f, 2.0f});

	// About a pivot: the pivot stays
	const Affine2 turned = Affine2::rotation(37.0f).about({50.0f, 20.0f});
	CHECK(near(turned.apply({50.0f, 20.0f}), {50.0f, 20.0f}));

	// The inverse undoes it; a flat transform has none
	const Affine2 any = Affine2::compose({7.0f, -3.0f}, 33.0f, {1.5f, 0.5f}, {10.0f, -20.0f});
	const std::optional<Affine2> inverse = any.inverse();
	REQUIRE(inverse);
	CHECK(nearTransform(*inverse * any, Affine2{}));
	CHECK(near(inverse->apply(any.apply({3.0f, 4.0f})), {3.0f, 4.0f}));
	CHECK_FALSE(Affine2::scaling({0.0f, 1.0f}).inverse());
	CHECK_FALSE(Affine2::scaling({NAN, 1.0f}).inverse());

	// The bounding box of a square turned 45 degrees about its centre
	const Rect box = Affine2::rotation(45.0f).about({5.0f, 5.0f}).boundingBox({0.0f, 0.0f, 10.0f, 10.0f});
	const float half = 5.0f * std::sqrt(2.0f);
	CHECK(near(Vec2{box.left, box.top}, {5.0f - half, 5.0f - half}));
	CHECK(near(Vec2{box.right, box.bottom}, {5.0f + half, 5.0f + half}));
}

TEST_CASE("transforms: layout doesn't change; hit-testing goes through each level's inverse") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(400, 400, clock);
	Panel& outer = box(window->root(), 100, 100, 200, 200);
	Panel& inner = box(outer, 150, 150, 40, 40);
	window->render();
	const Rect laidOut = inner.frame();

	SUBCASE("rotate") {
		outer.setTransform(Affine2::rotation(90.0f)); // about its centre (200, 200): inner's (270, 270) goes to (130, 270)
		window->render();
		CHECK(inner.frame() == laidOut);
		CHECK(window->root().hitTest({130.0f, 270.0f}) == &inner);
		CHECK(window->root().hitTest({270.0f, 270.0f}) == &outer); // where inner was laid out
	}
	SUBCASE("scale") {
		outer.setTransform(Affine2::scaling({0.5f, 0.5f})); // inner (250..290) goes to (225..245)
		window->render();
		CHECK(window->root().hitTest({235.0f, 235.0f}) == &inner);
		CHECK(window->root().hitTest({280.0f, 280.0f}) == &window->root()); // outside the scaled outer
	}
	SUBCASE("skew") {
		outer.setTransform(Affine2::skewing({45.0f, 0.0f})); // x moves by y - 200: inner's (270, 270) goes to (340, 270)
		window->render();
		CHECK(window->root().hitTest({335.0f, 270.0f}) == &inner);
		CHECK(window->root().hitTest({270.0f, 270.0f}) == &outer);
	}
	SUBCASE("translate") {
		outer.setTransform(Affine2::translation({-50.0f, 0.0f}));
		window->render();
		CHECK(window->root().hitTest({220.0f, 270.0f}) == &inner);
		CHECK(window->root().hitTest({150.0f, 270.0f}) == &outer);
	}
	SUBCASE("nested: each level maps the point into its own space") {
		outer.setTransform(Affine2::translation({-50.0f, 0.0f}));
		inner.setTransform(Affine2::rotation(45.0f)); // a diamond about (220, 270) on screen
		window->render();
		CHECK(window->root().hitTest({220.0f, 245.0f}) == &inner); // its top point, past the square's top edge
		CHECK(window->root().hitTest({203.0f, 253.0f}) == &outer); // the square's corner, cut off by the diamond
		const std::optional<Vec2> local = inner.toLocal({220.0f, 270.0f});
		REQUIRE(local);
		CHECK(near(*local, {270.0f, 270.0f}));
	}
	SUBCASE("a transform that flattens it is never hit") {
		outer.setTransform(Affine2::scaling({0.0f, 1.0f}));
		window->render();
		CHECK(window->root().hitTest({200.0f, 200.0f}) == &window->root());
		CHECK_FALSE(inner.toLocal({200.0f, 200.0f}));
	}
}

TEST_CASE("transforms: hover, presses and the wheel reach the panel through its transform, in its own space") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(400, 400, clock);
	Panel& outer = box(window->root(), 100, 100, 200, 200);
	outer.setScrollable(false, true);
	Panel& inner = box(outer, 0, 0, 200, 400);
	outer.setTransform(Affine2::scaling({0.5f, 0.5f})); // drawn at (150, 150)..(250, 250)
	window->render();

	Vec2 pressed;
	inner.onMouseButton.connect([&](const affresco::MouseButtonEvent& event) { pressed = event.position; });
	window->routeMouseMove({160.0f, 160.0f});
	CHECK(window->hoveredPanel() == &inner);
	window->routeMouseButton({affresco::MouseButton::Left, affresco::Action::Press, {}, {160.0f, 160.0f}});
	CHECK(window->mouseGrabber() == &inner);
	CHECK(near(pressed, {120.0f, 120.0f})); // in the layout's units
	window->routeMouseButton({affresco::MouseButton::Left, affresco::Action::Release, {}, {160.0f, 160.0f}});

	window->routeScroll({{0.0f, -2.0f}, {200.0f, 200.0f}});
	CHECK(outer.scrollOffset().y > 0.0f);
	window->routeMouseMove({120.0f, 120.0f}); // laid out there, but drawn elsewhere
	CHECK(window->hoveredPanel() == &window->root());
}

TEST_CASE("transforms: damage and accessibility bounds are the transformed bounding box") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(400, 400, clock);
	Panel& card = box(window->root(), 100, 100, 100, 100);
	card.setColor(affresco::Color::hex(0x6366f1));
	affresco::detail::GeometryCache cache;
	static_cast<void>(cache.refresh(affresco::detail::takeFrame(*window)));

	card.setTransform(Affine2::translation({200.0f, 0.0f}));
	const affresco::detail::Frame frame = affresco::detail::takeFrame(*window);
	std::optional<Rect> damage = cache.refresh(frame).damage;
	if(frame.damage) affresco::detail::addTo(damage, *frame.damage);
	REQUIRE(damage);
	CHECK(damage->left <= 100.0f);  // where it was
	CHECK(damage->right >= 400.0f); // and where it is
	CHECK(card.screenBounds() == Rect{300.0f, 100.0f, 400.0f, 200.0f});
}

TEST_CASE("transforms: the format's keys set the transform, and removing them clears it") {
	ManualClock clock;
	const auto window = createHeadlessWindow(400, 400, clock);
	affresco::Screen screen = affresco::Screen::fromString(R"json({
		"format": "affrescoui-screen", "version": 2,
		"root": { "id": "root", "children": [
			{ "id": "card", "x": 100, "y": 100, "width": 100, "height": 50, "rotate": 90,
			  "transformOrigin": "topLeft" },
			{ "id": "tag", "x": 0, "y": 300, "width": 40, "height": 20, "scale": [2, 1], "skew": [0, 0],
			  "translate": [10, 5], "transformOrigin": [0, 100] }
		] }
	})json");
	screen.build(*window);
	window->render();
	Panel* card = screen.find("card");
	Panel* tag = screen.find("tag");
	REQUIRE(card);
	REQUIRE(tag);
	CHECK(card->transform() == Affine2::rotation(90.0f));
	CHECK(card->transformOrigin() == Vec2{0.0f, 0.0f});
	CHECK(card->screenBounds() == Rect{50.0f, 100.0f, 100.0f, 200.0f}); // turned down about its top-left corner
	CHECK(tag->transform() == Affine2::compose({10.0f, 5.0f}, 0.0f, {2.0f, 1.0f}, {}));
	CHECK(tag->transformOrigin() == Vec2{0.0f, 1.0f});
	CHECK(tag->screenBounds() == Rect{10.0f, 305.0f, 90.0f, 325.0f});

	CHECK_THROWS_AS(affresco::Screen::fromString(R"json({
		"format": "affrescoui-screen", "version": 2,
		"root": { "id": "root", "children": [{ "id": "a", "scale": [1, 2, 3] }] }
	})json").build(*window), affresco::DocumentError);
}
