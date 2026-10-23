/// @file
/// @brief The "editor" type with a declared language (src/languages/LanguageEditor.cpp): auto-closing pairs, the
/// indentation rules, code commands bound to keys, a popup's keys, one-off requests and the events it reports.

#include "Headless.hpp"

#include <affresco/affresco.hpp>

#include <doctest/doctest.h>

#include <memory>
#include <string>

using affresco::json;
using affresco::Key;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

constexpr int controlKey = 0x2; ///< GLFW_MOD_CONTROL.

/// @brief A language with line comments, brackets, pairs and indentation rules.
constexpr const char* languages = R"json({ "x": {
	"comments": { "line": "//" },
	"brackets": [["{", "}"], ["(", ")"]],
	"pairs": [["{", "}"], ["(", ")"], ["\"", "\""]],
	"indent": { "increase": "\\{\\s*$", "decrease": "^\\s*\\}" }
} })json";

/// @brief A screen with a focused editor in language "x" on a headless window, its first frame drawn.
struct Built {
	ManualClock clock;                        ///< The window's clock.
	std::unique_ptr<affresco::Window> window; ///< The window.
	affresco::Screen screen;                  ///< The screen.

	/// @brief Builds the screen.
	/// @param text The editor's text.
	/// @param extra More of the editor's keys, each followed by a comma.
	explicit Built(const std::string& text, const std::string& extra = "")
	    : window(createHeadlessWindow(400, 300, clock)),
	      screen(affresco::Screen::fromString(
	          R"json({ "format": "affrescoui-screen", "version": 2, "languages": )json" + std::string(languages) +
	          R"json(, "state": { "text": )json" + json(text).dump() +
	          R"json(, "keys": [], "typed": [], "go": null, "put": null, "popup": false },
	          "actions": { "go": [{ "set": "state.go", "to": "{{event.value}}" }],
	            "put": [{ "set": "state.put", "to": "{{event.value}}" }],
	            "popup": [{ "set": "state.popup", "to": "{{event.value}}" }] },
	          "root": { "id": "root", "children": [{ "id": "code", "type": "editor", "bind": "state.text",
	          "language": "x", "autofocus": true, "width": 380, "height": 200, )json" +
	          extra + R"json( "popup": "{{state.popup}}", "goTo": "{{state.go}}", "insert": "{{state.put}}" }] } })json")) {
		screen.build(*window);
		window->render();
	}

	/// @brief Presses a key and types its character, as the platform delivers them.
	/// @param key The key.
	/// @param codepoint The character it types, or 0 for none.
	/// @param mods The modifier keys held (GLFW_MOD_* bits).
	void press(Key key, char32_t codepoint = 0, int mods = 0) {
		window->routeKey({key, 0, affresco::Action::Press, affresco::Mods{mods}});
		if(codepoint) window->routeChar({codepoint});
		window->routeKey({key, 0, affresco::Action::Release, affresco::Mods{mods}});
		window->render();
	}

	/// @brief Types characters that have no key of their own here.
	/// @param text The characters (ASCII).
	void type(std::string_view text) {
		for(const char c : text) {
			window->routeChar({static_cast<char32_t>(c)});
			window->render();
		}
	}

	/// @brief Sets a state key (go, put or popup) and draws a frame.
	/// @param key Its name, which is also its action's.
	/// @param value Its value.
	void set(const std::string& key, const json& value) {
		screen.runAction(key, window->root(), json{{"value", value}}.dump());
		window->render();
	}

	/// @brief Reads a state key.
	/// @param key Its name.
	/// @return Its value.
	[[nodiscard]] json state(const char* key) const { return json::parse(screen.stateJson())[key]; }
};

} // namespace

TEST_CASE("language editor: an opener adds its closer, typed over, wrapping a selection, and deleted with it") {
	Built built("");
	built.type("f(");
	CHECK(built.state("text") == "f()");
	built.type("a)");
	CHECK(built.state("text") == "f(a)"); // the closer is typed over
	built.press(Key::Backspace);
	built.press(Key::Backspace);
	built.type("(");
	built.press(Key::Backspace); // between an empty pair: both go
	CHECK(built.state("text") == "f(");

	Built word("ab");
	word.type("\"x");
	CHECK(word.state("text") == "\"xab"); // in front of a word, an opener is typed alone
}

TEST_CASE("language editor: Enter between brackets indents, and a closing line goes back a level") {
	Built built("");
	built.type("if {");
	built.press(Key::Enter);
	CHECK(built.state("text") == "if {\n    \n}");
	built.type("x");
	built.press(Key::Enter);
	built.type("}");
	CHECK(built.state("text") == "if {\n    x\n}\n}"); // the new line at the block's level, its } back one
}

TEST_CASE("language editor: commands bound to keys edit lines, each one undo step, and their key isn't typed") {
	Built built("a\n  b", R"json("commands": { "ctrl+/": "toggleComment", "ctrl+d": "deleteLines" },)json");
	built.press(Key::Slash, U'/', controlKey);
	CHECK(built.state("text") == "// a\n  b");
	built.press(Key::Slash, U'/', controlKey);
	CHECK(built.state("text") == "a\n  b");
	built.press(Key::D, U'd', controlKey);
	CHECK(built.state("text") == "  b");
}

TEST_CASE("language editor: a popup takes its keys, and goTo and insert apply once per request") {
	Built built("ab\ncd", R"json("onPopupKey": [{ "push": "state.keys", "value": "{{event.key}}" }],
		"onType": [{ "push": "state.typed", "value": "{{event.word}}" }],)json");
	built.set("go", json{{"line", 1}, {"column", 2}, {"request", 1}});
	built.type("e");
	CHECK(built.state("text") == "ab\ncde");
	CHECK(built.state("typed") == json{"cde"});

	built.set("popup", true);
	built.press(Key::Down);
	built.press(Key::Enter);
	CHECK(built.state("keys") == json{"down", "enter"});
	CHECK(built.state("text") == "ab\ncde"); // Enter went to the popup
	built.set("popup", false);

	built.set("put", json{{"text", "xyz"}, {"before", 3}, {"request", 1}});
	CHECK(built.state("text") == "ab\nxyz");
	built.type("!");
	built.set("go", json{{"line", 1}, {"column", 2}, {"request", 1}}); // the same requests: nothing again
	built.set("put", json{{"text", "xyz"}, {"before", 3}, {"request", 1}});
	built.type("?");
	CHECK(built.state("text") == "ab\nxyz!?");
}
