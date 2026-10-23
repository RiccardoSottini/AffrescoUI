/// @file
/// @brief The desktop kit (src/components/Kit*.cpp): a toolbar's buttons, toggles and separators, the "more" menu
/// its items that don't fit move into, a status bar whose items with a "run" are buttons, overlays that flip, a
/// popover's arrow facing its anchor, a context menu, a dialog's buttons and focus trap, and a stack of toasts.

#include "Checker.hpp"
#include "Headless.hpp"

#include <affresco/affresco.hpp>

#include <doctest/doctest.h>

#include <string>
#include <string_view>

using affresco::json;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief Presses and releases a key.
/// @param window The window.
/// @param key The key.
/// @param mods The modifiers held (1 Shift, 0x4 Alt).
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

/// @brief The first panel under another, depth first, with a role and an accessible name.
/// @param panel Where to look.
/// @param role The role.
/// @param label The name.
/// @return It, or nullptr.
const affresco::Panel* findAccessible(const affresco::Panel& panel, std::string_view role, std::string_view label) {
	if(panel.accessibleRole() == role && panel.accessibleLabel() == label) return &panel;
	for(const auto& child : panel.children())
		if(const affresco::Panel* found = findAccessible(*child, role, label)) return found;
	return nullptr;
}

/// @brief How many panels under another, itself included, have a role.
/// @param panel Where to count.
/// @param role The role.
/// @return Their number.
int countRole(const affresco::Panel& panel, std::string_view role) {
	int count = panel.accessibleRole() == role ? 1 : 0;
	for(const auto& child : panel.children()) count += countRole(*child, role);
	return count;
}

/// @brief The accessible name of the focused panel.
/// @param window The window.
/// @return It, or "" if none has the focus.
std::string focusedLabel(const affresco::Window& window) {
	return window.focusedPanel() ? window.focusedPanel()->accessibleLabel() : "";
}

/// @brief The screen's state.
/// @param screen The screen.
/// @return It.
json state(const affresco::Screen& screen) { return json::parse(screen.stateJson()); }

/// @brief Lays a built screen out twice, running what waits for the next turn.
/// @param window The window.
/// @param clock Its clock.
void settle(affresco::Window& window, ManualClock& clock) {
	window.render();
	clock.advance(1.0);
	window.render();
}

/// @brief A toolbar of icon buttons, a toggle and separators, as wide as the window, over a status bar.
/// @param width The toolbar's width.
/// @return The document.
std::string barsScreen(int width) {
	return R"json({
		"format": "affrescoui-screen", "version": 2,
		"state": { "n": 0, "last": null, "bold": true, "tb": {} },
		"actions": {
			"add": [{ "increment": "state.n", "by": 1 }, { "set": "state.last", "to": "{{what ?? 'add'}}" }]
		},
		"root": { "id": "root", "children": [{ "type": "column", "fill": true, "children": [
			{ "id": "tb", "use": "toolbar", "with": { "open": "state.tb", "width": )json" +
	       std::to_string(width) + R"json(, "items": [
				{ "title": "Cut", "icon": "scissors", "keyLabel": "Ctrl+X", "run": "add", "with": { "what": "cut" } },
				{ "title": "Copy", "icon": "copy", "run": "add", "with": { "what": "copy" } },
				{ "separator": true },
				{ "title": "Bold", "icon": "bold", "checked": "{{state.bold}}", "run": "add", "with": { "what": "bold" } },
				{ "title": "Italic", "icon": "italic", "enabled": false, "run": "add" },
				{ "separator": true },
				{ "title": "Paste", "icon": "clipboard", "run": "add", "with": { "what": "paste" } }
			] } },
			{ "grow": 1 },
			{ "id": "status", "use": "statusBar", "width": "100%", "with": { "items": [
				{ "text": "Ready" },
				{ "text": "UTF-8", "side": "end", "tooltip": "Encoding", "run": "add", "with": { "what": "encoding" } }
			] } }
		] }] }
	})json";
}

