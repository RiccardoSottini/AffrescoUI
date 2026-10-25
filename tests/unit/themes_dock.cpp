/// @file
/// @brief The desktop kit's dock (modules/themes/components/dock.json): areas showing their active panel with the use's
/// children, a panel moved from an area's menu, collapsed and maximised areas, edges resized by dragging and from the
/// keyboard in either writing direction; and components whose slots get the use's children in every place.

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
	window.routeKey({key, 0, affresco::Action::Press, {}});
	window.routeKey({key, 0, affresco::Action::Release, {}});
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

/// @brief Drags the pointer from one place to another with the left button, in a few moves.
/// @param window The window.
/// @param from Where the button goes down.
/// @param to Where it comes up.
void drag(affresco::Window& window, affresco::Vec2 from, affresco::Vec2 to) {
	window.routeMouseButton({affresco::MouseButton::Left, affresco::Action::Press, {}, from});
	for(int step = 1; step <= 4; ++step) {
		const float t = static_cast<float>(step) / 4.0f;
		window.routeMouseMove({from.x + (to.x - from.x) * t, from.y + (to.y - from.y) * t});
	}
	window.routeMouseButton({affresco::MouseButton::Left, affresco::Action::Release, {}, to});
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

/// @brief How many shown panels under another, itself included, have a role.
/// @param panel Where to count.
/// @param role The role.
/// @return Their number.
int countRole(const affresco::Panel& panel, std::string_view role) {
	if(!panel.isShown()) return 0;
	int count = panel.accessibleRole() == role ? 1 : 0;
	for(const auto& child : panel.children()) count += countRole(*child, role);
	return count;
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

/// @brief A dock filling an 800 by 500 window: panels A and B at the left, C at the right and D at the bottom, each
/// area showing a label with its panel's id.
/// @param dir The writing direction.
/// @return The document.
std::string dockScreen(std::string_view dir = "ltr") {
	return R"json({
		"format": "affrescoui-screen", "version": 2,
		"state": { "dock": {
			"left": { "tabs": ["a", "b"], "active": "a" }, "right": { "tabs": ["c"] },
			"bottom": { "tabs": ["d"] }, "center": { "tabs": [] }
		} },
		"root": { "id": "root", "children": [{ "fill": true, "dir": ")json" +
	       std::string(dir) + R"json(", "children": [{ "fill": true, "use": "dock", "with": {
			"sizes": { "left": 200, "right": 200, "bottom": 150 },
			"panels": { "a": { "title": "A" }, "b": { "title": "B" }, "c": { "title": "C" }, "d": { "title": "D" } } },
			"children": [{ "type": "label", "width": 100, "height": 20, "role": "text", "label": "{{'shows ' + panel}}",
			               "text": "{{'shows ' + panel}}" }] }] }] }
	})json";
}

/// @brief Builds a screen in a headless 800 by 500 window and lays it out.
struct Docked {
	ManualClock clock;                                                    ///< The window's clock.
	std::unique_ptr<affresco::Window> window = createHeadlessWindow(800, 500, clock); ///< The window.
	affresco::Screen screen;                                              ///< The screen.

	/// @brief Builds it.
	/// @param document The document.
	explicit Docked(const std::string& document) : screen(affresco::Screen::fromString(document)) {
		screen.build(*window);
		settle(*window, clock);
	}

	/// @brief The area showing a panel (the region named after it).
	/// @param title The panel's title, or the area's when it has none.
	/// @return It.
	const affresco::Panel& area(std::string_view title) const {
		const affresco::Panel* found = findAccessible(window->root(), "region", title);
		REQUIRE(found);
		return *found;
	}

	/// @brief Clicks a button of an area.
	/// @param title The area's region name.
	/// @param button The button's name.
	void click(std::string_view title, std::string_view button) {
		const affresco::Panel* found = findAccessible(area(title), "button", button);
		REQUIRE(found);
		clickOn(*window, *found);
		settle(*window, clock);
	}
};

} // namespace

