/// @file
/// @brief The desktop kit (src/components/Kit*.cpp): a toolbar's buttons, toggles and separators, the "more" menu
/// its items that don't fit move into, a status bar whose items with a "run" are buttons, and overlays that flip.

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
void press(affresco::Window& window, affresco::Key key) {
	window.routeKey({key, 0, affresco::Action::Press, {0}});
	window.routeKey({key, 0, affresco::Action::Release, {0}});
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
