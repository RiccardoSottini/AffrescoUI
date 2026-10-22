/// @file
/// @brief A headless Window (no display, no OpenGL): layout, input routing, mouse capture, hover, scrolling, focus,
/// tooltips and timers, on a clock the tests move by hand.

#include "Headless.hpp"

#include <affresco/render/Stack.hpp>
#include <affresco/render/Window.hpp>

#include <doctest/doctest.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

using affresco::Action;
using affresco::Edge;
using affresco::MouseButton;
using affresco::Panel;
using affresco::Rect;
using affresco::Vec2;
using affresco::Window;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;
using namespace affresco::literals;

namespace {

/// @brief Presses and releases the left button at a point, as the user would.
/// @param window The window.
/// @param at Where.
void click(Window& window, Vec2 at) {
	for(const Action action : {Action::Press, Action::Release})
		window.routeMouseButton({MouseButton::Left, action, {}, at});
}

/// @brief A panel of a fixed size at a position.
/// @param parent Where to add it.
/// @param x Left, in pixels.
/// @param y Top, in pixels.
/// @param width In pixels.
/// @param height In pixels.
/// @return The panel.
Panel& box(Panel& parent, float x, float y, float width, float height) {
	Panel& panel = parent.add<Panel>();
	panel.setPosition(affresco::px(x), affresco::px(y));
	panel.setSize(affresco::px(width), affresco::px(height));
	return panel;
}

} // namespace

TEST_CASE("render: a headless window lays out anchors, percentages and minimum sizes") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(400, 300, clock);

	Panel& header = window->add<Panel>();
	header.setId("header");
	header.setAnchor(Edge::Top, 0_px);
	header.setAnchor(Edge::Left, 0_px);
	header.setAnchor(Edge::Right, 0_px);
	header.setHeight(40_px);

	Panel& half = window->add<Panel>();
	half.setSize(50_pct, 25_pct);
	half.setMinSize(250_px, 0_px); // wider than half of 400

	Panel& below = window->add<Panel>();
	below.setAnchorTo(Edge::Top, "header", Edge::Bottom, 8.0f);
	below.setAnchor(Edge::Bottom, 10_px);
	below.setAnchor(Edge::Left, 10_px);
	below.setWidth(100_px);
	window->render();

	CHECK(header.frame() == Rect{0.0f, 0.0f, 400.0f, 40.0f});
	CHECK(half.frame() == Rect{0.0f, 0.0f, 250.0f, 75.0f});
	CHECK(below.frame() == Rect{10.0f, 48.0f, 110.0f, 290.0f});

	window->setSize(200, 100); // fires onResize as the OS would, and lays out again
	window->render();
	CHECK(header.frame() == Rect{0.0f, 0.0f, 200.0f, 40.0f});
	CHECK(half.frame().height() == 25.0f);
	CHECK(below.frame() == Rect{10.0f, 48.0f, 110.0f, 90.0f});
}

TEST_CASE("render: a headless window lays out rows and columns") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(300, 200, clock);

	auto& row = window->add<affresco::Stack>();
	row.setDirection(affresco::Direction::Row);
	row.setSize(300_px, 50_px);
	row.setGap(10.0f);
	Panel& fixed = row.add<Panel>();
	fixed.setSize(60_px, 50_px);
	Panel& grown = row.add<Panel>();
	grown.setSize(0_px, 50_px);
	grown.setGrow(1.0f);

	auto& column = window->add<affresco::Stack>();
	column.setPosition(0_px, 100_px);
	column.setSize(100_px, 100_px);
	Panel& first = column.add<Panel>();
	first.setSize(100_px, 30_px);
	Panel& second = column.add<Panel>();
	second.setSize(100_px, 30_px);
	window->render();

	CHECK(fixed.frame() == Rect{0.0f, 0.0f, 60.0f, 50.0f});
	CHECK(grown.frame() == Rect{70.0f, 0.0f, 300.0f, 50.0f}); // the room left after the gap
	CHECK(first.frame() == Rect{0.0f, 100.0f, 100.0f, 130.0f});
	CHECK(second.frame() == Rect{0.0f, 130.0f, 100.0f, 160.0f});
}

