/// @file
/// @brief The canvas (render's CanvasView and its spatial index) in a headless window: culling to the visible
/// rectangle, the camera bound to what a person does, and selection, moves and guides reported back.

#include "paint/GeometryCache.hpp"
#include "Headless.hpp"
#include "Rulers.hpp"
#include "SpatialIndex.hpp"

#include <affresco/render/CanvasView.hpp>
#include <affresco/render/Window.hpp>

#include <doctest/doctest.h>

#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

using affresco::Action;
using affresco::Camera2D;
using affresco::CanvasItem;
using affresco::CanvasItemList;
using affresco::CanvasTransform;
using affresco::CanvasView;
using affresco::Key;
using affresco::MouseButton;
using affresco::Rect;
using affresco::SpatialIndex;
using affresco::Vec2;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief Whether two rectangles meet, edges included.
/// @param a One.
/// @param b The other.
[[nodiscard]] bool meets(const Rect& a, const Rect& b) noexcept {
	return a.left <= b.right && b.left <= a.right && a.top <= b.bottom && b.top <= a.bottom;
}

/// @brief A square grid of items, 10 units wide every 20 units.
/// @param count How many.
/// @return The items, named "i0", "i1" and so on.
[[nodiscard]] std::vector<CanvasItem> gridItems(std::size_t count) {
	std::vector<CanvasItem> items(count);
	const auto columns = static_cast<std::size_t>(std::ceil(std::sqrt(static_cast<double>(count))));
	for(std::size_t i = 0; i < count; ++i) {
		const float x = static_cast<float>(i % columns) * 20.0f, y = static_cast<float>(i / columns) * 20.0f;
		items[i].id = "i" + std::to_string(i);
		items[i].bounds = {x, y, x + 10.0f, y + 10.0f};
	}
	return items;
}

/// @brief A headless 800 by 600 window holding a canvas that fills it.
struct CanvasWindow {
	/// @brief Gives the canvas the items and draws a frame.
	/// @param items The items.
	explicit CanvasWindow(std::vector<CanvasItem> items) {
		canvas.setSize(affresco::px(800.0f), affresco::px(600.0f));
		canvas.setModel(std::make_shared<CanvasItemList>(std::move(items)));
		canvas.onTransform.connect([this](const std::vector<CanvasTransform>& moved) { transforms = moved; });
		window->render();
	}

	/// @brief Lays out and records a frame, as a renderer would: the canvas paints, with no OpenGL.
	void draw() {
		window->render();
		static_cast<void>(cache.refresh(affresco::detail::takeFrame(*window)));
	}

	/// @brief Presses or releases the left button.
	/// @param action Press or Release.
	/// @param at Where.
	/// @param mods The modifier keys held.
	void button(Action action, Vec2 at, int mods = 0) const {
		window->routeMouseButton({MouseButton::Left, action, affresco::Mods{mods}, at});
	}

	/// @brief Drags with the left button from one point to another, in four steps.
	/// @param from Where it starts.
	/// @param to Where it ends.
	void drag(Vec2 from, Vec2 to) const {
		window->routeMouseMove(from);
		button(Action::Press, from);
		for(int step = 1; step <= 4; ++step) {
			const float t = static_cast<float>(step) / 4.0f;
			window->routeMouseMove({from.x + (to.x - from.x) * t, from.y + (to.y - from.y) * t});
		}
		button(Action::Release, to);
	}

	ManualClock clock;                                                                ///< The window's clock.
	std::unique_ptr<affresco::Window> window = createHeadlessWindow(800, 600, clock); ///< The window.
	CanvasView& canvas = window->root().add<CanvasView>();                            ///< The canvas.
	std::vector<CanvasTransform> transforms; ///< What the canvas last reported.
	affresco::detail::GeometryCache cache;   ///< Records the panels' painting.
};

} // namespace

TEST_CASE("canvas culling: the index finds exactly what meets an area") {
	std::vector<Rect> rects;
	for(int i = 0; i < 5000; ++i) {
		const auto x = static_cast<float>((i * 7919) % 10007) - 5000.0f, y = static_cast<float>((i * 104729) % 9973);
		const float size = i % 97 == 0 ? 4000.0f : static_cast<float>(i % 50 + 1);
		rects.push_back({x, y, x + size, y + size});
	}
	rects.push_back({NAN, 0.0f, 1.0f, 1.0f});          // not a number: never found
	rects.push_back({0.0f, 0.0f, INFINITY, INFINITY}); // unbounded: never found
	SpatialIndex index;
	index.build(rects);
	std::vector<std::uint32_t> found;
	for(const Rect area : {Rect{-100.0f, 100.0f, 300.0f, 400.0f}, Rect{-6000.0f, -10.0f, 6000.0f, 11000.0f},
	                       Rect{2000.0f, 2000.0f, 2000.0f, 2000.0f}, Rect{1e6f, 1e6f, 2e6f, 2e6f}}) {
		index.query(area, found);
		std::vector<std::uint32_t> expected;
		for(std::uint32_t i = 0; i < 5000; ++i)
			if(meets(rects[i], area)) expected.push_back(i);
		CHECK(found == expected);
	}
}

