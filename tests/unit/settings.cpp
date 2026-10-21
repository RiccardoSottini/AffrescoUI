/// @file
/// @brief Menus and settings (src/Contributions.hpp, src/Settings.hpp): the menus variable (groups, separators,
/// "when", submenus, titles and keys from the commands), the settings section (defaults, checked writes, reset by
/// "remove"), the settings and settingsList variables, the settingsPage component, and what the checker finds.

#include "Checker.hpp"
#include "Headless.hpp"

#include <affresco/affresco.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <string>
#include <vector>

using affresco::json;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief A screen with commands, keys, menus and settings.
/// @param menus The document's "menus".
/// @param root The root node.
/// @return The screen's document.
std::string document(const std::string& menus, const std::string& root = R"({ "type": "panel", "id": "root" })") {
	return R"json({
		"format": "affrescoui-screen", "version": 1,
		"state": { "n": 0, "dirty": false, "seen": null },
		"actions": {
			"add": [{ "increment": "state.n", "by": 1 }],
			"save": { "title": "Save", "icon": "save", "enablement": "{{state.dirty}}", "do": "add" },
			"open": { "title": "Open…", "do": "add" },
			"look": [{ "set": "state.seen", "to": { "menus": "{{menus}}", "settings": "{{settings}}",
			                                         "list": "{{settingsList}}" } }],
			"big": [{ "set": "settings.editor.fontSize", "to": 20 }],
			"wrong": [{ "set": "settings.editor.fontSize", "to": "large" }],
			"tooBig": [{ "set": "settings.editor.fontSize", "to": 99 }],
			"reset": [{ "remove": "settings.editor.fontSize" }]
		},
		"keys": { "ctrl+s": "save" },
		"settings": {
			"editor.fontSize": { "type": "integer", "minimum": 6, "maximum": 40, "default": 14, "title": "Font size",
			                     "description": "The editor's font size, in points." },
			"editor.wordWrap": { "type": "boolean", "default": false, "title": "Word wrap" },
			"theme": { "type": "string", "enum": ["dark", "light"], "default": "dark" }
		},
		"menus": )json" +
	       menus + R"json(,
		"root": )json" + root + R"json(
	})json";
}

/// @brief The menus most tests use: groups, an order, a hidden item and a submenu.
constexpr const char* fileMenus = R"({
	"file": {
		"save": { "run": "save", "group": "2_save" },
		"open": { "run": "open", "group": "1_open", "order": 2 },
		"new": { "run": "add", "group": "1_open", "order": 1, "title": "New", "icon": "plus" },
		"hidden": { "run": "add", "group": "1_open", "when": "{{state.dirty}}" },
		"recent": { "submenu": "recent", "group": "3_more", "title": "Open Recent" }
	},
	"recent": { "clear": { "run": "add", "title": "Clear" } }
})";

/// @brief The screen's state.
/// @param screen The screen.
/// @return It.
json state(const affresco::Screen& screen) { return json::parse(screen.stateJson()); }

/// @brief Runs an action from the root.
/// @param screen The screen.
/// @param name The action.
void run(affresco::Screen& screen, std::string_view name) {
	REQUIRE(screen.find("root"));
	screen.runAction(name, *screen.find("root"));
}

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

/// @brief The items of a resolved menu, its submenus' included.
/// @param menu The menu.
/// @return How many.
std::size_t itemsIn(const json& menu) {
	std::size_t total = 0;
	for(const json& entry : menu) total += 1 + (entry.contains("submenu") ? itemsIn(entry["submenu"]) : 0);
	return total;
}

} // namespace

TEST_CASE("format: the menus variable sorts by group and order, puts separators between groups, and hides by when") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(400, 300, clock);
	affresco::Screen screen = affresco::Screen::fromString(document(fileMenus));
	screen.build(*window);
	run(screen, "look");

	const json file = state(screen)["seen"]["menus"]["file"];
	CAPTURE(file.dump());
	REQUIRE(file.size() == 6); // new, open | save | recent
	CHECK(file[0]["name"] == "new");
	CHECK(file[0]["title"] == "New");
	CHECK(file[0]["icon"] == "plus");
	CHECK(file[1]["name"] == "open");
	CHECK(file[1]["title"] == "Open…"); // from the command
	CHECK(file[2]["separator"] == true);
	CHECK(file[3]["name"] == "save");
	CHECK(file[3]["enabled"] == false); // the command's enablement
	CHECK(file[3]["key"] == "ctrl+s");
	CHECK(file[3]["icon"] == "save");
	CHECK(file[4]["separator"] == true);
	REQUIRE(file[5]["submenu"].is_array());
	CHECK(file[5]["submenu"][0]["title"] == "Clear");
}