/// @brief The first panel under another, depth first, with an id.
/// @param panel Where to look.
/// @param id The id.
/// @return It, or nullptr.
const affresco::Panel* findId(const affresco::Panel& panel, std::string_view id) {
	if(panel.id() == id) return &panel;
	for(const auto& child : panel.children())
		if(const affresco::Panel* found = findId(*child, id)) return found;
	return nullptr;
}

/// @brief A button at the top or the bottom of the window with an open, flipping overlay below it: a tip at the
/// overlay's top over a body.
/// @param atBottom Whether the button is at the bottom.
/// @return The document.
std::string flipScreen(bool atBottom) {
	return R"json({
		"format": "affrescoui-screen", "version": 2,
		"root": { "id": "root", "children": [{ "type": "column", "fill": true, "children": [)json" +
	       std::string(atBottom ? R"json({ "grow": 1 },)json" : "") + R"json(
			{ "id": "anchor", "width": 80, "height": 30, "children": [
				{ "id": "pop", "width": 100, "height": 60,
				  "overlay": { "placement": "below", "flip": true, "open": true }, "children": [
					{ "id": "tip", "x": 10, "width": 10, "height": 10 },
					{ "id": "body", "y": 5, "width": 100, "height": 55 }
				] }
			] }
		] }] }
	})json";
}

/// @brief A button near the start of the top or the bottom of a 400 by 200 window with an open popover, in either
/// writing direction.
/// @param atBottom Whether the button is at the bottom.
/// @param rightToLeft Whether the screen is right to left.
/// @return The document.
std::string popoverScreen(bool atBottom, bool rightToLeft) {
	return R"json({
		"format": "affrescoui-screen", "version": 2,
		"state": { "pop": true },
		"root": { "id": "root", "children": [{ "type": "column", "fill": true, "dir": ")json" +
	       std::string(rightToLeft ? "rtl" : "ltr") + R"json(", "children": [)json" +
	       std::string(atBottom ? R"json({ "grow": 1 },)json" : "") + R"json(
			{ "type": "row", "width": 400, "height": 30, "children": [{ "width": 60 },
				{ "id": "anchor", "width": 80, "height": 30, "focusable": true, "label": "Share", "children": [
					{ "use": "popover", "with": { "open": "state.pop", "title": "Share", "text": "Anyone with the link." } }
				] }
			] }
		] }] }
	})json";
}

/// @brief A focusable area that opens a context menu where the pointer is: a checked item, one with an icon, a
/// disabled one, a separator and a submenu.
/// @param icons Whether the menu shows its items' icons.
/// @return The document.
std::string contextMenuScreen(bool icons) {
	return R"json({
		"format": "affrescoui-screen", "version": 2,
		"state": { "ctx": { "open": false, "x": 0, "y": 0 }, "wrap": true, "n": 0 },
		"actions": { "add": [{ "increment": "state.n", "by": 1 }] },
		"root": { "id": "root", "children": [{ "type": "column", "fill": true, "children": [{ "height": 40 },
			{ "id": "doc", "x": 50, "width": 300, "height": 200, "focusable": true, "label": "Document",
			  "onContextMenu": { "set": "state.ctx", "to": { "open": true, "x": "{{event.x}}", "y": "{{event.y}}" } },
			  "children": [{ "use": "contextMenu", "with": { "open": "state.ctx.open", "x": "{{state.ctx.x}}",
				"y": "{{state.ctx.y}}", "icons": )json" +
	       std::string(icons ? "true" : "false") + R"json(, "items": [
					{ "title": "Word wrap", "checked": "{{state.wrap}}", "run": "add" },
					{ "title": "Copy", "icon": "copy", "keyLabel": "Ctrl+C", "run": "add" },
					{ "title": "Paste", "enabled": false, "run": "add" },
					{ "separator": true },
					{ "title": "Zoom", "submenu": [{ "title": "In", "run": "add" }, { "title": "Out", "run": "add" }] }
				] } }] }
		] }] }
	})json";
}

} // namespace

