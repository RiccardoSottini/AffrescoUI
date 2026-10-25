/// @file
/// @brief Key notations (src/events/KeyNotation.hpp): the mac, pc and Emacs labels of chords, "window.keyLabels"
/// choosing the labels of commands and keybindings, and the `keyLabel` function.

#include "Headless.hpp"
#include "KeyNotation.hpp"
#include "Shortcuts.hpp"

#include <affresco/affresco.hpp>

#include <doctest/doctest.h>

#include <string>

using affresco::json;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;
using affresco::format::keyLabel;

namespace {

/// @brief A screen whose "look" action copies the keybindings and a few `keyLabel` calls into state.seen.
/// @param window The document's "window" section.
/// @return The screen's document.
std::string document(const std::string& window) {
	return R"json({
		"format": "affrescoui-screen", "version": 2, "window": )json" +
	       window + R"json(,
		"state": { "seen": null },
		"actions": {
			"open": { "title": "Open", "do": [] },
			"look": [{ "set": "state.seen", "to": {
				"keys": "{{keybindings}}", "commands": "{{commands}}",
				"echo": "{{keyLabel('ctrl+x', 'emacs') + '-'}}", "written": "{{keyLabel('Alt+Shift+5', 'emacs')}}",
				"none": "{{keyLabel(null)}}" } }]
		},
		"keys": { "ctrl+x ctrl+f": "open", "ctrl+l": "look" },
		"root": { "type": "panel", "id": "root" }
	})json";
}

/// @brief Builds a screen, presses Control+L and gives state.seen.
/// @param window The document's "window" section.
/// @return What "look" saw.
json seen(const std::string& window) {
	const ManualClock clock;
	const auto headless = createHeadlessWindow(400, 300, clock);
	affresco::Screen screen = affresco::Screen::fromString(document(window));
	screen.build(*headless);
	headless->routeKey({affresco::Key::L, 0, affresco::Action::Press, {0x0002}});
	headless->routeKey({affresco::Key::L, 0, affresco::Action::Release, {0x0002}});
	return json::parse(screen.stateJson())["seen"];
}

} // namespace

TEST_CASE("format: the Emacs notation writes C-, M-, s- and Emacs's key names") {
	CHECK(keyLabel("ctrl+x ctrl+f", "emacs") == "C-x C-f");
	CHECK(keyLabel("alt+x", "emacs") == "M-x");
	CHECK(keyLabel("alt+shift+comma", "emacs") == "M-<");
	CHECK(keyLabel("alt+shift+period", "emacs") == "M->");
	CHECK(keyLabel("alt+shift+5", "emacs") == "M-%");
	CHECK(keyLabel("ctrl+space", "emacs") == "C-SPC");
	CHECK(keyLabel("ctrl+slash", "emacs") == "C-/");
	CHECK(keyLabel("f3", "emacs") == "<f3>");
	CHECK(keyLabel("ctrl+alt+shift+f3", "emacs") == "C-M-S-<f3>");
	CHECK(keyLabel("ctrl+x enter", "emacs") == "C-x RET");
	CHECK(keyLabel("shift+a", "emacs") == "A");
	CHECK(keyLabel("ctrl+shift+a", "emacs") == "C-S-a");
	CHECK(keyLabel("cmd+s", "emacs") == "s-s");
	CHECK(keyLabel("ctrl+x r k", "emacs") == "C-x r k");
	CHECK(keyLabel("pagedown", "emacs") == "<next>");
	CHECK(keyLabel("ctrl+x leftbracket", "emacs") == "C-x [");
}

TEST_CASE("format: the mac and pc notations, and the platform's") {
	CHECK(keyLabel("ctrl+alt+shift+cmd+k", "mac") == "⌃⌥⇧⌘K");
	CHECK(keyLabel("ctrl+k ctrl+s", "pc") == "Ctrl+K Ctrl+S");
	CHECK(keyLabel("cmd+comma", "pc") == "Super+,");
#ifdef __APPLE__
	CHECK(keyLabel("ctrl+s") == "⌃S");
#else
	CHECK(keyLabel("ctrl+s") == "Ctrl+S");
#endif
	CHECK(keyLabel("ctrl+s", "platform") == keyLabel("ctrl+s"));
	CHECK(affresco::format::knownKeyNotation("emacs"));
	CHECK_FALSE(affresco::format::knownKeyNotation("vim"));
}

TEST_CASE("format: window.keyLabels chooses how commands and keybindings label their keys") {
	const json emacs = seen(R"({ "keyLabels": "emacs" })");
	REQUIRE(emacs["keys"].size() == 2);
	CHECK(emacs["keys"][0]["keyLabel"] == "C-x C-f");
	CHECK(emacs["keys"][1]["keyLabel"] == "C-l");
	CHECK(emacs["commands"][0]["keyLabel"] == "C-x C-f");
	CHECK(emacs["echo"] == "C-x-");
	CHECK(emacs["written"] == "M-%");
	CHECK(emacs["none"] == "");
	const json platform = seen("{}");
	CHECK(platform["keys"][0]["keyLabel"] == keyLabel("ctrl+x ctrl+f"));
}

TEST_CASE("format: an unknown window.keyLabels labels keys the platform's way") {
	const json vim = seen(R"({ "keyLabels": "vim" })");
	CHECK(vim["keys"][0]["keyLabel"] == keyLabel("ctrl+x ctrl+f"));
}
