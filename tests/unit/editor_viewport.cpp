/// @file
/// @brief The editor's view (src/editor/TextEditorViewSpan.cpp): a document's "viewport" names a state path the editor
/// writes its first and last visible lines, its rows and its line count to, as it scrolls, changes and resizes.

#include "Headless.hpp"

#include <affresco/affresco.hpp>

#include <doctest/doctest.h>

#include <memory>
#include <string>

using affresco::json;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief A screen with a focused editor of 30 lines whose view goes to state.view, on a headless window.
struct Viewed {
	ManualClock clock;                        ///< The window's clock.
	std::unique_ptr<affresco::Window> window; ///< The window.
	std::unique_ptr<affresco::Screen> screen; ///< The screen.

	/// @brief Builds the screen.
	/// @param type The node's type.
	explicit Viewed(const std::string& type = "editor")
	    : window(createHeadlessWindow(400, 300, clock)),
	      screen(std::make_unique<affresco::Screen>(affresco::Screen::fromString(
	          R"({"version": 2, "state": {"text": ")" + lines() + R"("}, "actions": {"short": [{"set": "state.text",
			"to": "one\ntwo"}]}, "root": {"id": "root", "children": [{"id": "code", "type": ")" +
	          type +
	          R"(", "bind": "state.text", "viewport": "state.view", "autofocus": true, "width": 300, "height": 100}]}})"))) {
		screen->build(*window);
		window->render();
	}
	/// @brief The text: 30 numbered lines, escaped for JSON.
	/// @return It.
	static std::string lines() {
		std::string text;
		for(int n = 1; n <= 30; ++n) text += (n > 1 ? "\\n" : "") + std::to_string(n);
		return text;
	}
	/// @brief Presses and releases a key, then renders.
	/// @param key The key.
	void press(affresco::Key key) const {
		window->routeKey({key, 0, affresco::Action::Press, {}});
		window->routeKey({key, 0, affresco::Action::Release, {}});
		window->render();
	}
	/// @brief What the editor wrote.
	/// @return state.view.
	[[nodiscard]] json view() const { return json::parse(screen->stateJson())["view"]; }
};

} // namespace

TEST_CASE("editor view: the editor writes its first and last lines, its rows and its lines") {
	const Viewed viewed;
	const json view = viewed.view();
	CHECK(view["first"] == 1);
	CHECK(view["lines"] == 30);
	CHECK(view["rows"].get<int>() > 1);
	CHECK(view["last"] == view["first"].get<int>() + view["rows"].get<int>() - 1);
}

TEST_CASE(
    "editor view: a scroll moves it, the end shows the last line, and a shorter text keeps its last line at the top") {
	const Viewed viewed;
	viewed.press(affresco::Key::PageDown);
	CHECK(viewed.view()["first"].get<int>() > 1);
	for(int n = 0; n < 10; ++n) viewed.press(affresco::Key::PageDown);
	CHECK(viewed.view()["last"] == 30);
	viewed.screen->runAction("short", viewed.window->root());
	viewed.window->render();
	viewed.window->render();
	const json view = viewed.view();
	CHECK(view["first"] == 2); // like the scroll, which lets the last line up to the top
	CHECK(view["last"] == 2);
	CHECK(view["lines"] == 2);
}

TEST_CASE("editor view: only an editor has one") {
	CHECK_THROWS_WITH(Viewed("textarea"), doctest::Contains("only text editors have \"viewport\""));
}

TEST_CASE("editor view: a canvas keeps its own \"view\"") {
	ManualClock clock;
	const auto window = createHeadlessWindow(400, 300, clock);
	auto screen = affresco::Screen::fromString(R"({"version": 2, "root": {"id": "root", "children": [{"id": "board",
		"type": "canvas", "width": 300, "height": 200, "view": {"camera": {"x": 0, "y": 0, "zoom": 2}}}]}})");
	CHECK_NOTHROW(screen.build(*window));
	window->render();
}