TEST_CASE("format: a flipping overlay goes above its anchor when only that side has room, mirrored") {
	for(const bool atBottom : {false, true}) {
		CAPTURE(atBottom);
		ManualClock clock;
		const auto window = createHeadlessWindow(300, 200, clock);
		affresco::Screen screen = affresco::Screen::fromString(flipScreen(atBottom));
		screen.build(*window);
		settle(*window, clock);
		const affresco::Panel* anchor = findId(window->root(), "anchor");
		const affresco::Panel* pop = findId(window->root(), "pop");
		const affresco::Panel* tip = findId(window->root(), "tip");
		const affresco::Panel* body = findId(window->root(), "body");
		REQUIRE(anchor);
		REQUIRE(pop);
		REQUIRE(tip);
		REQUIRE(body);
		CHECK(pop->isFlipped() == atBottom);
		if(atBottom) {
			CHECK(pop->frame().bottom == doctest::Approx(anchor->frame().top));
			CHECK(tip->frame().bottom == doctest::Approx(pop->frame().bottom)); // still facing the anchor
			CHECK(body->frame().top == doctest::Approx(pop->frame().top));
		} else {
			CHECK(pop->frame().top == doctest::Approx(anchor->frame().bottom));
			CHECK(tip->frame().top == doctest::Approx(pop->frame().top));
			CHECK(body->frame().bottom == doctest::Approx(pop->frame().bottom));
		}
		CHECK(tip->frame().left == doctest::Approx(pop->frame().left + 10.0f)); // the other axis stays
	}
}

TEST_CASE("format: a toolbar wide enough shows every item, with toggles selected and separators out of reach") {
	ManualClock clock;
	const auto window = createHeadlessWindow(560, 300, clock);
	affresco::Screen screen = affresco::Screen::fromString(barsScreen(560));
	screen.build(*window);
	settle(*window, clock);
	affresco::Panel& root = window->root();

	CHECK(findAccessible(root, "toolbar", "Toolbar") != nullptr);
	CHECK(findAccessible(root, "button", "Paste") != nullptr);
	CHECK(findAccessible(root, "button", "More") == nullptr);
	CHECK(countRole(root, "separator") == 2);

	const affresco::Panel* bold = findAccessible(root, "button", "Bold");
	REQUIRE(bold);
	CHECK(bold->accessibleState().selected == true);
	CHECK(bold->tooltip() == "Bold");
	const affresco::Panel* cut = findAccessible(root, "button", "Cut");
	REQUIRE(cut);
	CHECK(cut->accessibleState().selected == false);
	CHECK(cut->tooltip() == "Cut (Ctrl+X)");
	const affresco::Panel* italic = findAccessible(root, "button", "Italic");
	REQUIRE(italic);
	CHECK(italic->accessibleState().disabled);

	clickOn(*window, *cut);
	CHECK(state(screen)["n"] == 1);
	CHECK(state(screen)["last"] == "cut");
	clickOn(*window, *italic); // disabled: nothing runs
	CHECK(state(screen)["n"] == 1);
}

TEST_CASE("format: a toolbar's arrows move between its buttons, past separators") {
	ManualClock clock;
	const auto window = createHeadlessWindow(560, 300, clock);
	affresco::Screen screen = affresco::Screen::fromString(barsScreen(560));
	screen.build(*window);
	settle(*window, clock);

	press(*window, affresco::Key::Tab);
	CHECK(focusedLabel(*window) == "Cut");
	press(*window, affresco::Key::Right);
	CHECK(focusedLabel(*window) == "Copy");
	press(*window, affresco::Key::Right);
	CHECK(focusedLabel(*window) == "Bold");
	press(*window, affresco::Key::Enter);
	CHECK(state(screen)["last"] == "bold");
}

TEST_CASE("format: a narrow toolbar moves what doesn't fit into its \"more\" menu, without a trailing separator") {
	ManualClock clock;
	const auto window = createHeadlessWindow(560, 300, clock);
	affresco::Screen screen = affresco::Screen::fromString(barsScreen(130));
	screen.build(*window);
	settle(*window, clock);
	affresco::Panel& root = window->root();

	// 30 + 30 + 11 fit in 130 - 10 - 32; the separator would then end the row, so it moves too
	CHECK(findAccessible(root, "button", "Copy") != nullptr);
	CHECK(findAccessible(root, "button", "Bold") == nullptr);
	CHECK(findAccessible(root, "button", "Paste") == nullptr);
	CHECK(countRole(root, "separator") == 0);
	const affresco::Panel* more = findAccessible(root, "button", "More");
	REQUIRE(more);

	clickOn(*window, *more);
	settle(*window, clock);
	CHECK(state(screen)["tb"]["more"] == true);
	REQUIRE(window->focusedPanel()); // the modal menu gave the focus to its first item, Bold
	press(*window, affresco::Key::Enter);
	settle(*window, clock);
	CHECK(state(screen)["last"] == "bold");
	CHECK(state(screen)["tb"]["more"] == false);
}

