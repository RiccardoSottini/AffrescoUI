/// @file
/// @brief What apps kept meeting in the library: modifier keys on pointer events, the clipboard's text read by a
/// "paste" step, a warning for two includes that give the same name, and what only building a screen rejects
/// (automation::buildHeadless(), which `affrescoui check` runs).

#include "Headless.hpp"

#include <affresco/affresco.hpp>
#include <affresco/automation/Scenarios.hpp>
#include <affresco/core/Log.hpp>
#include <affresco/format/Rules.hpp>

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>

using affresco::json;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief A screen built in a headless window.
struct Built {
	ManualClock clock;                        ///< The window's time.
	std::unique_ptr<affresco::Window> window; ///< The window.
	affresco::Screen screen;                  ///< The screen.

	/// @brief Builds a screen.
	/// @param text The screen.
	explicit Built(const char* text)
	    : window(createHeadlessWindow(400, 300, clock)), screen(affresco::Screen::fromString(text)) {
		screen.build(*window);
		window->render();
	}

	/// @brief The live state.
	/// @return It.
	[[nodiscard]] json state() const { return json::parse(screen.stateJson()); }

	/// @brief Clicks a point with modifier keys held, as a person does: the keys pressed first, released after.
	/// @param at The point.
	/// @param mods The modifiers (GLFW bits).
	void click(affresco::Vec2 at, int mods = 0) {
		if(mods & 0x1) window->routeKey({affresco::Key::LeftShift, 0, affresco::Action::Press, {mods}});
		window->routeMouseMove(at);
		window->routeMouseButton({affresco::MouseButton::Left, affresco::Action::Press, {mods}, at});
		window->routeMouseButton({affresco::MouseButton::Left, affresco::Action::Release, {mods}, at});
		if(mods & 0x1) window->routeKey({affresco::Key::LeftShift, 0, affresco::Action::Release, {}});
		window->render();
	}
};

/// @brief A panel that keeps the modifier keys of its click and its drag in the state.
constexpr const char* modifiersScreen = R"json({
	"format": "affrescoui-screen", "version": 2,
	"state": { "shift": null, "alt": null, "dragShift": null },
	"root": { "id": "root", "children": [
		{ "id": "box", "x": 0, "y": 0, "width": 100, "height": 100,
		  "onClick": [{ "set": "state.shift", "to": "{{event.shift}}" }, { "set": "state.alt", "to": "{{event.alt}}" }],
		  "onDrag": [{ "set": "state.dragShift", "to": "{{event.shift}}" }] }
	] }
})json";

} // namespace

TEST_CASE("pointer events carry the modifier keys held") {
	Built built(modifiersScreen);
	built.click({50, 50}, 0x1);
	CHECK(built.state()["shift"] == true);
	CHECK(built.state()["alt"] == false);
	built.click({50, 50});
	CHECK(built.state()["shift"] == false);

	// A drag reads the keys held since the last key event: Shift pressed before the press, kept while moving
	affresco::Window& window = *built.window;
	window.routeKey({affresco::Key::LeftShift, 0, affresco::Action::Press, {0x1}});
	window.routeMouseMove({10, 10});
	window.routeMouseButton({affresco::MouseButton::Left, affresco::Action::Press, {0x1}, {10, 10}});
	window.routeMouseMove({40, 40});
	window.render();
	CHECK(built.state()["dragShift"] == true);
	window.routeKey({affresco::Key::LeftShift, 0, affresco::Action::Release, {}});
	window.routeMouseMove({60, 60});
	window.render();
	CHECK(built.state()["dragShift"] == false);
	window.routeMouseButton({affresco::MouseButton::Left, affresco::Action::Release, {}, {60, 60}});
}

TEST_CASE("paste reads the clipboard's text on the person's own click, never from a timer") {
	Built built(R"json({
		"format": "affrescoui-screen", "version": 2,
		"state": { "clip": null, "given": null, "late": null },
		"root": { "id": "root", "children": [
			{ "id": "now", "x": 0, "y": 0, "width": 100, "height": 100,
			  "onClick": [{ "paste": "text", "into": "state.clip", "onDone": [{ "set": "state.given", "to": "{{result}}" }] }] },
			{ "id": "later", "x": 200, "y": 0, "width": 100, "height": 100,
			  "onClick": [{ "after": 10, "do": [{ "paste": "text", "into": "state.late" }] }] }
		] }
	})json");
	built.window->setClipboardText("copied {{state}}");
	built.click({50, 50});
	CHECK(built.state()["clip"] == "copied {{state}}"); // written as it is, never evaluated
	CHECK(built.state()["given"] == "copied {{state}}");

	std::string reported;
	built.screen.onError([&reported](const std::string& message) { reported += message; });
	built.click({250, 50});
	built.clock.advance(20);
	built.window->render();
	CHECK(built.state()["late"].is_null());
	CHECK(reported.find("person's own input") != std::string::npos);
}

TEST_CASE("a revision can't add a paste step: it reads what the person copied") {
	const affresco::format::Rules rules;
	const json before = json::parse(R"({"format": "affrescoui-screen", "version": 2, "state": {"x": 0},
		"actions": {"next": [{"set": "state.x", "to": 1}]}, "root": {"id": "r"}})");
	json after = before;
	after["actions"]["next"] = json::parse(R"([{"paste": "text", "into": "state.x"}])");
	CHECK_THROWS_WITH_AS(rules.checkChange(before, after), doctest::Contains("permission denied"),
	                     affresco::DocumentError);
}

