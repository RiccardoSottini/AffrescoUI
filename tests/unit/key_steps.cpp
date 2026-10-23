/// @file
/// @brief The key steps (src/steps/KeySteps.cpp): press sends keys as the keyboard would, without the bindings
/// hearing them, insert types text at the caret and cut takes a span to the clipboard or a state path; and the key
/// names they read (Shortcuts.hpp).

#include "Headless.hpp"
#include "Shortcuts.hpp"

#include <affresco/affresco.hpp>

#include <doctest/doctest.h>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

using affresco::json;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief Collects the errors logged while it lives.
class Errors {
public:
	/// @brief Starts collecting.
	Errors() {
		affresco::setLogSink([this](affresco::LogLevel level, std::string_view message) {
			if(level == affresco::LogLevel::Error) lines.emplace_back(message);
		});
	}
	/// @brief Gives the log back to stderr.
	~Errors() { affresco::setLogSink(nullptr); }
	Errors(const Errors&) = delete;
	Errors& operator=(const Errors&) = delete;
	Errors(Errors&&) = delete;
	Errors& operator=(Errors&&) = delete;

	std::vector<std::string> lines; ///< The errors, oldest first.
};

/// @brief A screen with an editor (focused, bound to state.text) and a text field, on a headless window.
struct Typed {
	ManualClock clock;                        ///< The window's clock.
	std::unique_ptr<affresco::Window> window; ///< The window.
	std::unique_ptr<affresco::Screen> screen; ///< The screen.

	/// @brief Builds the screen.
	/// @param actions Its "actions".
	/// @param keys Its "keys".
	explicit Typed(const std::string& actions, const std::string& keys = "{}")
	    : window(createHeadlessWindow(400, 300, clock)),
	      screen(std::make_unique<affresco::Screen>(affresco::Screen::fromString(
	          R"({"version": 2, "state": {"text": "ab\ncd", "query": "find me", "clip": "x", "n": 0}, "actions": )" +
	          actions + R"(, "keys": )" + keys + R"(, "root": {"id": "root", "children": [
				{"id": "code", "type": "editor", "bind": "state.text", "autofocus": true, "width": 300, "height": 100},
				{"id": "query", "type": "textarea", "bind": "state.query", "width": 300, "height": 30}]}})"))) {
		screen->build(*window);
		window->render();
	}
	/// @brief Runs an action and the update it asks for.
	/// @param action The action.
	void run(std::string_view action) const {
		screen->runAction(action, window->root());
		window->render();
	}
	/// @brief A state value.
	/// @param key Its key.
	/// @return The value.
	[[nodiscard]] json state(const char* key) const { return json::parse(screen->stateJson())[key]; }
};

} // namespace

TEST_CASE("keys: a press names a key and its modifiers, and types what a US keyboard types") {
	using namespace affresco::format;
	const auto press = keyPress("Ctrl + X");
	REQUIRE(press);
	CHECK(press->key == affresco::Key::X);
	CHECK(press->mods.control());
	CHECK_FALSE(press->mods.shift());
	CHECK(typedBy(*press) == 0);
	CHECK(typedBy(*keyPress("shift+1")) == U'!');
	CHECK(typedBy(*keyPress("k")) == U'k');
	CHECK(typedBy(*keyPress("shift+k")) == U'K');
	CHECK(typedBy(*keyPress("space")) == U' ');
	CHECK(typedBy(*keyPress("enter")) == 0);
	CHECK_FALSE(keyPress("nokey"));
	CHECK_FALSE(keyPress("ctrl+"));
	CHECK_FALSE(keyPress(""));
	CHECK(normalizeChord("  Ctrl + X   ctrl+s ") == "ctrl+x ctrl+s");
	CHECK(normalizeChord("  ctrl  s ") == "ctrl+s"); // a press of modifiers only takes the next word as its key
	CHECK(normalizeChord("x shift ctrl") == "x ctrl+shift+");
	CHECK(normalizeChord("a b+") == "a");
}

TEST_CASE("keys: press types into the focused editor, \"times\" over, and a binding doesn't hear it") {
	const Typed typed(R"({"type": [{"press": "x", "times": 3}], "self": [{"increment": "state.n", "by": 1},
		{"press": "ctrl+y y"}]})",
	                  R"({"ctrl+y": "self"})");
	typed.run("type");
	CHECK(typed.state("text") == "xxxab\ncd");
	typed.window->routeKey({affresco::Key::Y, 0, affresco::Action::Press, {0x0002}});
	typed.window->routeKey({affresco::Key::Y, 0, affresco::Action::Release, {0x0002}});
	typed.window->render();
	CHECK(typed.state("n") == 1);               // the binding ran once: its own ctrl+y press didn't run it again
	CHECK(typed.state("text") == "xxxyab\ncd"); // and its press typed one y
}

TEST_CASE("keys: press sends a chord's keys to the node \"in\" names, and an editing key edits") {
	const Typed typed(R"({"go": [{"press": "end backspace", "in": "#query"}]})");
	typed.run("go");
	CHECK(typed.state("query") == "find m");
	CHECK(typed.state("text") == "ab\ncd");
}

TEST_CASE("keys: insert types text at the caret of an editor or a text field") {
	const Typed typed(R"({"a": [{"insert": "hi "}], "b": [{"insert": "!", "in": "#query"}]})");
	typed.run("a");
	CHECK(typed.state("text") == "hi ab\ncd");
	typed.run("b");
	CHECK(typed.state("query") == "find me!");
}

TEST_CASE("keys: cut takes the line's end, then its break, into a state path, appending or keeping") {
	const Typed typed(R"({"kill": [{"cut": "lineEnd", "into": "state.clip", "append": true}],
		"copy": [{"cut": "line", "into": "state.clip", "keep": true}],
		"field": [{"cut": "line", "in": "#query"}]})");
	typed.run("kill");
	CHECK(typed.state("text") == "\ncd");
	CHECK(typed.state("clip") == "xab");
	typed.run("kill"); // the caret is at the end: the break goes
	CHECK(typed.state("text") == "cd");
	CHECK(typed.state("clip") == "xab\n");
	typed.run("copy");
	CHECK(typed.state("text") == "cd");
	CHECK(typed.state("clip") == "cd");
	typed.run("field");
	CHECK(typed.state("query") == "");
	CHECK(typed.window->clipboardText() == "find me");
}

TEST_CASE("keys: a press with no such key, or too many times, is an error") {
	const Typed typed(R"({"bad": [{"press": "ctrl+nokey"}], "many": [{"press": "x", "times": 1001}],
		"what": [{"cut": "paragraph"}]})");
	const Errors errors;
	typed.run("bad");
	typed.run("many");
	typed.run("what");
	REQUIRE(errors.lines.size() == 3);
	CHECK(errors.lines[0].contains("no key 'ctrl+nokey'"));
	CHECK(errors.lines[1].contains("from 1 to 1000"));
	CHECK(errors.lines[2].contains("\"lineEnd\""));
	CHECK(typed.state("text") == "ab\ncd");
}