TEST_CASE("format: a status bar's items with a run are buttons, the others text") {
	ManualClock clock;
	const auto window = createHeadlessWindow(560, 300, clock);
	affresco::Screen screen = affresco::Screen::fromString(barsScreen(560));
	screen.build(*window);
	settle(*window, clock);
	affresco::Panel& root = window->root();

	CHECK(findAccessible(root, "status", "Status") != nullptr);
	const affresco::Panel* ready = findAccessible(root, "text", "Ready");
	REQUIRE(ready);
	const affresco::Panel* encoding = findAccessible(root, "button", "Encoding");
	REQUIRE(encoding);
	for(int tab = 0; tab < 6 && focusedLabel(*window) != "Encoding"; ++tab) {
		press(*window, affresco::Key::Tab);
		CHECK(focusedLabel(*window) != "Ready");
	}
	CHECK(focusedLabel(*window) == "Encoding");
	CHECK(encoding->frame().right > 500); // at the end
	clickOn(*window, *encoding);
	CHECK(state(screen)["last"] == "encoding");
}

TEST_CASE("format: a popover's arrow faces its anchor, above or below it and at its start in either direction") {
	for(const bool atBottom : {false, true})
		for(const bool rightToLeft : {false, true}) {
			CAPTURE(atBottom);
			CAPTURE(rightToLeft);
			ManualClock clock;
			const auto window = createHeadlessWindow(400, 200, clock);
			affresco::Screen screen = affresco::Screen::fromString(popoverScreen(atBottom, rightToLeft));
			screen.build(*window);
			settle(*window, clock);
			const affresco::Panel* anchor = findId(window->root(), "anchor");
			const affresco::Panel* pop = findAccessible(window->root(), "dialog", "Share");
			REQUIRE(anchor);
			REQUIRE(pop);
			REQUIRE(pop->children().size() == 3);
			const affresco::Rect& arrow = pop->children()[0]->frame();
			const affresco::Rect& body = pop->children()[1]->frame();
			CHECK(pop->isFlipped() == atBottom);
			if(atBottom) {
				CHECK(pop->frame().bottom == doctest::Approx(anchor->frame().top));
				CHECK(arrow.bottom == doctest::Approx(pop->frame().bottom - 2.0f));
				CHECK(body.top == doctest::Approx(pop->frame().top));
			} else {
				CHECK(pop->frame().top == doctest::Approx(anchor->frame().bottom));
				CHECK(arrow.top == doctest::Approx(pop->frame().top + 2.0f));
				CHECK(body.bottom == doctest::Approx(pop->frame().bottom));
			}
			CHECK(anchor->frame().left == doctest::Approx(rightToLeft ? 260.0f : 60.0f));
			if(rightToLeft) {
				CHECK(pop->frame().right == doctest::Approx(anchor->frame().right));
				CHECK(arrow.right == doctest::Approx(pop->frame().right - 16.0f));
			} else {
				CHECK(pop->frame().left == doctest::Approx(anchor->frame().left));
				CHECK(arrow.left == doctest::Approx(pop->frame().left + 16.0f));
			}
		}
}

TEST_CASE("format: a popover closes on Escape and on a click outside it") {
	for(const bool escape : {true, false}) {
		CAPTURE(escape);
		ManualClock clock;
		const auto window = createHeadlessWindow(400, 200, clock);
		affresco::Screen screen = affresco::Screen::fromString(popoverScreen(false, false));
		screen.build(*window);
		settle(*window, clock);
		REQUIRE(findAccessible(window->root(), "dialog", "Share"));
		if(escape)
			press(*window, affresco::Key::Escape);
		else
			for(const affresco::Action action : {affresco::Action::Press, affresco::Action::Release})
				window->routeMouseButton({affresco::MouseButton::Left, action, {}, affresco::Vec2{390.0f, 190.0f}});
		settle(*window, clock);
		CHECK(state(screen)["pop"] == false);
	}
}