TEST_CASE("render: a headless window routes clicks to the panel under the pointer, which captures the mouse") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(200, 100, clock);
	Panel& left = box(window->root(), 0, 0, 100, 100);
	Panel& right = box(window->root(), 100, 0, 100, 100);

	std::vector<std::string> heard;
	left.onMouseButton.connect([&](const affresco::MouseButtonEvent& event) {
		heard.push_back(std::string("left ") + (event.action == Action::Press ? "press" : "release"));
	});
	left.onMouseMove.connect([&](const affresco::MouseMoveEvent&) { heard.emplace_back("left move"); });
	right.onMouseButton.connect([&](const affresco::MouseButtonEvent&) { heard.emplace_back("right button"); });

	window->routeMouseButton({MouseButton::Left, Action::Press, {}, {10.0f, 10.0f}});
	CHECK(window->mouseGrabber() == &left);
	window->routeMouseMove({150.0f, 10.0f}); // a drag outside: the grabbing panel still hears it
	CHECK(window->hoveredPanel() == &right);
	window->routeMouseButton({MouseButton::Left, Action::Release, {}, {150.0f, 10.0f}});
	CHECK(window->mouseGrabber() == nullptr);
	CHECK(heard == std::vector<std::string>{"left press", "left move", "left release"});

	click(*window, {150.0f, 50.0f});
	CHECK(heard.back() == "right button");
	CHECK(window->focusedPanel() == &right); // a left click focuses
}

TEST_CASE("render: hover enters and leaves panels and their ancestors, and sets the cursor") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(200, 100, clock);
	Panel& outer = box(window->root(), 0, 0, 100, 100);
	Panel& inner = box(outer, 10, 10, 20, 20);
	inner.setCursor(affresco::Cursor::Hand);

	std::vector<std::string> heard;
	outer.onHover.connect([&](bool on) { heard.emplace_back(on ? "outer in" : "outer out"); });
	inner.onHover.connect([&](bool on) { heard.emplace_back(on ? "inner in" : "inner out"); });

	window->routeMouseMove({15.0f, 15.0f});
	CHECK(window->hoveredPanel() == &inner);
	window->routeMouseMove({50.0f, 50.0f}); // still in outer
	window->routeMouseMove({150.0f, 50.0f});
	window->routeMouseMove({15.0f, 15.0f});
	window->routeCursorEnter(false); // the pointer left the window
	CHECK(window->hoveredPanel() == nullptr);
	CHECK(heard == std::vector<std::string>{"outer in", "inner in", "inner out", "outer out", "outer in", "inner in",
	                                        "inner out", "outer out"});
}

TEST_CASE("render: a scroll scrolls the nearest scrollable panel under the pointer") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(200, 200, clock);
	Panel& list = box(window->root(), 0, 0, 100, 100);
	list.setScrollable(false, true);
	Panel& item = box(list, 0, 0, 100, 300);

	int heard = 0;
	item.onScroll.connect([&](const affresco::ScrollEvent&) { ++heard; });
	window->render();
	window->routeScroll({{0.0f, -2.0f}, {50.0f, 50.0f}}); // two wheel steps down
	CHECK(heard == 1);
	CHECK(list.scrollOffset() == Vec2{0.0f, 48.0f});
	window->routeScroll({{0.0f, -100.0f}, {50.0f, 50.0f}});
	CHECK(list.scrollOffset() == Vec2{0.0f, 200.0f}); // no further than the content

	window->render();
	CHECK(window->panelAt({50.0f, 10.0f}) == &item);
	CHECK(item.frame().top == -200.0f);
}

TEST_CASE("render: focus moves, and a destroyed panel drops the focus, hover and capture") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(200, 100, clock);
	Panel& first = box(window->root(), 0, 0, 100, 100);
	box(window->root(), 100, 0, 100, 100);

	std::vector<bool> changes;
	first.onFocusChanged.connect([&](bool focused) { changes.push_back(focused); });
	window->setFocus(&first);
	CHECK(first.hasFocus());
	click(*window, {150.0f, 50.0f});
	CHECK_FALSE(first.hasFocus());
	CHECK(changes == std::vector<bool>{true, false});

	window->routeMouseButton({MouseButton::Left, Action::Press, {}, {150.0f, 50.0f}});
	REQUIRE(window->focusedPanel());
	window->root().removeChildren();
	CHECK(window->focusedPanel() == nullptr);
	CHECK(window->hoveredPanel() == nullptr);
	CHECK(window->mouseGrabber() == nullptr);
	window->routeMouseButton({MouseButton::Left, Action::Release, {}, {150.0f, 50.0f}});
}

