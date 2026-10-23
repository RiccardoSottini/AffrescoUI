/// @file
/// @brief A node's "*" key takes every press its other keys don't name or begin, before the document's keys and the
/// text of a focused field.

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

/// @brief A screen with a focused field whose keys record the presses "*" takes in state.seen.
/// @param nodeKeys The field's keys besides "*".
/// @param documentKeys The document's keys.
/// @return The screen.
std::string recorderScreen(const std::string& nodeKeys, const std::string& documentKeys) {
	return R"json({ "format": "affrescoui-screen", "version": 2, "state": { "text": "", "seen": [], "n": 0 },
		"actions": { "add": [{ "increment": "state.n", "by": 1 }] }, "keys": )json" +
	       documentKeys + R"json(, "root": { "id": "root", "children": [{ "id": "field", "type": "textarea",
		"bind": "state.text", "autofocus": true, "width": 300, "height": 60, "keys": { )json" + nodeKeys +
	       R"json( "*": { "push": "state.seen", "value": "{{event.key}}" } } }] } })json";
}

} // namespace

TEST_CASE("any key: \"*\" takes plain and modified presses before the document's keys and the text") {
	Built built(recorderScreen("", R"({ "ctrl+x ctrl+f": "add", "ctrl+g": "add" })"));
	built.press(affresco::Key::A, U'a');
	built.press(affresco::Key::X, 0, controlKey);
	built.press(affresco::Key::G, 0, controlKey);
	CHECK(built.state("seen") == json::array({"a", "ctrl+x", "ctrl+g"}));
	CHECK(built.state("text") == ""); // nothing typed
	CHECK(built.state("n") == 0);     // the document's keys didn't run, nor did a chord start
}

TEST_CASE("any key: the node's own keys and chords go first") {
	Built built(recorderScreen(R"("enter": "add", "ctrl+c k": "add",)", "{}"));
	built.press(affresco::Key::Enter);
	built.press(affresco::Key::C, 0, controlKey); // begins the node's chord: "*" leaves it
	built.press(affresco::Key::K);
	CHECK(built.state("n") == 2);
	built.press(affresco::Key::B, U'b');
	CHECK(built.state("seen") == json::array({"b"}));
}