TEST_CASE("format: a context menu opens where the pointer is on a right click") {
	ManualClock clock;
	const auto window = createHeadlessWindow(600, 400, clock);
	affresco::Screen screen = affresco::Screen::fromString(contextMenuScreen(false));
	screen.build(*window);
	settle(*window, clock);
	CHECK(state(screen)["ctx"]["open"] == false);
	for(const affresco::Action action : {affresco::Action::Press, affresco::Action::Release})
		window->routeMouseButton({affresco::MouseButton::Right, action, {}, affresco::Vec2{120.0f, 90.0f}});
	settle(*window, clock);
	CHECK(state(screen)["ctx"]["open"] == true);
	const affresco::Panel* wrap = findAccessible(window->root(), "menuitem", "Word wrap");
	REQUIRE(wrap);
	CHECK(wrap->parent()->frame().left == doctest::Approx(120.0f));
	CHECK(wrap->parent()->frame().top == doctest::Approx(90.0f));
	CHECK(countRole(window->root(), "separator") == 1);
}

TEST_CASE("format: Shift+F10 and the Menu key open a context menu at the focused node") {
	for(const bool menuKey : {false, true}) {
		CAPTURE(menuKey);
		ManualClock clock;
		const auto window = createHeadlessWindow(600, 400, clock);
		affresco::Screen screen = affresco::Screen::fromString(contextMenuScreen(false));
		screen.build(*window);
		settle(*window, clock);
		const affresco::Panel* doc = findId(window->root(), "doc");
		REQUIRE(doc);
		clickOn(*window, *doc);
		REQUIRE(focusedLabel(*window) == "Document");
		const affresco::Key key = menuKey ? affresco::Key::Menu : affresco::Key::F10;
		const affresco::Mods mods{menuKey ? 0 : 1};
		window->routeKey({key, 0, affresco::Action::Press, mods});
		window->routeKey({key, 0, affresco::Action::Release, mods});
		settle(*window, clock);
		CHECK(state(screen)["ctx"]["open"] == true);
		CHECK(state(screen)["ctx"]["x"] == doctest::Approx(doc->frame().left));
		CHECK(state(screen)["ctx"]["y"] == doctest::Approx(doc->frame().bottom));
		CHECK(findAccessible(window->root(), "menuitem", "Word wrap"));
	}
}

TEST_CASE("format: a context menu's checked item is selected, its icons opt in, and Enter runs an item") {
	for(const bool icons : {false, true}) {
		CAPTURE(icons);
		ManualClock clock;
		const auto window = createHeadlessWindow(600, 400, clock);
		affresco::Screen screen = affresco::Screen::fromString(contextMenuScreen(icons));
		screen.build(*window);
		settle(*window, clock);
		for(const affresco::Action action : {affresco::Action::Press, affresco::Action::Release})
			window->routeMouseButton({affresco::MouseButton::Right, action, {}, affresco::Vec2{120.0f, 90.0f}});
		settle(*window, clock);
		const affresco::Panel* wrap = findAccessible(window->root(), "menuitem", "Word wrap");
		const affresco::Panel* copy = findAccessible(window->root(), "menuitem", "Copy");
		const affresco::Panel* paste = findAccessible(window->root(), "menuitem", "Paste");
		REQUIRE(wrap);
		REQUIRE(copy);
		REQUIRE(paste);
		CHECK(wrap->accessibleState().selected == true);
		CHECK_FALSE(copy->accessibleState().selected.has_value());
		CHECK(paste->accessibleState().disabled);
		CHECK(wrap->children().size() == 3); // the check, the title and the key label
		CHECK(copy->children().size() == (icons ? 3u : 2u));
		// the titles line up after the check, with or without icons
		const affresco::Rect& title = copy->children()[icons ? 1 : 0]->frame();
		CHECK(title.left == doctest::Approx(copy->frame().left + 34.0f));
		window->render();
		press(*window, affresco::Key::Down);
		press(*window, affresco::Key::Enter);
		settle(*window, clock);
		CHECK(state(screen)["n"] == 1);
		CHECK(state(screen)["ctx"]["open"] == false);
	}
}