TEST_CASE("format: a dock shows each area's active panel with the use's children, and the checker accepts it") {
	Docked dock(dockScreen());
	CHECK(countRole(dock.window->root(), "region") == 4);
	CHECK(findAccessible(dock.area("A"), "text", "shows a"));
	CHECK(findAccessible(dock.area("C"), "text", "shows c"));
	CHECK(findAccessible(dock.area("D"), "text", "shows d"));
	CHECK(findAccessible(dock.area("Center"), "text", "shows "));
	const affresco::Rect left = dock.area("A").frame();
	const affresco::Rect bottom = dock.area("D").frame();
	CHECK(left.left == doctest::Approx(0.0f));
	CHECK(left.right - left.left == doctest::Approx(200.0f));
	CHECK(bottom.bottom - bottom.top == doctest::Approx(150.0f));

	const json document = json::parse(dockScreen());
	const auto findings = affresco::format::check(document, document["state"]);
	REQUIRE(findings);
	for(const auto& finding : *findings) CAPTURE(finding.text());
	CHECK(findings->empty());
}

TEST_CASE("format: a dock's area menu moves its panel to another area, and an empty area goes") {
	Docked dock(dockScreen());
	dock.click("D", "Move to");
	const affresco::Panel* menu = findAccessible(dock.area("D"), "menu", "Move to");
	REQUIRE(menu);
	CHECK(countRole(*menu, "menuitem") == 3);
	const affresco::Panel* left = findAccessible(*menu, "menuitem", "Left");
	REQUIRE(left);
	clickOn(*dock.window, *left);
	settle(*dock.window, dock.clock);
	const json docked = state(dock.screen)["dock"];
	CHECK(docked["left"]["tabs"] == json{"a", "b", "d"});
	CHECK(docked["left"]["active"] == "d");
	CHECK(docked["bottom"]["tabs"].empty());
	CHECK(docked["menu"].is_null());
	CHECK(countRole(dock.window->root(), "region") == 3);
	CHECK(findAccessible(dock.area("D"), "text", "shows d"));
}

TEST_CASE("format: a dock's side area collapses to a strip of its panels, and one maximises over the others") {
	Docked dock(dockScreen());
	dock.click("A", "Collapse");
	CHECK(state(dock.screen)["dock"]["left"]["collapsed"] == true);
	const affresco::Panel* strip = findAccessible(dock.window->root(), "toolbar", "Left");
	REQUIRE(strip);
	CHECK(strip->frame().right - strip->frame().left == doctest::Approx(40.0f));
	const affresco::Panel* b = findAccessible(*strip, "button", "B");
	REQUIRE(b);
	clickOn(*dock.window, *b);
	settle(*dock.window, dock.clock);
	CHECK(state(dock.screen)["dock"]["left"]["active"] == "b");
	CHECK(state(dock.screen)["dock"]["left"]["collapsed"] == false);

	dock.click("C", "Maximize");
	CHECK(state(dock.screen)["dock"]["maximized"] == "right");
	CHECK(countRole(dock.window->root(), "region") == 1);
	CHECK(dock.area("C").frame().right - dock.area("C").frame().left == doctest::Approx(800.0f));
	dock.click("C", "Restore");
	CHECK(state(dock.screen)["dock"]["maximized"].is_null());
	CHECK(countRole(dock.window->root(), "region") == 4);
}

