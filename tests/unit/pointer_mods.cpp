/// @file
/// @brief The modifier keys a mouse button's events carry (`event.shift`, `ctrl`, `alt`, `cmd`), as key events do.

#include "Headless.hpp"

#include <affresco/affresco.hpp>

#include <doctest/doctest.h>

#include <string>

using affresco::json;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief A row whose click, right click and double click keep the keys held in the state.
constexpr const char* rowScreen = R"json({
	"format": "affrescoui-screen", "version": 2,
	"state": { "click": null, "menu": null, "twice": null },
	"root": { "id": "main", "children": [
		{ "id": "row", "anchors": { "left": 0, "top": 0 }, "width": 100, "height": 40,
		  "onClick": { "set": "state.click", "to": { "shift": "{{event.shift}}", "cmd": "{{event.cmd}}", "alt": "{{event.alt}}", "ctrl": "{{event.ctrl}}" } },
		  "onDoubleClick": { "set": "state.twice", "to": "{{event.shift}}" },
		  "onContextMenu": { "set": "state.menu", "to": "{{event.ctrl}}" } }
	] }
})json";

/// @brief Clicks with a button and the keys held.
/// @param window The window.
/// @param button The button.
/// @param bits The keys held (GLFW_MOD_* bits).
void click(affresco::Window& window, affresco::MouseButton button, int bits) {
	for(const affresco::Action action : {affresco::Action::Press, affresco::Action::Release})
		window.routeMouseButton({button, action, affresco::Mods{bits}, {50.0f, 20.0f}});
	window.render();
}

} // namespace

TEST_CASE("format: a click, a double click and a right click say which modifier keys were held") {
	ManualClock clock;
	const auto window = createHeadlessWindow(400, 300, clock);
	affresco::Screen screen = affresco::Screen::fromString(rowScreen);
	screen.build(*window);
	window->render();

	click(*window, affresco::MouseButton::Left, 0x0008); // Cmd (Super)
	json state = json::parse(screen.stateJson());
	CHECK(state["click"] == json{{"shift", false}, {"cmd", true}, {"alt", false}, {"ctrl", false}});

	clock.advance(1.0); // past the double click's time
	click(*window, affresco::MouseButton::Left, 0x0001 | 0x0004); // Shift and Alt
	state = json::parse(screen.stateJson());
	CHECK(state["click"] == json{{"shift", true}, {"cmd", false}, {"alt", true}, {"ctrl", false}});

	click(*window, affresco::MouseButton::Left, 0x0001); // the second click soon after: a double click
	CHECK(json::parse(screen.stateJson())["twice"] == true);

	click(*window, affresco::MouseButton::Right, 0x0002); // Control
	CHECK(json::parse(screen.stateJson())["menu"] == true);
}