/// @brief A dialog of three buttons over a button outside it, and a toast with an action.
/// @return The document.
std::string dialogScreen() {
	return R"json({
		"format": "affrescoui-screen", "version": 2,
		"state": { "dlg": true, "toast": true, "answer": null },
		"actions": { "answer": [{ "set": "state.answer", "to": "{{what}}" }] },
		"root": { "id": "root", "children": [{ "type": "column", "fill": true, "children": [
			{ "width": 80, "height": 24, "focusable": true, "label": "Outside",
			  "onClick": { "set": "state.answer", "to": "outside" } },
			{ "use": "dialog", "with": { "open": "state.dlg", "title": "Save changes?", "message": "The file has changes.",
			  "buttons": [{ "title": "Save", "run": "answer", "with": { "what": "save" } },
			              { "title": "Discard", "run": "answer", "with": { "what": "discard" } },
			              { "title": "Cancel" }] } },
			{ "use": "toast", "with": { "open": "state.toast", "message": "Saved",
			  "actions": [{ "title": "Undo", "run": "answer", "with": { "what": "undo" } }] } }
		] }] }
	})json";
}

TEST_CASE("format: a dialog's buttons are named buttons, and Tab and Shift+Tab stay in it") {
	for(const int mods : {0, 1}) {
		CAPTURE(mods);
		ManualClock clock;
		const auto window = createHeadlessWindow(600, 400, clock);
		affresco::Screen screen = affresco::Screen::fromString(dialogScreen());
		screen.build(*window);
		settle(*window, clock);
		const affresco::Panel* dialog = findAccessible(window->root(), "dialog", "Save changes?");
		REQUIRE(dialog);
		CHECK(countRole(*dialog, "button") == 3);
		CHECK(findAccessible(*dialog, "button", "Discard"));
		for(int i = 0; i < 5; ++i) {
			press(*window, affresco::Key::Tab, mods);
			const std::string label = focusedLabel(*window);
			CAPTURE(label);
			CHECK((label == "Save" || label == "Discard" || label == "Cancel"));
		}
		CHECK(state(screen)["answer"].is_null());
	}
}

TEST_CASE("format: Escape cancels a dialog, and the arrows and Enter choose a button") {
	for(const bool escape : {true, false}) {
		CAPTURE(escape);
		ManualClock clock;
		const auto window = createHeadlessWindow(600, 400, clock);
		affresco::Screen screen = affresco::Screen::fromString(dialogScreen());
		screen.build(*window);
		settle(*window, clock);
		if(escape) {
			press(*window, affresco::Key::Escape);
			settle(*window, clock);
			CHECK(state(screen)["answer"].is_null());
		} else {
			for(int i = 0; i < 3 && focusedLabel(*window) != "Save"; ++i) press(*window, affresco::Key::Tab);
			REQUIRE(focusedLabel(*window) == "Save");
			press(*window, affresco::Key::Right);
			CHECK(focusedLabel(*window) == "Discard");
			press(*window, affresco::Key::Enter);
			settle(*window, clock);
			CHECK(state(screen)["answer"] == "discard");
		}
		CHECK(state(screen)["dlg"] == false);
	}
}

TEST_CASE("format: a toast's close button and actions are named buttons") {
	ManualClock clock;
	const auto window = createHeadlessWindow(600, 400, clock);
	affresco::Screen screen = affresco::Screen::fromString(dialogScreen());
	screen.build(*window);
	settle(*window, clock);
	press(*window, affresco::Key::Escape); // the dialog
	settle(*window, clock);
	const affresco::Panel* toast = findAccessible(window->root(), "status", "");
	REQUIRE(toast);
	CHECK(findAccessible(*toast, "button", "Undo"));
	const affresco::Panel* close = findAccessible(*toast, "button", "Close");
	REQUIRE(close);
	bool reached = false;
	for(int i = 0; i < 4 && !reached; ++i) {
		press(*window, affresco::Key::Tab);
		reached = focusedLabel(*window) == "Close";
	}
	CHECK(reached);
	clickOn(*window, *close);
	settle(*window, clock);
	CHECK(state(screen)["toast"] == false);
	CHECK(state(screen)["answer"].is_null());
}

