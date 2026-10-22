/// @file
/// @brief Commands and keybindings (src/Contributions.hpp, the runtime's key resolution): bindings with "when",
/// lists of bindings, chords, enablement, and the context, commands and keybindings variables; and what the checker
/// finds in "keys".

#include "Checker.hpp"
#include "Headless.hpp"
#include "Shortcuts.hpp"

#include <affresco/affresco.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <string>
#include <vector>

using affresco::json;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief Presses a key with Control held, as the user would.
/// @param window The window.
/// @param key The key.
void pressControl(affresco::Window& window, affresco::Key key) {
	window.routeKey({key, 0, affresco::Action::Press, {0x0002}});
	window.routeKey({key, 0, affresco::Action::Release, {0x0002}});
}

/// @brief Presses a key with no modifier.
/// @param window The window.
/// @param key The key.
void press(affresco::Window& window, affresco::Key key) {
	window.routeKey({key, 0, affresco::Action::Press, {}});
	window.routeKey({key, 0, affresco::Action::Release, {}});
}

/// @brief A screen with commands and keys, built on a headless window.
/// @param keys The document's "keys".
/// @return The screen's document.
std::string document(const std::string& keys) {
	return R"json({
		"format": "affrescoui-screen", "version": 2,
		"state": { "n": 0, "mode": "a", "dirty": false, "seen": null },
		"actions": {
			"add": [{ "increment": "state.n", "by": 1 }],
			"addTen": [{ "increment": "state.n", "by": 10 }],
			"save": { "title": "Save", "category": "File", "enablement": "{{state.dirty}}",
			          "do": [{ "set": "state.n", "to": 100 }] },
			"open": { "title": "Open", "category": "File", "do": "add" },
			"look": [{ "set": "state.seen", "to": { "commands": "{{commands}}", "keys": "{{keybindings}}",
			                                         "platform": "{{context.platform}}" } }]
		},
		"keys": )json" +
	       keys + R"json(,
		"root": { "type": "panel", "id": "root" }
	})json";
}

/// @brief The screen's state.
/// @param screen The screen.
/// @return It.
json state(const affresco::Screen& screen) { return json::parse(screen.stateJson()); }

/// @brief The checker's findings on a document, one line each.
/// @param document The document.
/// @return The lines.
std::vector<std::string> findings(const json& document) {
	const auto found = affresco::format::check(document, document.value("state", json::object()), {});
	REQUIRE(found);
	std::vector<std::string> lines;
	if(!found) return lines;
	for(const auto& finding : *found) lines.push_back(finding.text());
	return lines;
}

/// @brief Whether any line contains a text.
/// @param lines The lines.
/// @param text The text.
/// @return true if one does.
bool anyHas(const std::vector<std::string>& lines, const std::string& text) {
	return std::ranges::any_of(lines, [&](const std::string& line) { return line.contains(text); });
}

} // namespace

TEST_CASE("format: a binding runs when its \"when\" is true, and a list runs its first match") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(400, 300, clock);
	affresco::Screen screen = affresco::Screen::fromString(document(R"({
		"ctrl+r": [{ "when": "{{state.mode == 'b'}}", "do": "addTen" }, { "do": "add" }],
		"ctrl+e": { "when": "{{state.mode == 'b'}}", "do": "addTen" }
	})"));
	screen.build(*window);

	pressControl(*window, affresco::Key::R);
	CHECK(state(screen)["n"] == 1);
	pressControl(*window, affresco::Key::E); // its "when" is false
	CHECK(state(screen)["n"] == 1);
}

TEST_CASE("format: a chord waits for its next press, and a press that doesn't continue it is dropped") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(400, 300, clock);
	affresco::Screen screen =
	    affresco::Screen::fromString(document(R"({ "ctrl+k ctrl+a": "add", "ctrl+r": "addTen" })"));
	screen.build(*window);

	pressControl(*window, affresco::Key::K);
	CHECK(state(screen)["n"] == 0);
	pressControl(*window, affresco::Key::A);
	CHECK(state(screen)["n"] == 1);

	pressControl(*window, affresco::Key::K);
	pressControl(*window, affresco::Key::R); // ends the chord and is dropped
	CHECK(state(screen)["n"] == 1);
	pressControl(*window, affresco::Key::R);
	CHECK(state(screen)["n"] == 11);

	pressControl(*window, affresco::Key::K);
	press(*window, affresco::Key::Escape); // ends the chord
	pressControl(*window, affresco::Key::A);
	CHECK(state(screen)["n"] == 11);
}

TEST_CASE("format: a key skips a disabled command, and a run step ignores its enablement") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(400, 300, clock);
	affresco::Screen screen = affresco::Screen::fromString(document(R"({ "ctrl+s": "save", "ctrl+r": "open" })"));
	screen.build(*window);

	pressControl(*window, affresco::Key::S); // "dirty" is false
	CHECK(state(screen)["n"] == 0);
	pressControl(*window, affresco::Key::R); // a command whose "do" names another action
	CHECK(state(screen)["n"] == 1);
	REQUIRE(screen.find("root"));
	screen.runAction("save", *screen.find("root"));
	CHECK(state(screen)["n"] == 100);
}

TEST_CASE("format: the commands and keybindings variables list the document's commands and keys") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(400, 300, clock);
	affresco::Screen screen =
	    affresco::Screen::fromString(document(R"({ "ctrl+s": "save", "ctrl+k ctrl+o": "open", "ctrl+l": "look" })"));
	screen.build(*window);
	pressControl(*window, affresco::Key::L);

	const json seen = state(screen)["seen"];
	REQUIRE(seen["commands"].size() == 2);
	CHECK(seen["commands"][0]["name"] == "open"); // File, then Open before Save
	CHECK(seen["commands"][0]["key"] == "ctrl+k ctrl+o");
	CHECK(seen["commands"][1]["name"] == "save");
	CHECK(seen["commands"][1]["enabled"] == false);
	CHECK(seen["commands"][1]["keyLabel"] == affresco::format::keyLabel("ctrl+s"));
	CHECK(seen["keys"].size() == 3);
	CHECK(seen["keys"][0]["scope"] == "screen");
	CHECK(seen["platform"].is_string());
}

TEST_CASE("format: the checker finds keys bound twice, single presses a chord shadows, and unknown actions") {
	const json bound = json::parse(document(R"({
		"ctrl+s": [{ "when": "{{state.dirty}}", "do": "save" }, { "when": "{{state.dirty}}", "do": "add" }],
		"ctrl+k": "add", "ctrl+k ctrl+a": "add",
		"ctrl+q": { "do": "nowhere" },
		"ctrl+w": { "when": "{{state.n +}}", "do": "add" }
	})"));
	const std::vector<std::string> lines = findings(bound);
	CAPTURE(lines);
	CHECK(anyHas(lines, "bound twice"));
	CHECK(anyHas(lines, "shadowed by a chord"));
	CHECK(anyHas(lines, "keys.ctrl+q.do"));
	CHECK(anyHas(lines, "keys.ctrl+w.when"));

	const json clean = json::parse(document(R"({
		"ctrl+s": [{ "when": "{{state.dirty}}", "do": "save" }, { "do": "add" }],
		"ctrl+k ctrl+a": "add"
	})"));
	CHECK(findings(clean).empty());
}