TEST_CASE("canvas culling: of 100,000 items only those on screen are drawn") {
	CanvasWindow test(gridItems(100'000));
	test.draw();
	const Rect visible = test.canvas.camera().visible({800.0f, 600.0f});
	std::size_t expected = 0;
	for(std::size_t i = 0; i < 100'000; ++i)
		if(meets(test.canvas.model()->item(i).bounds, visible)) ++expected;
	CHECK(test.canvas.drawnItems() == expected);
	CHECK(expected < 2000);

	test.canvas.setCamera({3000.0f, 3000.0f, 0.5f});
	test.draw();
	CHECK(test.canvas.drawnItems() > 0);
	CHECK(test.canvas.drawnItems() < 6000);
	test.canvas.setCamera({-1e5f, -1e5f, 1.0f}); // nothing there
	test.draw();
	CHECK(test.canvas.drawnItems() == 0);
}

TEST_CASE("canvas: the camera is bounded, pans by dragging with space and zooms at the pointer by keys") {
	CanvasWindow test(gridItems(100));
	test.canvas.setCamera({NAN, 5.0f, 1e9f});
	CHECK(test.canvas.camera() == Camera2D{0.0f, 0.0f, Camera2D::maximumZoom});
	test.canvas.setCamera({});

	test.button(Action::Press, {400.0f, 300.0f}); // a click gives the canvas the keys
	test.button(Action::Release, {400.0f, 300.0f});
	test.window->routeKey({Key::Space, 0, Action::Press, affresco::Mods{0}});
	test.drag({400.0f, 300.0f}, {300.0f, 250.0f});
	test.window->routeKey({Key::Space, 0, Action::Release, affresco::Mods{0}});
	CHECK(test.canvas.camera().x == doctest::Approx(100.0f));
	CHECK(test.canvas.camera().y == doctest::Approx(50.0f));

	const Vec2 pointer{500.0f, 400.0f};
	test.window->routeMouseMove(pointer);
	const Vec2 before = test.canvas.worldAt(pointer);
	test.window->routeKey({Key::Equal, 0, Action::Press, affresco::Mods{0}});
	CHECK(test.canvas.camera().zoom == doctest::Approx(1.25f));
	const Vec2 after = test.canvas.worldAt(pointer);
	CHECK(after.x == doctest::Approx(before.x));
	CHECK(after.y == doctest::Approx(before.y));
}

TEST_CASE("canvas: a click selects, a marquee selects what it meets, and a drag reports the move") {
	CanvasWindow test(gridItems(100)); // 10 by 10, at 20-unit steps
	test.canvas.setRulers(false);
	test.canvas.setSnap(false);
	test.window->render();

	test.button(Action::Press, {25.0f, 25.0f});
	test.button(Action::Release, {25.0f, 25.0f});
	CHECK(test.canvas.selection() == std::vector<std::string>{"i11"});

	test.drag({55.0f, 35.0f}, {25.0f, 15.0f}); // meets columns 1 and 2 of row 1
	CHECK(test.canvas.selection() == std::vector<std::string>{"i11", "i12"});

	test.drag({42.0f, 28.0f}, {72.0f, 48.0f}); // on i12, clear of the handles
	REQUIRE(test.transforms.size() == 2);
	CHECK(test.transforms[0].id == "i11");
	CHECK(test.transforms[0].bounds == Rect{50.0f, 40.0f, 60.0f, 50.0f});
	CHECK(test.transforms[1].bounds == Rect{70.0f, 40.0f, 80.0f, 50.0f});

	test.window->routeKey({Key::Escape, 0, Action::Press, affresco::Mods{0}});
	CHECK(test.canvas.selection().empty());
}

TEST_CASE("canvas: dragging from the top ruler makes a horizontal guide, and back onto it removes it") {
	const CanvasWindow test(gridItems(4));
	test.drag({300.0f, 10.0f}, {300.0f, 200.5f});
	REQUIRE(test.canvas.guides().horizontal.size() == 1);
	CHECK(test.canvas.guides().horizontal[0] == doctest::Approx(200.0f).epsilon(0.01));
	CHECK(test.canvas.guides().vertical.empty());
	test.drag({300.0f, 200.0f}, {300.0f, 5.0f});
	CHECK(test.canvas.guides().horizontal.empty());
}

TEST_CASE("canvas: a grid far from the origin ends, and a guide taken away mid-drag is let go") {
	CanvasWindow test(gridItems(4));
	test.canvas.setGrid(10.72089672088623f);
	test.canvas.setCamera({1.5889568680091192e22f, -1.5889568680091192e22f, 1.0f});
	test.draw(); // a start so far left that the spacing no longer moves it would never end

	test.canvas.setCamera({});
	test.window->routeMouseMove({300.0f, 10.0f});
	test.button(Action::Press, {300.0f, 10.0f});
	test.window->routeMouseMove({300.0f, 100.0f});
	REQUIRE(test.canvas.guides().horizontal.size() == 1);
	test.canvas.setGuides({}); // a peer clears the guides while one is dragged
	test.window->routeMouseMove({300.0f, 150.0f});
	test.window->routeMouseMove({300.0f, 5.0f});
	test.button(Action::Release, {300.0f, 5.0f});
	CHECK(test.canvas.guides().horizontal.empty());
}

TEST_CASE("canvas: a ruler too far out for whole ticks has none") {
	std::vector<affresco::RulerTick> ticks;
	affresco::rulerTicks(0.0, 1.0f, 200.0f, ticks);
	CHECK_FALSE(ticks.empty());
	affresco::rulerTicks(1e30, 1.0f, 200.0f, ticks);
	CHECK(ticks.empty());
	affresco::rulerTicks(-1e30, 1.0f, 200.0f, ticks);
	CHECK(ticks.empty());
}