/// @brief A stack of toasts showing the last three of four, one with a progress bar and one with an action.
/// @return The document.
std::string toastsScreen() {
	return R"json({
		"format": "affrescoui-screen", "version": 2,
		"state": { "answer": null, "notes": [
			{ "message": "One" },
			{ "message": "Two", "progress": 0.5 },
			{ "message": "Three", "actions": [{ "title": "Undo", "run": "answer", "with": { "what": "three" } }] },
			{ "message": "Four" }
		] },
		"actions": { "answer": [{ "set": "state.answer", "to": "{{what}}" }] },
		"root": { "id": "root", "children": [{ "type": "column", "fill": true, "children": [
			{ "use": "toasts", "with": { "name": "notes", "limit": 3 } }
		] }] }
	})json";
}

/// @brief The messages of the toasts in a state.
/// @param state The state.
/// @return They, joined by spaces.
std::string messages(const json& state) {
	std::string joined;
	for(const json& note : state["notes"]) joined += (joined.empty() ? "" : " ") + note["message"].get<std::string>();
	return joined;
}

TEST_CASE("format: a stack of toasts shows the last ones, newest at the bottom") {
	ManualClock clock;
	const auto window = createHeadlessWindow(600, 400, clock);
	affresco::Screen screen = affresco::Screen::fromString(toastsScreen());
	screen.build(*window);
	settle(*window, clock);
	const affresco::Panel* stack = findAccessible(window->root(), "region", "Notifications");
	REQUIRE(stack);
	CHECK(countRole(*stack, "status") == 3);
	CHECK_FALSE(findAccessible(*stack, "status", "One"));
	const affresco::Panel* two = findAccessible(*stack, "status", "Two");
	const affresco::Panel* four = findAccessible(*stack, "status", "Four");
	REQUIRE(two);
	REQUIRE(four);
	CHECK(stack->frame().bottom == doctest::Approx(384.0f));
	CHECK(stack->frame().right == doctest::Approx(584.0f));
	CHECK(stack->frame().bottom - stack->frame().top == doctest::Approx(72.0f + 64.0f + 64.0f + 2 * 8.0f));
	CHECK(two->frame().top == doctest::Approx(stack->frame().top));
	CHECK(four->frame().bottom == doctest::Approx(stack->frame().bottom));
}

TEST_CASE("format: a toast's close button removes it, and an action removes it and runs") {
	for(const bool action : {false, true}) {
		CAPTURE(action);
		ManualClock clock;
		const auto window = createHeadlessWindow(600, 400, clock);
		affresco::Screen screen = affresco::Screen::fromString(toastsScreen());
		screen.build(*window);
		settle(*window, clock);
		const affresco::Panel* three = findAccessible(window->root(), "status", "Three");
		REQUIRE(three);
		const affresco::Panel* button = findAccessible(*three, "button", action ? "Undo" : "Close");
		REQUIRE(button);
		clickOn(*window, *button);
		settle(*window, clock);
		CHECK(messages(state(screen)) == "One Two Four");
		CHECK(state(screen)["answer"] == (action ? json("three") : json(nullptr)));
		CHECK(findAccessible(window->root(), "status", "One"));
	}
}

TEST_CASE("format: a stack of toasts closes when its last one goes") {
	ManualClock clock;
	const auto window = createHeadlessWindow(600, 400, clock);
	affresco::Screen screen = affresco::Screen::fromString(toastsScreen());
	screen.build(*window);
	settle(*window, clock);
	for(int i = 0; i < 4; ++i) {
		const affresco::Panel* close = findAccessible(window->root(), "button", "Close");
		REQUIRE(close);
		clickOn(*window, *close);
		settle(*window, clock);
	}
	CHECK(state(screen)["notes"].empty());
	CHECK(countRole(window->root(), "status") == 0);
	CHECK_FALSE(findAccessible(window->root(), "button", "Close"));
}
