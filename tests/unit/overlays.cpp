/// @file
/// @brief Overlays and focus (src/Events.cpp, src/BuiltInComponents.cpp): the tab order, modal overlays that trap
/// the focus, arrow keys and mnemonics, the dismiss step, and the built-in menu, dialog and toast components.

#include "Checker.hpp"
#include "Headless.hpp"

#include <affresco/affresco.hpp>

#include <doctest/doctest.h>

#include <string>

using affresco::json;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief Presses and releases a key.
/// @param window The window.
/// @param key The key.
/// @param mods The modifiers (0x1 Shift, 0x4 Alt).
void press(affresco::Window& window, affresco::Key key, int mods = 0) {
	window.routeKey({key, 0, affresco::Action::Press, {mods}});
	window.routeKey({key, 0, affresco::Action::Release, {mods}});
}

/// @brief Clicks the middle of a panel, as the user would.
/// @param window The window.
/// @param panel The panel.
void clickOn(affresco::Window& window, const affresco::Panel& panel) {
	const affresco::Rect& frame = panel.frame();
	const affresco::Vec2 middle{(frame.left + frame.right) / 2.0f, (frame.top + frame.bottom) / 2.0f};
	for(const affresco::Action action : {affresco::Action::Press, affresco::Action::Release})
		window.routeMouseButton({affresco::MouseButton::Left, action, {}, middle});
}

/// @brief The id of the focused panel.
/// @param window The window.
/// @return It, or "" if none has the focus.
std::string focusedId(const affresco::Window& window) {
	return window.focusedPanel() ? window.focusedPanel()->id() : "";
}

/// @brief The screen's state.
/// @param screen The screen.
/// @return It.
json state(const affresco::Screen& screen) { return json::parse(screen.stateJson()); }

/// @brief Builds a screen on a window and lays it out, running what waits for the next turn (a modal's focus).
/// @param window The window.
/// @param clock Its clock.
/// @param screen The screen.
void settle(affresco::Window& window, ManualClock& clock, affresco::Screen& screen) {
	window.render();
	clock.advance(1.0);
	window.render();
	(void)screen;
}

/// @brief A menu bar's document: a File menu with a submenu, and actions that count.
constexpr const char* menuScreen = R"json({
	"format": "affrescoui-screen", "version": 2,
	"state": { "n": 0, "last": null, "bar": {} },
	"actions": {
		"add": [{ "increment": "state.n", "by": 1 }, { "set": "state.last", "to": "{{what ?? 'add'}}" }]
	},
	"root": { "id": "root", "type": "column", "children": [
		{ "id": "bar", "use": "menuBar", "with": { "open": "state.bar", "menus": [
			{ "name": "file", "title": "File", "items": [
				{ "title": "New", "run": "add", "with": { "what": "new" } },
				{ "separator": true },
				{ "title": "Recent", "submenu": [{ "title": "One", "run": "add", "with": { "what": "one" } }] },
				{ "title": "Off", "enabled": false, "run": "add" }
			] }
		] } }
	] }
})json";

} // namespace

TEST_CASE("format: Tab follows tabIndex first, then the tree, and Shift+Tab goes back") {
	ManualClock clock;
	const auto window = createHeadlessWindow(400, 300, clock);
	affresco::Screen screen = affresco::Screen::fromString(R"json({
		"format": "affrescoui-screen", "version": 2,
		"root": { "id": "root", "type": "column", "children": [
			{ "id": "a", "height": 20, "focusable": true },
			{ "id": "b", "height": 20, "focusable": true, "tabIndex": 1 },
			{ "id": "skip", "height": 20, "focusable": true, "tabIndex": -1 },
			{ "id": "c", "height": 20, "focusable": true }
		] }
	})json");
	screen.build(*window);
	settle(*window, clock, screen);

	press(*window, affresco::Key::Tab);
	CHECK(focusedId(*window) == "b");
	press(*window, affresco::Key::Tab);
	CHECK(focusedId(*window) == "a");
	press(*window, affresco::Key::Tab);
	CHECK(focusedId(*window) == "c");
	press(*window, affresco::Key::Tab); // wraps
	CHECK(focusedId(*window) == "b");
	press(*window, affresco::Key::Tab, 0x1);
	CHECK(focusedId(*window) == "c");
}