TEST_CASE("format: settings start at their defaults, take checked writes, and go back to the default on remove") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(400, 300, clock);
	affresco::Screen screen = affresco::Screen::fromString(document("{}"));
	screen.build(*window);

	run(screen, "look");
	json seen = state(screen)["seen"];
	CHECK(seen["settings"]["editor"]["fontSize"] == 14);
	CHECK(seen["settings"]["theme"] == "dark");
	REQUIRE(seen["list"].size() == 3);
	CHECK(seen["list"][0]["key"] == "editor.fontSize"); // by key
	CHECK(seen["list"][0]["title"] == "Font size");
	CHECK(seen["list"][0]["maximum"] == 40);
	CHECK(seen["list"][0]["modified"] == false);
	CHECK(seen["list"][2]["title"] == "theme"); // no title: the key

	run(screen, "big");
	CHECK(state(screen)["settings"]["editor.fontSize"] == 20); // stored flat
	run(screen, "look");
	seen = state(screen)["seen"];
	CHECK(seen["settings"]["editor"]["fontSize"] == 20);
	CHECK(seen["list"][0]["modified"] == true);

	run(screen, "wrong"); // not an integer: reported, and the value stays
	CHECK(state(screen)["settings"]["editor.fontSize"] == 20);
	run(screen, "tooBig"); // over its maximum
	CHECK(state(screen)["settings"]["editor.fontSize"] == 20);

	run(screen, "reset");
	CHECK_FALSE(state(screen)["settings"].contains("editor.fontSize"));
	run(screen, "look");
	CHECK(state(screen)["seen"]["settings"]["editor"]["fontSize"] == 14);
}

TEST_CASE("format: the settings page lists the settings, and its search narrows them") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(600, 400, clock);
	const std::string page = R"({ "type": "panel", "id": "root", "children": [
		{ "use": "settingsPage", "id": "page", "width": "100%", "height": "100%",
		  "with": { "search": "{{state.settingsQuery}}" } }] })";
	json screenJson = json::parse(document("{}", page));
	screenJson["state"]["settingsQuery"] = "";
	screenJson["actions"]["search"] = json::parse(R"([{ "set": "state.settingsQuery", "to": "WRAP" }])");
	affresco::Screen screen = affresco::Screen::fromString(screenJson.dump());
	screen.build(*window);
	window->render();

	const affresco::Panel* found = screen.find("page");
	REQUIRE(found);
	REQUIRE(found->children().size() == 2); // the search field, then the rows
	const affresco::Panel& rows = *found->children()[1];
	CHECK(rows.children().size() == 3);
	run(screen, "search"); // matches "Word wrap", in any case
	window->render();
	CHECK(rows.children().size() == 1);

	screenJson["actions"].erase("wrong");
	screenJson["actions"].erase("tooBig");
	CHECK(findings(screenJson).empty());
}

TEST_CASE("format: a bad settings section is refused when the screen loads") {
	const auto load = [](const std::string& settings) {
		json screen = json::parse(document("{}"));
		screen["settings"] = json::parse(settings);
		return affresco::Screen::fromString(screen.dump());
	};
	CHECK_THROWS(load(R"({ "a": { "type": "number" } })"));                           // no default
	CHECK_THROWS(load(R"({ "a": { "type": "number", "default": "x" } })"));           // not its type
	CHECK_THROWS(load(R"({ "a": { "type": "number", "default": 1 }, "a.b": { "type": "number", "default": 1 } })"));
	CHECK_THROWS(load(R"({ "a..b": { "type": "number", "default": 1 } })"));
}

TEST_CASE("format: a bad menus section is refused when the screen loads") {
	CHECK_THROWS((void)affresco::Screen::fromString(document(R"({ "file": { "x": { "group": "a" } } })")));  // no run
	CHECK_THROWS((void)affresco::Screen::fromString(document(R"({ "file": { "x": { "submenu": "nope" } } })")));
	CHECK_THROWS((void)affresco::Screen::fromString(document(R"({ "file": { "x": { "run": "add", "order": "1" } } })")));
	CHECK_THROWS((void)affresco::Screen::fromString(document(R"({ "file": { "x": { "run": "add", "colour": 1 } } })")));
}

TEST_CASE("format: a menu that opens itself many times over resolves within a budget of items") {
	// 20 items, each opening the menu again, 8 levels deep: 20^9 items without a budget
	std::string items;
	for(int i = 0; i < 20; ++i) items += (i > 0 ? ",\"i" : "\"i") + std::to_string(i) + R"(": { "submenu": "m" })";
	const ManualClock clock;
	const auto window = createHeadlessWindow(400, 300, clock);
	affresco::Screen screen = affresco::Screen::fromString(document(R"({ "m": { )" + items + " } }"));
	screen.build(*window);
	run(screen, "look");
	const json menu = state(screen)["seen"]["menus"]["m"];
	CHECK(menu.size() == 20);
	CHECK(itemsIn(menu) <= 10000);
}

TEST_CASE("format: the checker knows the settings' types, and finds writes to unknown settings") {
	json bad = json::parse(document("{}"));
	bad["actions"]["unknown"] = json::parse(R"([{ "set": "settings.editor.size", "to": 1 }])");
	bad["actions"]["mistyped"] = json::parse(R"([{ "set": "settings.editor.wordWrap", "to": "yes" }])");
	const std::vector<std::string> lines = findings(bad);
	CAPTURE(lines);
	CHECK(anyHas(lines, "no setting 'editor.size'"));
	CHECK(anyHas(lines, "settings.editor.wordWrap"));
	CHECK(anyHas(lines, "settings.editor.fontSize")); // the "wrong" action

	json clean = json::parse(document(fileMenus));
	clean["actions"].erase("wrong");
	clean["actions"].erase("tooBig"); // a constant over the maximum is the checker's to find too
	CHECK(findings(clean).empty());
}
