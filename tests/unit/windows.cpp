/// @file
/// @brief The "windows" section and its steps without a display: the section is checked, and the `windows` variable
/// follows openWindow, closeWindow and fullscreen, though nothing is shown.

#include "Headless.hpp"

#include <affresco/affresco.hpp>

#include <doctest/doctest.h>

#include <string>

using affresco::json;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief A screen with an audience window, actions that work it, and a computed copy of the variable in the state.
constexpr const char* windowsScreen = R"json({
	"format": "affrescoui-screen", "version": 2,
	"windows": { "audience": { "title": "Slide show", "image": "slide", "background": "#000000" } },
	"state": { "seen": null },
	"actions": {
		"show": [{ "openWindow": "audience", "fullscreen": true }, { "set": "state.seen", "to": "{{windows}}" }],
		"hide": [{ "closeWindow": "audience" }, { "set": "state.seen", "to": "{{windows}}" }],
		"fill": [{ "fullscreen": "toggle", "of": "audience" }, { "set": "state.seen", "to": "{{windows}}" }],
		"fillMain": [{ "fullscreen": true }, { "set": "state.seen", "to": "{{windows}}" }]
	},
	"root": { "id": "root" }
})json";

/// @brief Builds a screen from a document with one more top-level section.
/// @param section The section's JSON.
/// @return Whether the document was accepted.
bool acceptsWindows(const std::string& section) {
	const std::string text =
	    R"({"format": "affrescoui-screen", "version": 2, "windows": )" + section + R"(, "root": {"id": "root"}})";
	try {
		static_cast<void>(affresco::Screen::fromString(text));
		return true;
	} catch(const std::exception&) {
		return false;
	}
}

} // namespace

TEST_CASE("windows: the section is checked") {
	CHECK(acceptsWindows(R"({"audience": {"image": "slide", "width": 1280, "height": 720, "keys": false}})"));
	CHECK_FALSE(acceptsWindows(R"({"audience": {"title": "no image"}})"));
	CHECK_FALSE(acceptsWindows(R"({"main": {"image": "slide"}})"));
	CHECK_FALSE(acceptsWindows(R"({"9lives": {"image": "slide"}})"));
	CHECK_FALSE(acceptsWindows(R"({"audience": {"image": "slide", "width": 50}})"));
	CHECK_FALSE(acceptsWindows(R"({"audience": {"image": "slide", "colour": "#000"}})"));
	CHECK_FALSE(acceptsWindows(R"({"a": {"image": "x"}, "b": {"image": "x"}, "c": {"image": "x"}, "d": {"image": "x"},
		"e": {"image": "x"}})"));
	CHECK_FALSE(acceptsWindows(R"([])"));
}

TEST_CASE("windows: without a display the variable follows the steps") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(320, 200, clock); // outlives the screen
	affresco::Screen screen = affresco::Screen::fromString(windowsScreen);
	screen.build(*window);
	window->render();
	const auto seen = [&] { return json::parse(screen.stateJson())["seen"]; };

	screen.runAction("show", window->root());
	CHECK(seen()["audience"] == json({{"open", true}, {"fullscreen", true}}));
	CHECK(seen()["main"]["fullscreen"] == false);

	screen.runAction("fill", window->root());
	CHECK(seen()["audience"]["fullscreen"] == false);

	screen.runAction("hide", window->root());
	CHECK(seen()["audience"] == json({{"open", false}, {"fullscreen", false}}));

	screen.runAction("show", window->root()); // opens again
	CHECK(seen()["audience"]["open"] == true);

	screen.runAction("fillMain", window->root());
	CHECK(seen()["main"]["fullscreen"] == true);
}