TEST_CASE("format: a tabIndex past int's range is the last in the tab order, and a negative one is skipped") {
	ManualClock clock;
	const auto window = createHeadlessWindow(400, 300, clock);
	affresco::Screen screen = affresco::Screen::fromString(R"json({
		"format": "affrescoui-screen", "version": 2,
		"root": { "id": "root", "type": "column", "children": [
			{ "id": "huge", "height": 20, "focusable": true, "tabIndex": 1e30 },
			{ "id": "one", "height": 20, "focusable": true, "tabIndex": 1 },
			{ "id": "below", "height": 20, "focusable": true, "tabIndex": -1e30 }
		] }
	})json");
	screen.build(*window);
	settle(*window, clock, screen);

	press(*window, affresco::Key::Tab);
	CHECK(focusedId(*window) == "one");
	press(*window, affresco::Key::Tab);
	CHECK(focusedId(*window) == "huge");
	press(*window, affresco::Key::Tab); // wraps, past the skipped one
	CHECK(focusedId(*window) == "one");
}

TEST_CASE("format: Enter or Space on a focusable node clicks it, and Alt with its mnemonic does too") {
	ManualClock clock;
	const auto window = createHeadlessWindow(400, 300, clock);
	affresco::Screen screen = affresco::Screen::fromString(R"json({
		"format": "affrescoui-screen", "version": 2, "state": { "n": 0, "keyboard": null },
		"root": { "id": "root", "type": "column", "children": [
			{ "id": "go", "height": 20, "focusable": true, "mnemonic": "g",
			  "onClick": [{ "increment": "state.n", "by": 1 }, { "set": "state.keyboard", "to": "{{event.keyboard}}" }] }
		] }
	})json");
	screen.build(*window);
	settle(*window, clock, screen);

	press(*window, affresco::Key::Tab);
	press(*window, affresco::Key::Enter);
	press(*window, affresco::Key::Space);
	CHECK(state(screen)["n"] == 2);
	CHECK(state(screen)["keyboard"] == true);
	window->setFocus(nullptr);
	press(*window, affresco::Key::G, 0x4);
	CHECK(state(screen)["n"] == 3);
}

TEST_CASE("format: the arrows move through a list, Tab leaves it in one step, and a letter picks an item") {
	ManualClock clock;
	const auto window = createHeadlessWindow(400, 300, clock);
	affresco::Screen screen = affresco::Screen::fromString(R"json({
		"format": "affrescoui-screen", "version": 2, "state": { "picked": null },
		"root": { "id": "root", "type": "column", "children": [
			{ "id": "list", "type": "column", "height": 60, "arrows": "vertical", "children": [
				{ "id": "x", "height": 20, "focusable": true, "mnemonic": "x" },
				{ "id": "y", "height": 20, "focusable": true, "mnemonic": "y",
				  "onClick": { "set": "state.picked", "to": "y" } },
				{ "id": "z", "height": 20, "focusable": true }
			] },
			{ "id": "after", "height": 20, "focusable": true }
		] }
	})json");
	screen.build(*window);
	settle(*window, clock, screen);

	press(*window, affresco::Key::Tab);
	CHECK(focusedId(*window) == "x");
	press(*window, affresco::Key::Down);
	CHECK(focusedId(*window) == "y");
	press(*window, affresco::Key::End);
	CHECK(focusedId(*window) == "z");
	press(*window, affresco::Key::Down); // wraps
	CHECK(focusedId(*window) == "x");
	press(*window, affresco::Key::Up);
	CHECK(focusedId(*window) == "z");
	press(*window, affresco::Key::Y);
	CHECK(state(screen)["picked"] == "y");
	press(*window, affresco::Key::Tab); // past the list
	CHECK(focusedId(*window) == "after");
}