TEST_CASE("format: a dock's areas resize by dragging their inner edge, mirrored right to left") {
	for(const bool rightToLeft : {false, true}) {
		CAPTURE(rightToLeft);
		Docked dock(dockScreen(rightToLeft ? "rtl" : "ltr"));
		const float edge = rightToLeft ? 602.0f : 198.0f;
		const float outward = rightToLeft ? -60.0f : 60.0f;
		drag(*dock.window, {edge, 200.0f}, {edge + outward, 200.0f});
		settle(*dock.window, dock.clock);
		CHECK(state(dock.screen)["dock"]["sizes"]["left"] == 260);
		CHECK(dock.area("A").frame().right - dock.area("A").frame().left == doctest::Approx(260.0f));

		const float top = dock.area("D").frame().top;
		drag(*dock.window, {400.0f, top + 2.0f}, {400.0f, top - 40.0f});
		settle(*dock.window, dock.clock);
		CHECK(state(dock.screen)["dock"]["sizes"]["bottom"] == 192);

		drag(*dock.window, {400.0f, 100.0f}, {300.0f, 100.0f}); // inside the centre: nothing
		settle(*dock.window, dock.clock);
		CHECK(state(dock.screen)["dock"]["sizes"]["left"] == 260);
	}
}

TEST_CASE("format: a dock's edges resize from the keyboard, the arrows following the writing direction") {
	for(const bool rightToLeft : {false, true}) {
		CAPTURE(rightToLeft);
		Docked dock(dockScreen(rightToLeft ? "rtl" : "ltr"));
		affresco::Panel* sash = const_cast<affresco::Panel*>(findAccessible(dock.area("A"), "separator", "Resize"));
		REQUIRE(sash);
		dock.window->setFocus(sash);
		press(*dock.window, affresco::Key::Right);
		settle(*dock.window, dock.clock);
		// the dock's "dir" is "inherit": its arrows go as left to right unless told
		CHECK(state(dock.screen)["dock"]["sizes"]["left"] == 216);

		affresco::Panel* bottom = const_cast<affresco::Panel*>(findAccessible(dock.area("D"), "separator", "Resize"));
		REQUIRE(bottom);
		dock.window->setFocus(bottom);
		press(*dock.window, affresco::Key::Up);
		press(*dock.window, affresco::Key::Up);
		press(*dock.window, affresco::Key::Down);
		settle(*dock.window, dock.clock);
		CHECK(state(dock.screen)["dock"]["sizes"]["bottom"] == 166);
	}

	std::string told = dockScreen("rtl");
	told.insert(told.find(R"("sizes")"), R"("dir": "rtl", )");
	Docked dock(told);
	affresco::Panel* sash = const_cast<affresco::Panel*>(findAccessible(dock.area("A"), "separator", "Resize"));
	REQUIRE(sash);
	dock.window->setFocus(sash);
	press(*dock.window, affresco::Key::Left); // the left area is at the right: Left makes it wider
	settle(*dock.window, dock.clock);
	CHECK(state(dock.screen)["dock"]["sizes"]["left"] == 216);
}

TEST_CASE("format: every slot of a component gets the use's children, and a use without children empties them") {
	const std::string document = R"json({
		"format": "affrescoui-screen", "version": 2,
		"components": { "twice": { "params": {}, "node": { "type": "column", "width": 100, "height": 60, "children": [
			{ "repeat": { "for": "side", "in": ["one", "two"] }, "width": 100, "height": 30, "label": "{{side}}",
			  "role": "group", "children": [{ "slot": true }] }
		] } } },
		"root": { "id": "root", "children": [
			{ "use": "twice", "children": [{ "type": "label", "width": 90, "height": 20, "role": "text",
			                               "label": "{{'in ' + side}}" }] },
			{ "use": "twice", "y": 100 }
		] }
	})json";
	Docked built(document);
	const affresco::Panel* one = findAccessible(built.window->root(), "group", "one");
	const affresco::Panel* two = findAccessible(built.window->root(), "group", "two");
	REQUIRE(one);
	REQUIRE(two);
	CHECK(findAccessible(*one, "text", "in one"));
	CHECK(findAccessible(*two, "text", "in two"));
	const auto& uses = built.window->root().children();
	REQUIRE(uses.size() == 2);
	for(std::size_t use = 0; use < uses.size(); ++use) {
		CAPTURE(use);
		REQUIRE(uses[use]->children().size() == 2);
		for(const auto& group : uses[use]->children()) // the second use's hold nothing, not the slot itself
			CHECK(group->children().size() == (use == 0 ? 1u : 0u));
	}
}