TEST_CASE("render: a headless window keeps a clipboard of its own and never draws") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(200, 100, clock, "headless");
	CHECK(window->title() == "headless");
	CHECK(window->nativeHandle() == nullptr);
	CHECK_FALSE(window->shouldClose());
	window->setClipboardText("copied");
	CHECK(window->clipboardText() == "copied");
	CHECK_FALSE(window->saveScreenshot("never-written.ppm"));
	CHECK_FALSE(window->saveScreenshot("never-written.ppm", 2));
}

TEST_CASE("render: a tooltip shows when the pointer rests, on the window's clock") {
	ManualClock clock;
	const auto window = createHeadlessWindow(200, 100, clock);
	box(window->root(), 0, 0, 100, 100).setTooltip("Help");

	window->routeMouseMove({10.0f, 10.0f});
	REQUIRE(window->secondsToNextTimer());
	CHECK(window->secondsToNextTimer().value_or(-1.0) == doctest::Approx(0.6));
	clock.advance(599.0);
	CHECK(window->secondsToNextTimer().value_or(-1.0) > 0.0);
	clock.advance(1.0);
	CHECK(window->secondsToNextTimer().value_or(-1.0) == 0.0);
	window->render(); // shows it
	CHECK_FALSE(window->secondsToNextTimer());
}

TEST_CASE("render: timers run when the clock says they are due, earliest first") {
	ManualClock clock;
	const auto window = createHeadlessWindow(200, 100, clock);

	std::vector<int> ran;
	window->setTimer(20.0, [&] { ran.push_back(20); });
	window->setTimer(10.0, [&] { ran.push_back(10); });
	window->setTimer(30.0, [&] { ran.push_back(30); });
	window->render();
	CHECK(ran.empty());

	clock.advance(25.0);
	window->render();
	CHECK(ran == std::vector<int>{10, 20});
	CHECK(window->secondsToNextTimer().value_or(-1.0) == doctest::Approx(0.005));

	clock.advance(5.0);
	window->render();
	CHECK(ran == std::vector<int>{10, 20, 30});
	CHECK_FALSE(window->secondsToNextTimer());
}

TEST_CASE("render: a deferred task runs once the outermost event is delivered") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(200, 100, clock);
	Panel& panel = box(window->root(), 0, 0, 100, 100);

	std::vector<std::string> order;
	panel.onMouseButton.connect([&](const affresco::MouseButtonEvent& event) {
		if(event.action != Action::Press) return;
		window->defer([&] { order.emplace_back("deferred"); });
		order.emplace_back("handler");
		CHECK(window->routingInput());
	});
	window->routeMouseButton({MouseButton::Left, Action::Press, {}, {10.0f, 10.0f}});
	CHECK(order == std::vector<std::string>{"handler", "deferred"});
	CHECK_FALSE(window->routingInput());
}

TEST_CASE("render: a drop leaves out the paths a screen can't take, and too many of them") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(200, 100, clock);
	std::vector<std::vector<std::string>> drops;
	window->onDrop.connect([&](const affresco::DropEvent& event) { drops.push_back(event.paths); });

	const std::string nul("/a\0b", 4);
	window->routeDrop({{"/ok.txt", "/bad\xFF.txt", nul, std::string(32769, 'a'), "", "/caf\xC3\xA9\xEF\xBF\xBD"}, {}});
	REQUIRE(drops.size() == 1);
	CHECK(drops[0] == std::vector<std::string>{"/ok.txt", "/caf\xC3\xA9\xEF\xBF\xBD"});

	window->routeDrop({{"/\xC0\xAF", "/\xED\xA0\x80"}, {}}); // an overlong slash, a surrogate: nothing left
	CHECK(drops.size() == 1);

	window->routeDrop({std::vector<std::string>(5000, "/f"), {}});
	REQUIRE(drops.size() == 2);
	CHECK(drops[1].size() == 4096);
}
