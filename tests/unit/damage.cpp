/// @file
/// @brief Retained geometry and damage (render's detail::GeometryCache), headless: what a change records again, what
/// it damages, and which shaders the frame needs, with no OpenGL.

#include "GeometryCache.hpp"
#include "Headless.hpp"

#include <affresco/render/Panel.hpp>
#include <affresco/render/Window.hpp>

#include <doctest/doctest.h>

#include <memory>
#include <optional>

using affresco::Panel;
using affresco::px;
using affresco::Rect;
using affresco::detail::createHeadlessWindow;
using affresco::detail::GeometryCache;
using affresco::detail::ManualClock;
using affresco::detail::Refreshed;
using affresco::detail::takeFrame;

namespace {

/// @brief Adds a panel of a colour at a place.
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
	panel.setColor(affresco::Color::hex(0x6366f1));
	return panel;
}

/// @brief Refreshes a window's next frame: what changed since the last refresh, the window's damage added.
/// @param cache The cache.
/// @param window The window.
/// @return The damage and the shaders.
Refreshed refreshFrame(GeometryCache& cache, affresco::Window& window) {
	const affresco::detail::Frame frame = takeFrame(window);
	Refreshed refreshed = cache.refresh(frame);
	if(frame.damage) affresco::detail::addTo(refreshed.damage, *frame.damage);
	return refreshed;
}

} // namespace

TEST_CASE("damage: a change damages only where the panel was and is; nothing changed damages nothing") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(400, 300, clock);
	window->root().setColor(affresco::Color::hex(0xf1f5f9));
	Panel& a = box(window->root(), 10, 10, 100, 50);
	Panel& b = box(window->root(), 200, 10, 100, 50);
	GeometryCache cache;
	CHECK(refreshFrame(cache, *window).damage == Rect{0, 0, 400, 300}); // the first frame: everything
	CHECK_FALSE(refreshFrame(cache, *window).damage);                   // nothing changed

	a.setColor(affresco::Color::hex(0xf97316)); // its look only
	CHECK(refreshFrame(cache, *window).damage == Rect{10, 10, 110, 60});

	a.setX(px(50)); // it moves: where it was and where it is
	CHECK(refreshFrame(cache, *window).damage == Rect{10, 10, 150, 60});

	b.setVisible(false); // hidden: where it was
	CHECK(refreshFrame(cache, *window).damage == Rect{200, 10, 300, 60});
	b.setVisible(true);
	CHECK(refreshFrame(cache, *window).damage == Rect{200, 10, 300, 60});

	window->root().removeChildren(1); // b is gone: where it was, and the root (its parent) draws again
	CHECK(refreshFrame(cache, *window).damage == Rect{0, 0, 400, 300});
}

TEST_CASE("damage: a shadow is damage where it falls, outside the panel") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(400, 300, clock);
	Panel& card = box(window->root(), 100, 100, 100, 50);
	GeometryCache cache;
	static_cast<void>(refreshFrame(cache, *window));

	card.setShadow({.offset = {0.0f, 10.0f}, .blur = 4.0f, .color = affresco::Color::hex(0x000000).withAlpha(0.5f)});
	const std::optional<Rect> damage = refreshFrame(cache, *window).damage;
	REQUIRE(damage);
	const Rect area = damage.value_or(Rect{});
	CHECK(area.left < 100.0f);   // the blur, past the left edge
	CHECK(area.bottom > 160.0f); // moved down and blurred
	CHECK(area.top == 100.0f);   // the panel: its shadow, moved down, starts 3 px below its top
}

TEST_CASE("damage: a panel moved by a layout pass is recorded again, one the pass skipped is not") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(400, 300, clock);
	Panel& left = box(window->root(), 0, 0, 100, 300);
	const Panel& inside = box(left, 10, 10, 50, 50);
	Panel& right = box(window->root(), 200, 0, 100, 300);
	const Panel& still = box(right, 10, 10, 50, 50);
	GeometryCache cache;
	static_cast<void>(refreshFrame(cache, *window));

	left.setX(px(20)); // its child moves with it; the right column doesn't
	const Refreshed refreshed = refreshFrame(cache, *window);
	CHECK(refreshed.damage == Rect{0, 0, 120, 300});
	CHECK(inside.frame() == Rect{30, 10, 80, 60});
	CHECK(still.frame() == Rect{210, 10, 260, 60});
}

TEST_CASE("damage: the frame's shaders come from every shown panel, even where nothing changed") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(400, 300, clock);
	Panel& rounded = box(window->root(), 10, 10, 100, 50);
	Panel& plain = box(window->root(), 200, 10, 100, 50);
	GeometryCache cache;
	CHECK_FALSE(refreshFrame(cache, *window).shapes);

	rounded.setCornerRadii(affresco::CornerRadii::all(8.0f));
	CHECK(refreshFrame(cache, *window).shapes);
	plain.setColor(affresco::Color::hex(0x22c55e)); // another panel changes: the frame still has the shape
	CHECK(refreshFrame(cache, *window).shapes);
	rounded.setVisible(false); // no shape shown any more
	CHECK_FALSE(refreshFrame(cache, *window).shapes);
}

TEST_CASE("damage: Window::invalidate() records every panel again and damages everything") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(400, 300, clock);
	static_cast<void>(box(window->root(), 10, 10, 100, 50));
	GeometryCache cache;
	static_cast<void>(refreshFrame(cache, *window));

	window->invalidate();
	const affresco::detail::Frame frame = takeFrame(*window);
	CHECK(frame.damageAll);
	CHECK(cache.refresh(frame).damage == Rect{0, 0, 400, 300});
}

TEST_CASE("damage: a cached subtree needs the colour shader, and damages like any other panel") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(400, 300, clock);
	Panel& group = box(window->root(), 10, 10, 200, 100);
	Panel& inner = box(group, 10, 10, 50, 50);
	GeometryCache cache;
	CHECK_FALSE(refreshFrame(cache, *window).color);

	group.setCached(true); // drawn as one quad of its texture, a colour page
	CHECK(refreshFrame(cache, *window).color);

	inner.setColor(affresco::Color::hex(0x22c55e));
	const Refreshed changed = refreshFrame(cache, *window);
	REQUIRE(changed.damage);
	CHECK(changed.color);

	group.setCached(false);
	CHECK_FALSE(refreshFrame(cache, *window).color);
}