TEST_CASE("format: a modal overlay takes the focus, keeps Tab inside, and gives the focus back when it closes") {
	ManualClock clock;
	const auto window = createHeadlessWindow(400, 300, clock);
	affresco::Screen screen = affresco::Screen::fromString(R"json({
		"format": "affrescoui-screen", "version": 2, "state": { "open": false },
		"root": { "id": "root", "type": "column", "children": [
			{ "id": "opener", "height": 20, "focusable": true, "onClick": { "set": "state.open", "to": true } },
			{ "id": "outside", "height": 20, "focusable": true },
			{ "id": "box", "width": 200, "height": 80, "type": "column",
			  "overlay": { "placement": "center", "open": "state.open", "dismiss": "outside", "modal": true },
			  "children": [
				{ "id": "one", "height": 20, "focusable": true },
				{ "id": "two", "height": 20, "focusable": true, "onClick": { "dismiss": true } }
			  ] }
		] }
	})json");
	screen.build(*window);
	settle(*window, clock, screen);

	press(*window, affresco::Key::Tab);
	REQUIRE(focusedId(*window) == "opener");
	press(*window, affresco::Key::Enter);
	settle(*window, clock, screen);
	CHECK(focusedId(*window) == "one");
	press(*window, affresco::Key::Tab);
	CHECK(focusedId(*window) == "two");
	press(*window, affresco::Key::Tab); // stays inside
	CHECK(focusedId(*window) == "one");

	press(*window, affresco::Key::Tab);
	press(*window, affresco::Key::Enter); // "dismiss": true
	settle(*window, clock, screen);
	CHECK(state(screen)["open"] == false);
	CHECK(focusedId(*window) == "opener");
}

TEST_CASE("format: a modal overlay keeps the focus a step gives inside it") {
	ManualClock clock;
	const auto window = createHeadlessWindow(400, 300, clock);
	affresco::Screen screen = affresco::Screen::fromString(R"json({
		"format": "affrescoui-screen", "version": 2, "state": { "open": false },
		"root": { "id": "root", "type": "column", "children": [
			{ "id": "opener", "height": 20, "focusable": true,
			  "onClick": [{ "set": "state.open", "to": true }, { "focus": "#two" }] },
			{ "id": "box", "width": 200, "height": 80, "type": "column",
			  "overlay": { "placement": "center", "open": "state.open", "dismiss": "outside", "modal": true },
			  "children": [
				{ "id": "one", "height": 20, "focusable": true },
				{ "id": "two", "height": 20, "focusable": true }
			  ] }
		] }
	})json");
	screen.build(*window);
	settle(*window, clock, screen);

	press(*window, affresco::Key::Tab);
	REQUIRE(focusedId(*window) == "opener");
	press(*window, affresco::Key::Enter);
	settle(*window, clock, screen);
	CHECK(focusedId(*window) == "two"); // the step's, not the modal's first
}

TEST_CASE("format: focusing a container focuses its first focusable node, unless the focus is already inside it") {
	ManualClock clock;
	const auto window = createHeadlessWindow(400, 300, clock);
	affresco::Screen screen = affresco::Screen::fromString(R"json({
		"format": "affrescoui-screen", "version": 2,
		"actions": { "toBox": [{ "focus": "#box" }] },
		"root": { "id": "root", "type": "column", "children": [
			{ "id": "outside", "height": 20, "focusable": true },
			{ "id": "box", "height": 80, "type": "column", "children": [
				{ "id": "one", "height": 20, "focusable": true },
				{ "id": "two", "height": 20, "focusable": true }
			] }
		] }
	})json");
	screen.build(*window);
	settle(*window, clock, screen);

	press(*window, affresco::Key::Tab);
	REQUIRE(focusedId(*window) == "outside");
	screen.runAction("toBox", window->root(), "{}"); // the column takes no focus: its first focusable node does
	settle(*window, clock, screen);
	CHECK(focusedId(*window) == "one");

	press(*window, affresco::Key::Tab);
	REQUIRE(focusedId(*window) == "two");
	screen.runAction("toBox", window->root(), "{}"); // already inside: it stays
	settle(*window, clock, screen);
	CHECK(focusedId(*window) == "two");
}

