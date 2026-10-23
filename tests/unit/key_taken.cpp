/// @file
/// @brief A key a binding runs for is taken (Window::takeKey): the focused editor or text field neither edits with it
/// nor types its character, and a chord's later presses don't type either.

#include "Headless.hpp"

#include <affresco/affresco.hpp>

#include <doctest/doctest.h>

#include <memory>
#include <string>

using affresco::json;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

constexpr int controlKey = 0x2; ///< GLFW_MOD_CONTROL.

/// @brief A screen on a headless window, its first frame drawn.
struct Built {
	ManualClock clock;                        ///< The window's clock.
	std::unique_ptr<affresco::Window> window; ///< The window.
	affresco::Screen screen;                  ///< The screen.

	/// @brief Builds a screen.
	/// @param text The screen.
	explicit Built(const std::string& text)
	    : window(createHeadlessWindow(400, 300, clock)), screen(affresco::Screen::fromString(text)) {
		screen.build(*window);
		window->render();
	}

	/// @brief Presses a key and types its character, as the platform delivers them.
	/// @param key The key.
	/// @param codepoint The character it types, or 0 for none.
	/// @param mods The modifier keys held (GLFW_MOD_* bits).
	void press(affresco::Key key, char32_t codepoint = 0, int mods = 0) {
		window->routeKey({key, 0, affresco::Action::Press, affresco::Mods{mods}});
		if(codepoint) window->routeChar({codepoint});
		window->routeKey({key, 0, affresco::Action::Release, affresco::Mods{mods}});
		window->render();
	}

	/// @brief Reads a state key.
	/// @param key Its name.
	/// @return Its value.
	[[nodiscard]] json state(const char* key) const { return json::parse(screen.stateJson())[key]; }
};

/// @brief A screen with one focused input and a counter that "add" increments.
/// @param type The input's type.
/// @param nodeKeys The input's own "keys".
/// @param documentKeys The document's "keys".
/// @return The screen.
std::string inputScreen(const std::string& type, const std::string& nodeKeys, const std::string& documentKeys) {
	return R"json({ "format": "affrescoui-screen", "version": 2, "state": { "text": "ab", "n": 0 },
		"actions": { "add": [{ "increment": "state.n", "by": 1 }] }, "keys": )json" +
	       documentKeys + R"json(, "root": { "id": "root", "children": [{ "id": "field", "type": ")json" + type +
	       R"json(", "bind": "state.text", "autofocus": true, "width": 300, "height": 60, "keys": )json" + nodeKeys +
	       "}] } }";
}

} // namespace

TEST_CASE("key taken: a binding on the focused editor keeps its key from the text") {
	Built built(inputScreen("editor", R"({ "tab": "add", "x": "add" })", "{}"));
	built.press(affresco::Key::Tab, U'\t');
	built.press(affresco::Key::X, U'x');
	CHECK(built.state("n") == 2);
	CHECK(built.state("text") == "ab");
	built.press(affresco::Key::Y, U'y'); // no binding: typed
	CHECK(built.state("text") == "yab");
}

TEST_CASE("key taken: a chord's presses type nothing in the focused editor, even one that ends it") {
	Built built(inputScreen("editor", "{}", R"({ "ctrl+k x": "add" })"));
	built.press(affresco::Key::K, 0, controlKey);
	built.press(affresco::Key::X, U'x');
	CHECK(built.state("n") == 1);
	CHECK(built.state("text") == "ab");

	built.press(affresco::Key::K, 0, controlKey);
	built.press(affresco::Key::Z, U'z'); // doesn't continue the chord: ends it and is dropped
	CHECK(built.state("n") == 1);
	CHECK(built.state("text") == "ab");

	built.press(affresco::Key::K, 0, controlKey);
	built.press(affresco::Key::Escape); // ends the chord
	built.press(affresco::Key::X, U'x');
	CHECK(built.state("n") == 1);
	CHECK(built.state("text") == "xab");
}

TEST_CASE("key taken: a binding on a focused text field keeps Backspace from erasing") {
	Built built(inputScreen("textarea", R"({ "backspace": "add" })", "{}"));
	built.press(affresco::Key::Backspace);
	CHECK(built.state("n") == 1);
	CHECK(built.state("text") == "ab");
	built.press(affresco::Key::C, U'c');
	CHECK(built.state("text") == "abc");
}

TEST_CASE("key taken: a field without bindings edits and types as before") {
	Built built(inputScreen("textarea", "{}", R"({ "ctrl+k x": "add" })"));
	built.press(affresco::Key::Backspace);
	CHECK(built.state("text") == "a");
	built.press(affresco::Key::X, U'x'); // a plain key isn't the document's while a text field has focus
	CHECK(built.state("text") == "ax");
	CHECK(built.state("n") == 0);
}

TEST_CASE("key taken: text typed after a taken key, without a key press of its own, is typed") {
	Built built(inputScreen("editor", R"({ "ctrl+k": "add" })", "{}"));
	built.press(affresco::Key::K, 0, controlKey);
	CHECK(built.state("n") == 1);
	built.window->routeChar({U'q'}); // as automation and input methods type
	built.window->render();
	CHECK(built.state("text") == "qab");
}