TEST_CASE("two includes that give the same name differently are warned of; the screen's own win silently") {
	const std::filesystem::path folder = std::filesystem::temp_directory_path() / "affrescoui-twice-defined";
	std::filesystem::create_directories(folder);
	const auto write = [&](const char* name, const char* text) { std::ofstream(folder / name) << text; };
	write("a.json", R"({"actions": {"go": [{"set": "state.x", "to": 1}]}, "state": {"x": 0, "y": 0}})");
	write("b.json", R"({"actions": {"go": [{"set": "state.x", "to": 2}]}, "state": {"y": 5}})");
	write("screen.json", R"({"format": "affrescoui-screen", "version": 2, "include": ["a.json", "b.json"],
		"state": {"y": 1}, "root": {"id": "r"}})");
	std::string logged;
	affresco::setLogSink([&](affresco::LogLevel, std::string_view message) { logged += std::string(message) + "\n"; });
	const affresco::Screen screen = affresco::Screen::fromFile(folder / "screen.json");
	affresco::setLogSink(nullptr);
	std::filesystem::remove_all(folder);
	CHECK(logged.find("b.json defines actions.go") != std::string::npos);
	CHECK(logged.find("state.y") == std::string::npos); // the screen's own state.y is meant to win
	CHECK(json::parse(screen.documentJson())["actions"]["go"][0]["to"] == 1);
}

TEST_CASE("building headless rejects what loading alone lets through") {
	// Loads: the properties are applied only when the screen is built
	affresco::Screen gap = affresco::Screen::fromString(R"({"format": "affrescoui-screen", "version": 2,
		"root": {"type": "column", "id": "r", "gap": 4}})");
	CHECK_THROWS_WITH(affresco::automation::buildHeadless(std::move(gap)),
	                  doctest::Contains("root.gap: not allowed on the root"));
	affresco::Screen padded = affresco::Screen::fromString(R"({"format": "affrescoui-screen", "version": 2,
		"root": {"id": "r", "children": [{"id": "box", "gap": 8}]}})");
	CHECK_THROWS_WITH(affresco::automation::buildHeadless(std::move(padded)),
	                  doctest::Contains("root.children[0].gap"));
	CHECK_NOTHROW(affresco::automation::buildHeadless(affresco::Screen::fromString(
	    R"({"format": "affrescoui-screen", "version": 2, "root": {"id": "r", "children": [{"id": "a"}]}})")));
}

TEST_CASE("Cmd+A in a text area selects the whole text, which what is typed next replaces") {
	Built built(R"json({
		"format": "affrescoui-screen", "version": 2, "state": { "draft": "" },
		"root": { "id": "root", "children": [
			{ "id": "field", "type": "textarea", "bind": "state.draft", "x": 0, "y": 0, "width": 200, "height": 24 }
		] }
	})json");
	affresco::Window& window = *built.window;
	built.click({50, 12});
	const auto type = [&](std::u32string_view text) {
		for(const char32_t c : text) window.routeChar({c});
		window.render();
	};
	const auto press = [&](affresco::Key key, int mods) {
		window.routeKey({key, 0, affresco::Action::Press, {mods}});
		window.routeKey({key, 0, affresco::Action::Release, {mods}});
		window.render();
	};
	type(U"abc");
	CHECK(built.state()["draft"] == "abc");
	press(affresco::Key::A, 0x8); // Cmd
	type(U"x");
	CHECK(built.state()["draft"] == "x");
	type(U"y");
	CHECK(built.state()["draft"] == "xy"); // the selection ended with the replacement
	press(affresco::Key::A, 0x2); // Ctrl
	press(affresco::Key::Backspace, 0);
	CHECK(built.state()["draft"] == "");
	type(U"kept");
	press(affresco::Key::A, 0x2);
	press(affresco::Key::Left, 0); // another key ends the selection
	type(U"!");
	CHECK(built.state()["draft"] == "kept!");
}

TEST_CASE("letter spacing widens a label fitted to its text by the room between its characters") {
	const auto widthWith = [](const char* spacing) {
		const std::string font = (std::filesystem::path(AFFRESCOUI_FONTS_DIR) / "Roboto-Regular.ttf").generic_string();
		const std::string text = std::string(R"({"format": "affrescoui-screen", "version": 2,
			"fonts": {"default": {"file": ")") + font + R"(", "size": 14}},
			"root": {"id": "root", "children": [
				{"id": "title", "type": "label", "text": "SPACED", "fitText": true)" + spacing + "}]}}";
		Built built(text.c_str());
		affresco::Panel* title = built.screen.find("title");
		REQUIRE(title);
		return title->frame().width();
	};
	const float plain = widthWith("");
	CHECK(widthWith(R"(, "letterSpacing": 4)") == doctest::Approx(plain + 4.0f * 5.0f).epsilon(0.02));
	CHECK(widthWith(R"(, "letterSpacing": 0)") == doctest::Approx(plain));
}

TEST_CASE("\"justify\" is a label's horizontal alignment; its vertical one stays") {
	Built built(R"json({"format": "affrescoui-screen", "version": 2, "root": {"id": "root", "children": [
		{"id": "a", "type": "label", "text": "x", "wrap": true, "align": "justify center"},
		{"id": "b", "type": "label", "text": "x", "wrap": true, "align": {"horizontal": "justify"}},
		{"id": "c", "type": "label", "text": "x", "wrap": true, "align": "center"}]}})json");
	const auto justified = [&](const char* id) {
		return dynamic_cast<affresco::Label*>(built.screen.find(id))->justify();
	};
	CHECK(justified("a"));
	CHECK(justified("b"));
	CHECK_FALSE(justified("c"));
}