TEST_CASE("format: scrollTo reaches a panel in an overlay that opens in the same update") {
	ManualClock clock;
	const auto window = createHeadlessWindow(400, 300, clock);
	affresco::Screen screen = affresco::Screen::fromString(R"json({
		"format": "affrescoui-screen", "version": 2, "state": { "open": false },
		"actions": { "open": [{ "set": "state.open", "to": true }, { "scrollTo": "#end" }] },
		"root": { "id": "root", "type": "column", "children": [
			{ "id": "list", "width": 200, "height": 100, "type": "column", "scroll": "vertical",
			  "overlay": { "placement": "center", "open": "state.open" },
			  "children": [
				{ "height": 300 },
				{ "id": "end", "height": 20 }
			  ] }
		] }
	})json");
	screen.build(*window);
	settle(*window, clock, screen);
	affresco::Panel* list = screen.find("list");
	REQUIRE(list);
	REQUIRE_FALSE(list->isShown());

	screen.runAction("open", window->root(), "{}");
	for(int frame = 0; frame < 4; ++frame) settle(*window, clock, screen);
	CHECK(list->isShown());
	CHECK(list->scrollOffset().y > 0.0f); // scrolled to its last child
}

TEST_CASE("format: the built-in menu bar opens a menu, runs an item, and a submenu click keeps it open") {
	ManualClock clock;
	const auto window = createHeadlessWindow(800, 600, clock);
	affresco::Screen screen = affresco::Screen::fromString(menuScreen);
	screen.build(*window);
	settle(*window, clock, screen);
	REQUIRE(window->root().children().size() > 0);

	press(*window, affresco::Key::F, 0x4); // Alt+F: the File title's mnemonic
	settle(*window, clock, screen);
	CHECK(state(screen)["bar"]["file"] == true);
	REQUIRE(window->focusedPanel()); // the modal menu gave the focus to its first item

	press(*window, affresco::Key::Enter); // New
	settle(*window, clock, screen);
	CHECK(state(screen)["n"] == 1);
	CHECK(state(screen)["last"] == "new");
	CHECK(state(screen)["bar"]["file"] == false);

	press(*window, affresco::Key::F, 0x4);
	settle(*window, clock, screen);
	press(*window, affresco::Key::R); // Recent opens its submenu
	settle(*window, clock, screen);
	CHECK(state(screen)["bar"]["fileSub"]["m2"] == true);
	CHECK(state(screen)["bar"]["file"] == true);

	REQUIRE(window->focusedPanel());
	clickOn(*window, *window->focusedPanel()); // One, in the submenu: the File menu doesn't close first
	settle(*window, clock, screen);
	CHECK(state(screen)["last"] == "one");
	CHECK(state(screen)["bar"]["file"] == false);

	press(*window, affresco::Key::F, 0x4);
	settle(*window, clock, screen);
	press(*window, affresco::Key::O); // Off is disabled
	settle(*window, clock, screen);
	CHECK(state(screen)["n"] == 2);
	press(*window, affresco::Key::Escape);
	settle(*window, clock, screen);
	CHECK(state(screen)["bar"]["file"] == false);
}

