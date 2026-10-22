/// @file
/// @brief The desktop (src/Contributions.cpp, src/Events.cpp, render's Window): files dropped from the OS reach
/// `onDrop` as `event.files`, the display's scale and colour scheme reach `window`, and `window.menuBar` gives the
/// window its menus, whose items run their actions.

#include "Headless.hpp"

#include <affresco/affresco.hpp>

#include <doctest/doctest.h>

#include <string>

using affresco::json;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief The screen's state.
/// @param screen The screen.
/// @return It.
json state(const affresco::Screen& screen) { return json::parse(screen.stateJson()); }

/// @brief A label's text.
/// @param screen The screen.
/// @param id The label's id.
/// @return Its text.
std::string text(affresco::Screen& screen, const std::string& id) {
	auto* label = dynamic_cast<affresco::Label*>(screen.find(id));
	REQUIRE(label);
	return label->text();
}

/// @brief A screen with a drop target, labels showing the window's variables, and a native menu bar.
constexpr const char* desktopScreen = R"json({
	"format": "affrescoui-screen", "version": 2,
	"window": { "menuBar": [{ "menu": "file", "title": "File" }, { "menu": "edit" }] },
	"state": { "files": null, "ran": null },
	"actions": {
		"save": [{ "set": "state.ran", "to": "save" }],
		"open": [{ "set": "state.ran", "to": "{{'open ' + name}}" }]
	},
	"menus": {
		"file": { "save": { "run": "save", "title": "Save", "group": "2" },
		          "open": { "run": "open", "with": { "name": "a.txt" }, "title": "Open", "group": "1" } },
		"edit": { "undo": { "run": "save", "title": "Undo", "when": "{{false}}" } }
	},
	"root": { "id": "root", "children": [
		{ "id": "target", "width": "100%", "height": "100%", "onDrop": [{ "set": "state.files", "to": "{{event.files}}" }],
		  "children": [
			{ "id": "scheme", "type": "label", "text": "{{window.colorScheme}}" },
			{ "id": "scaled", "type": "label", "text": "{{window.scale > 1 ? 'scaled' : 'one'}}" },
			{ "id": "native", "type": "label", "text": "{{window.nativeMenuBar ? 'native' : 'drawn'}}" }
		] }
	] }
})json";

} // namespace

TEST_CASE("format: files dropped from the OS reach onDrop as event.files") {
	ManualClock clock;
	const auto window = createHeadlessWindow(400, 300, clock);
	affresco::Screen screen = affresco::Screen::fromString(desktopScreen);
	screen.build(*window);
	window->render();

	window->routeDrop({{"/a/b.txt", "/c.png"}, {200.0f, 150.0f}});
	window->render();
	CHECK(state(screen)["files"] == json::parse(R"(["/a/b.txt", "/c.png"])"));
}

TEST_CASE("format: window.colorScheme and window.scale follow the display") {
	ManualClock clock;
	const auto window = createHeadlessWindow(400, 300, clock);
	affresco::Screen screen = affresco::Screen::fromString(desktopScreen);
	screen.build(*window);
	window->render();
	CHECK(text(screen, "scheme") == "light");
	CHECK(text(screen, "scaled") == "one");
	CHECK(text(screen, "native") == "drawn"); // headless: no native menu bar

	window->routeDisplay({2.0f, affresco::ColorScheme::Dark});
	window->render();
	CHECK(text(screen, "scheme") == "dark");
	CHECK(text(screen, "scaled") == "scaled");
}

TEST_CASE("format: window.menuBar gives the window the menus, whose items run their actions") {
	ManualClock clock;
	const auto window = createHeadlessWindow(400, 300, clock);
	affresco::Screen screen = affresco::Screen::fromString(desktopScreen);
	screen.build(*window);
	window->render();

	const auto& bar = window->menuBar();
	REQUIRE(bar.size() == 2);
	CHECK(bar[0].title == "File");
	CHECK(bar[1].title == "edit");       // the menu's name when there is no title
	CHECK(bar[1].submenu.empty());       // its only item's "when" is false
	REQUIRE(bar[0].submenu.size() == 3); // open, a separator between the groups, save
	CHECK(bar[0].submenu[0].title == "Open");
	CHECK(bar[0].submenu[1].separator);
	REQUIRE(bar[0].submenu[0].run);
	const auto open = bar[0].submenu[0].run, save = bar[0].submenu[2].run; // copies: running one updates the bar
	open();
	window->render();
	CHECK(state(screen)["ran"] == "open a.txt");
	save();
	window->render();
	CHECK(state(screen)["ran"] == "save");
}

TEST_CASE("format: a native menu bar that lists a large menu many times keeps to a budget of items") {
	// Win32 numbers menu items with 16 bits: 20 copies of a 5,000-item menu would wrap them
	json document = json::parse(desktopScreen);
	json big = json::object();
	for(int i = 0; i < 5000; ++i) big["i" + std::to_string(i)] = {{"run", "save"}};
	document["menus"]["big"] = big;
	document["window"]["menuBar"] = json::array();
	for(int i = 0; i < 20; ++i) document["window"]["menuBar"].push_back({{"menu", "big"}});
	ManualClock clock;
	const auto window = createHeadlessWindow(400, 300, clock);
	affresco::Screen screen = affresco::Screen::fromString(document.dump());
	screen.build(*window);
	window->render();

	std::size_t items = 0;
	for(const auto& menu : window->menuBar()) items += 1 + menu.submenu.size();
	CHECK(items == 10000);
	CHECK(window->menuBar().size() == 2); // 1 + 5,000, then 1 + 4,998
}
