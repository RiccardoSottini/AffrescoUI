/// @file
/// @brief Filling the display (SecondWindows::mayFill(), Window::setFullscreen()): only the person's own click or key,
/// or the application's code, starts it; the window then shows a hint it draws itself, and Esc always leaves; and a
/// revision may not add the step.

#include "Headless.hpp"

#include <affresco/affresco.hpp>
#include <affresco/format/Rules.hpp>

#include <doctest/doctest.h>

#include <string>

using affresco::json;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief A screen that tries to fill the display on load, from a timer and from the person's click and key.
constexpr const char* fillingScreen = R"json({
	"format": "affrescoui-screen", "version": 2,
	"windows": { "audience": { "image": "slide" } },
	"state": { "escaped": false, "seen": null },
	"onLoad": [{ "fullscreen": true }],
	"keys": { "f": "fill", "escape": [{ "set": "state.escaped", "to": true }] },
	"actions": {
		"fill": [{ "fullscreen": true }],
		"later": [{ "after": 100, "do": [{ "fullscreen": true }] }],
		"openLater": [{ "after": 100, "do": [{ "openWindow": "audience", "fullscreen": true },
			{ "set": "state.seen", "to": "{{windows}}" }] }]
	},
	"root": { "id": "root", "onClick": "fill" }
})json";

/// @brief Whether the window shows the hint that Esc leaves full screen, once it has run its due timers.
/// @param window The window.
/// @return Whether it does.
bool showsHint(affresco::Window& window) {
	window.render();
	return window.showsFullscreenHint();
}

/// @brief Presses and lets go of a key on a window.
/// @param window The window.
/// @param key The key.
void type(affresco::Window& window, affresco::Key key) {
	window.routeKey({key, 0, affresco::Action::Press, {}});
	window.routeKey({key, 0, affresco::Action::Release, {}});
}

} // namespace

TEST_CASE("fullscreen: a load, a timer or a host's message doesn't fill the display; the person's click does") {
	ManualClock clock;
	const auto window = createHeadlessWindow(320, 200, clock);
	affresco::Screen screen = affresco::Screen::fromString(fillingScreen);
	screen.build(*window);
	window->render();
	CHECK_FALSE(window->isFullscreen()); // onLoad
	CHECK_FALSE(showsHint(*window));

	screen.runAction("later", window->root()); // the action is the host's, its timer isn't
	clock.advance(150.0);
	window->render();
	CHECK_FALSE(window->isFullscreen());

	screen.runAction("openLater", window->root()); // the window opens, without filling the display
	clock.advance(150.0);
	window->render();
	CHECK(json::parse(screen.stateJson())["seen"]["audience"] == json({{"open", true}, {"fullscreen", false}}));

	// A scroll or a move is no press
	window->routeScroll({{0.0f, 1.0f}, {10.0f, 10.0f}});
	CHECK_FALSE(window->isFullscreen());

	window->routeMouseButton({affresco::MouseButton::Left, affresco::Action::Press, {}, {10.0f, 10.0f}});
	window->routeMouseButton({affresco::MouseButton::Left, affresco::Action::Release, {}, {10.0f, 10.0f}});
	CHECK(window->isFullscreen());
}

TEST_CASE("fullscreen: the window says how to leave, and Esc leaves, still reaching the screen") {
	ManualClock clock;
	const auto window = createHeadlessWindow(320, 200, clock);
	affresco::Screen screen = affresco::Screen::fromString(fillingScreen);
	screen.build(*window);
	window->render();

	type(*window, affresco::Key::F);
	REQUIRE(window->isFullscreen());
	CHECK(showsHint(*window));
	clock.advance(4100.0);
	CHECK_FALSE(showsHint(*window)); // gone after a few seconds
	CHECK(window->isFullscreen());

	type(*window, affresco::Key::Escape);
	CHECK_FALSE(window->isFullscreen());
	CHECK(json::parse(screen.stateJson())["escaped"] == true);

	screen.runAction("fill", window->root()); // the application's own code may
	CHECK(window->isFullscreen());
}

TEST_CASE("fullscreen: a revision may not add the step, or open a window filling the display") {
	const json before = json::parse(R"json({"format": "affrescoui-screen", "version": 2,
		"windows": { "audience": { "image": "slide" } },
		"actions": { "next": [{ "set": "state.p", "to": 1 }] }, "state": { "p": 0 }, "root": { "id": "root" }})json");
	const affresco::format::Rules rules;
	for(const json& steps : {json::parse(R"([{ "fullscreen": true }])"),
	                         json::parse(R"([{ "openWindow": "audience", "fullscreen": true }])")}) {
		CAPTURE(steps.dump());
		json after = before;
		after["actions"]["next"] = steps;
		CHECK_THROWS_WITH_AS(rules.checkChange(before, after), doctest::Contains("permission denied"),
		                     affresco::DocumentError);
	}
}