TEST_CASE("format: the built-in dialog and toast run their buttons and actions, and close") {
	ManualClock clock;
	const auto window = createHeadlessWindow(800, 600, clock);
	affresco::Screen screen = affresco::Screen::fromString(R"json({
		"format": "affrescoui-screen", "version": 2,
		"state": { "dialog": true, "toast": true, "answer": null },
		"actions": { "answer": [{ "set": "state.answer", "to": "{{choice}}" }] },
		"root": { "id": "root", "children": [
			{ "id": "ask", "use": "dialog", "with": { "open": "state.dialog", "title": "Save changes?",
			  "buttons": [{ "title": "Save", "run": "answer", "with": { "choice": "save" } }, { "title": "Cancel" }] } },
			{ "id": "note", "use": "toast", "with": { "open": "state.toast", "message": "Indexing", "progress": 0.5,
			  "actions": [{ "title": "Stop", "run": "answer", "with": { "choice": "stop" } }] } }
		] }
	})json");
	screen.build(*window);
	settle(*window, clock, screen);

	REQUIRE(window->focusedPanel()); // the dialog is modal: Save has the focus
	press(*window, affresco::Key::Enter);
	settle(*window, clock, screen);
	CHECK(state(screen)["answer"] == "save");
	CHECK(state(screen)["dialog"] == false);

	press(*window, affresco::Key::S, 0x4); // Stop, the toast's action
	settle(*window, clock, screen);
	CHECK(state(screen)["answer"] == "stop");
	CHECK(state(screen)["toast"] == false);
}

TEST_CASE("format: a menu bar's heading takes its menu's width, or one from its title") {
	ManualClock clock;
	const auto window = createHeadlessWindow(800, 600, clock);
	affresco::Screen screen = affresco::Screen::fromString(R"json({
		"format": "affrescoui-screen", "version": 2,
		"state": { "bar": {} },
		"root": { "id": "root", "children": [
			{ "id": "bar", "use": "menuBar", "width": 400, "with": { "open": "state.bar", "menus": [
				{ "name": "file", "title": "File", "items": [] },
				{ "name": "selection", "title": "Selection", "width": 70, "items": [] }
			] } }
		] }
	})json");
	screen.build(*window);
	settle(*window, clock, screen);
	const affresco::Panel* bar = screen.find("bar");
	REQUIRE(bar);
	REQUIRE(bar->children().size() == 2);
	CHECK(bar->children()[0]->bounds().width() == doctest::Approx(4 * 8 + 20));
	CHECK(bar->children()[1]->bounds().width() == doctest::Approx(70));
}

TEST_CASE("format: a dialog with primaryLast puts its first button on the right, still focused first") {
	ManualClock clock;
	const auto window = createHeadlessWindow(800, 600, clock);
	affresco::Screen screen = affresco::Screen::fromString(R"json({
		"format": "affrescoui-screen", "version": 2,
		"state": { "dialog": true, "answer": null },
		"actions": { "answer": [{ "set": "state.answer", "to": "{{choice}}" }] },
		"root": { "id": "root", "children": [
			{ "id": "ask", "use": "dialog", "with": { "open": "state.dialog", "title": "Save changes?", "primaryLast": true,
			  "buttons": [{ "title": "Save", "run": "answer", "with": { "choice": "save" } }, { "title": "Cancel" }] } }
		] }
	})json");
	screen.build(*window);
	settle(*window, clock, screen);

	const affresco::Panel* focused = window->focusedPanel();
	REQUIRE(focused);
	REQUIRE(focused->parent());
	const auto& buttons = focused->parent()->children();
	REQUIRE(buttons.size() == 2);
	CHECK(focused == buttons[0].get());                              // Save, the first
	CHECK(buttons[0]->bounds().left > buttons[1]->bounds().left); // on the right of Cancel
	press(*window, affresco::Key::Enter);
	settle(*window, clock, screen);
	CHECK(state(screen)["answer"] == "save");
}

TEST_CASE("format: the checker knows the built-in components, and a document's own component wins") {
	const json document = json::parse(menuScreen);
	const auto found = affresco::format::check(document, document["state"], {});
	REQUIRE(found);
	std::string lines;
	for(const auto& finding : *found) lines += finding.text() + "\n";
	CAPTURE(lines);
	CHECK(found->empty());

	ManualClock clock;
	const auto window = createHeadlessWindow(400, 300, clock);
	affresco::Screen screen = affresco::Screen::fromString(R"json({
		"format": "affrescoui-screen", "version": 2,
		"components": { "menu": { "node": { "id": "mine", "height": 10 } } },
		"root": { "id": "root", "children": [{ "id": "m", "use": "menu" }] }
	})json");
	screen.build(*window);
	settle(*window, clock, screen);
	CHECK(screen.find("m"));
}
