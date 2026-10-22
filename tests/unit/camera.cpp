/// @file
/// @brief Camera2D (render's public Camera2D.hpp): mapping between world and screen, zooming at a point, panning,
/// fitting a rectangle, and the zoom's range.

#include <affresco/render/Camera2D.hpp>

#include <doctest/doctest.h>

#include <cmath>
#include <limits>

using affresco::Camera2D;
using affresco::Rect;
using affresco::Vec2;

TEST_CASE("camera: world and screen map both ways") {
	const Camera2D camera{100.0f, -50.0f, 2.0f};
	const Vec2 screen = camera.toScreen(Vec2{110.0f, -40.0f});
	CHECK(screen.x == doctest::Approx(20.0f));
	CHECK(screen.y == doctest::Approx(20.0f));
	const Vec2 world = camera.toWorld(screen);
	CHECK(world.x == doctest::Approx(110.0f));
	CHECK(world.y == doctest::Approx(-40.0f));

	const Rect visible = camera.visible({800.0f, 600.0f});
	CHECK(visible.left == doctest::Approx(100.0f));
	CHECK(visible.top == doctest::Approx(-50.0f));
	CHECK(visible.width() == doctest::Approx(400.0f));
	CHECK(visible.height() == doctest::Approx(300.0f));
}

TEST_CASE("camera: zooming at a point keeps the world under it") {
	const Camera2D camera{-30.0f, 12.5f, 0.75f};
	const Vec2 pointer{317.0f, 211.0f};
	const Vec2 before = camera.toWorld(pointer);
	for(const float zoom : {0.01f, 0.3f, 1.0f, 4.0f, 256.0f}) {
		const Camera2D zoomed = camera.zoomedAt(pointer, zoom);
		CHECK(zoomed.zoom == doctest::Approx(zoom));
		const Vec2 after = zoomed.toWorld(pointer);
		CHECK(after.x == doctest::Approx(before.x).epsilon(1e-4));
		CHECK(after.y == doctest::Approx(before.y).epsilon(1e-4));
	}
}

TEST_CASE("camera: the zoom stays within its range") {
	CHECK(Camera2D::clampZoom(0.0f) == Camera2D::minimumZoom);
	CHECK(Camera2D::clampZoom(-3.0f) == Camera2D::minimumZoom);
	CHECK(Camera2D::clampZoom(1e9f) == Camera2D::maximumZoom);
	CHECK(Camera2D::clampZoom(std::numeric_limits<float>::infinity()) == Camera2D::maximumZoom);
	CHECK(Camera2D::clampZoom(std::numeric_limits<float>::quiet_NaN()) == 1.0f);
	CHECK(Camera2D{}.zoomedAt({10.0f, 10.0f}, 1000.0f).zoom == Camera2D::maximumZoom);
	CHECK(Camera2D{}.zoomedAt({10.0f, 10.0f}, 0.0001f).zoom == Camera2D::minimumZoom);
}

TEST_CASE("camera: panning follows the pointer at any zoom") {
	const Camera2D camera{0.0f, 0.0f, 4.0f};
	const Camera2D panned = camera.pannedBy({40.0f, -20.0f});
	CHECK(panned.x == doctest::Approx(-10.0f));
	CHECK(panned.y == doctest::Approx(5.0f));
	CHECK(panned.zoom == 4.0f);
	// What was under the pointer moves with it
	const Vec2 grabbed = camera.toScreen(Vec2{3.0f, 7.0f});
	const Vec2 moved = panned.toScreen(Vec2{3.0f, 7.0f});
	CHECK(moved.x == doctest::Approx(grabbed.x + 40.0f));
	CHECK(moved.y == doctest::Approx(grabbed.y - 20.0f));
}

TEST_CASE("camera: fitting shows a rectangle whole and centred") {
	const Rect content{1000.0f, 500.0f, 3000.0f, 1500.0f};
	const Camera2D camera = Camera2D::fitted(content, {800.0f, 600.0f}, 40.0f);
	CHECK(camera.zoom == doctest::Approx(720.0f / 2000.0f));
	const Rect visible = camera.visible({800.0f, 600.0f});
	CHECK(visible.left <= content.left);
	CHECK(visible.right >= content.right);
	CHECK(visible.top <= content.top);
	CHECK(visible.bottom >= content.bottom);
	CHECK((visible.left + visible.right) / 2.0f == doctest::Approx(2000.0f));
	CHECK((visible.top + visible.bottom) / 2.0f == doctest::Approx(1000.0f));

	// Empty: zoom 1, centred on it; huge: the smallest zoom
	const Camera2D point = Camera2D::fitted({10.0f, 10.0f, 10.0f, 10.0f}, {200.0f, 100.0f}, 20.0f);
	CHECK(point == Camera2D{-90.0f, -40.0f, 1.0f});
	CHECK(Camera2D::fitted({0.0f, 0.0f, 1e9f, 1e9f}, {800.0f, 600.0f}, 0.0f).zoom == Camera2D::minimumZoom);
}
