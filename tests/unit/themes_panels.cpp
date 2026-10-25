/// @file
/// @brief The desktop kit's panels (modules/themes/components/panels.json): an inspector's rows editing an object in
/// the state, numbers scrubbed by dragging their name, typed and stepped within their range, choices picked from a
/// list, vectors edited one axis at a time, collapsible groups and disabled rows, each change running its action.

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

/// @brief Drags the pointer sideways from the middle of a panel with the left button, in a few moves.
/// @param window The window.
/// @param panel Where the button goes down.
/// @param dx How far it goes.
void dragFrom(affresco::Window& window, const affresco::Panel& panel, float dx) {
	const affresco::Rect& frame = panel.frame();
	const affresco::Vec2 from{(frame.left + frame.right) / 2.0f, (frame.top + frame.bottom) / 2.0f};
	window.routeMouseButton({affresco::MouseButton::Left, affresco::Action::Press, {}, from});
	for(int step = 1; step <= 4; ++step) window.routeMouseMove({from.x + dx * static_cast<float>(step) / 4.0f, from.y});
	window.routeMouseButton({affresco::MouseButton::Left, affresco::Action::Release, {}, {from.x + dx, from.y}});
}

/// @brief The first shown panel under another, depth first, with a role and an accessible name.
/// @param panel Where to look.
/// @param role The role.
/// @param label The name.
/// @return It, or nullptr.
const affresco::Panel* findAccessible(const affresco::Panel& panel, std::string_view role, std::string_view label) {
	if(!panel.isShown()) return nullptr;
	if(panel.accessibleRole() == role && panel.accessibleLabel() == label) return &panel;
	for(const auto& child : panel.children())
		if(const affresco::Panel* found = findAccessible(*child, role, label)) return found;
	return nullptr;
}

/// @brief The first shown label under a panel showing a text.
/// @param panel Where to look.
/// @param text The text.
/// @return It, or nullptr.
const affresco::Panel* findText(const affresco::Panel& panel, std::string_view text) {
	if(!panel.isShown()) return nullptr;
	if(const auto* label = dynamic_cast<const affresco::Label*>(&panel); label && label->text() == text) return &panel;
	for(const auto& child : panel.children())
		if(const affresco::Panel* found = findText(*child, text)) return found;
	return nullptr;
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

/// @brief An inspector of state.shape in a 400 by 600 window, its changes counted in state.changes and the last one
/// kept in state.last: a size in a group, a vector, a choice and a switch in another, and a disabled number.
/// @return The document.
std::string inspectorScreen() {
	return R"json({
		"format": "affrescoui-screen", "version": 2,
		"state": { "changes": 0, "last": null,
		           "shape": { "width": 100, "angle": 0.2, "position": [1, 2, 3], "blend": "normal", "on": false, "z": 5 } },
		"actions": {
			"changed": [{ "increment": "state.changes", "by": 1 }, { "set": "state.last", "to": "{{ {key: key, value: value} }}" }]
		},
		"root": { "id": "root", "children": [{ "use": "inspector", "fill": true, "with": {
			"bind": "state.shape", "values": "{{state.shape}}", "width": 400, "height": 600, "onChange": "changed",
			"groups": [
				{ "id": "size", "title": "Size", "rows": [
					{ "key": "width", "label": "Width", "unit": "px", "min": 10, "max": 200 },
					{ "key": "angle", "label": "Angle", "step": 0.1, "max": 1 }
				] },
				{ "id": "more", "title": "More", "rows": [
					{ "key": "position", "label": "Position", "type": "vector" },
					{ "key": "blend", "label": "Blend", "type": "select", "options": ["normal", { "id": "add", "label": "Add" }] },
					{ "key": "on", "label": "On", "type": "toggle" },
					{ "key": "z", "label": "Depth", "enabled": false }
				] }
			] } }] }
	})json";
}

/// @brief An inspector built in a headless 400 by 600 window and laid out.
struct Inspected {
	ManualClock clock;                                                                ///< The window's clock.
	std::unique_ptr<affresco::Window> window = createHeadlessWindow(400, 600, clock); ///< The window.
	affresco::Screen screen{affresco::Screen::fromString(inspectorScreen())};         ///< The screen.

	/// @brief Builds it.
	Inspected() {
		screen.build(*window);
		settle(*window, clock);
	}

	/// @brief A shown node by role and name, required.
	/// @param role The role; empty for a text field, whose role is implied.
	/// @param label The name.
	/// @return It.
	const affresco::Panel& node(std::string_view role, std::string_view label) const {
		const affresco::Panel* found = findAccessible(window->root(), role, label);
		REQUIRE(found);
		return *found;
	}

	/// @brief Focuses a text field by clicking it, takes its text away and types another, then Enter.
	/// @param label The field's name.
	/// @param typed The text.
	void type(std::string_view label, std::string_view typed) {
		clickOn(*window, node("", label));
		for(int i = 0; i < 12; ++i) press(*window, affresco::Key::Backspace);
		for(const char c : typed) window->routeChar({static_cast<char32_t>(c)});
		press(*window, affresco::Key::Enter);
		settle(*window, clock);
	}

	/// @brief A value of the inspected object.
	/// @param key Its key.
	/// @return It.
	json value(std::string_view key) const { return state(screen)["shape"][std::string(key)]; }
};

} // namespace

TEST_CASE("format: an inspector shows a control for each row in its groups, and the checker accepts it") {
	Inspected inspector;
	CHECK(inspector.node("form", "Properties").isShown());
	CHECK(inspector.node("button", "Size").isShown());
	CHECK(inspector.node("", "Width").isShown());
	CHECK(inspector.node("", "Position x").isShown());
	CHECK(inspector.node("", "Position z").isShown());
	CHECK(inspector.node("combobox", "Blend").isShown());
	CHECK(findText(inspector.window->root(), "100"));
	CHECK(findText(inspector.window->root(), "0.2")); // a step of 0.1 shows one decimal
	CHECK(findText(inspector.window->root(), "px"));

	const json document = json::parse(inspectorScreen());
	const auto findings = affresco::format::check(document, document["state"]);
	REQUIRE(findings);
	for(const auto& finding : *findings) CAPTURE(finding.text());
	CHECK(findings->empty());
}

TEST_CASE(
    "format: an inspector's number changes by dragging its name, within its range, its action told each new value") {
	Inspected inspector;
	const affresco::Panel* name = findText(inspector.window->root(), "Width");
	REQUIRE(name);
	dragFrom(*inspector.window, *name, 40.0f);
	settle(*inspector.window, inspector.clock);
	CHECK(inspector.value("width") == 120);
	CHECK(state(inspector.screen)["changes"] == 4); // 105, 110, 115 and 120
	CHECK(state(inspector.screen)["last"] == json{{"key", "width"}, {"value", 120}});

	dragFrom(*inspector.window, *findText(inspector.window->root(), "Width"), 300.0f);
	settle(*inspector.window, inspector.clock);
	CHECK(inspector.value("width") == 200);         // its maximum
	CHECK(state(inspector.screen)["changes"] == 7); // 158, 195 and 200, not 200 again

	// A disabled row's name doesn't scrub
	dragFrom(*inspector.window, *findText(inspector.window->root(), "Depth"), 40.0f);
	settle(*inspector.window, inspector.clock);
	CHECK(inspector.value("z") == 5);
	CHECK(state(inspector.screen)["changes"] == 7);
}

TEST_CASE("format: an inspector's typed number is kept within its range, and a text that isn't one changes nothing") {
	Inspected inspector;
	inspector.type("Width", "150");
	CHECK(inspector.value("width") == 150);
	CHECK(state(inspector.screen)["last"] == json{{"key", "width"}, {"value", 150}});
	inspector.type("Width", "999");
	CHECK(inspector.value("width") == 200);
	inspector.type("Width", "wide");
	CHECK(inspector.value("width") == 200);
	CHECK(state(inspector.screen)["changes"] == 2);

	// The arrows step it, rounded to the step's decimals
	clickOn(*inspector.window, inspector.node("", "Angle"));
	press(*inspector.window, affresco::Key::Up);
	settle(*inspector.window, inspector.clock);
	CHECK(inspector.value("angle") == doctest::Approx(0.3));
	CHECK(inspector.value("angle").dump() == "0.3");
	for(int i = 0; i < 10; ++i) press(*inspector.window, affresco::Key::Up);
	settle(*inspector.window, inspector.clock);
	CHECK(inspector.value("angle") == 1); // its maximum
}

TEST_CASE("format: an inspector's vector changes one axis at a time, its action told the whole vector") {
	Inspected inspector;
	inspector.type("Position y", "7.5");
	CHECK(inspector.value("position") == json{1, 7.5, 3});
	CHECK(state(inspector.screen)["last"] == json{{"key", "position"}, {"value", {1, 7.5, 3}}});

	clickOn(*inspector.window, inspector.node("", "Position z"));
	press(*inspector.window, affresco::Key::Down);
	settle(*inspector.window, inspector.clock);
	CHECK(inspector.value("position") == json{1, 7.5, 2});
	CHECK(state(inspector.screen)["changes"] == 2);
}

TEST_CASE("format: an inspector's choice opens a list of its options, and its groups collapse") {
	Inspected inspector;
	clickOn(*inspector.window, inspector.node("combobox", "Blend"));
	settle(*inspector.window, inspector.clock);
	CHECK(inspector.node("combobox", "Blend").accessibleState().expanded.value_or(false));
	clickOn(*inspector.window, inspector.node("option", "Add"));
	settle(*inspector.window, inspector.clock);
	CHECK(inspector.value("blend") == "add");
	CHECK(state(inspector.screen)["last"] == json{{"key", "blend"}, {"value", "add"}});
	CHECK_FALSE(findAccessible(inspector.window->root(), "option", "Add"));
	CHECK(findText(inspector.window->root(), "Add"));

	// A group's header hides its rows and shows them again
	CHECK(inspector.node("button", "Size").accessibleState().expanded.value_or(false));
	clickOn(*inspector.window, inspector.node("button", "Size"));
	settle(*inspector.window, inspector.clock);
	CHECK_FALSE(inspector.node("button", "Size").accessibleState().expanded.value_or(false));
	CHECK_FALSE(findAccessible(inspector.window->root(), "", "Width"));
	CHECK(state(inspector.screen)["inspector"]["closed"]["size"] == true);
	clickOn(*inspector.window, inspector.node("button", "Size"));
	settle(*inspector.window, inspector.clock);
	CHECK(findAccessible(inspector.window->root(), "", "Width"));
}

namespace {

/// @brief A ribbon of a given width in a 720 by 300 window, its commands kept in state.last: a tab with a large
/// button and small ones in a group, a group of switches and two large ones, and another tab.
/// @param width The ribbon's width.
/// @return The document.
std::string ribbonScreen(int width) {
	return R"json({
		"format": "affrescoui-screen", "version": 2,
		"state": { "last": "", "bold": true },
		"actions": { "command": { "set": "state.last", "to": "{{what}}" } },
		"root": { "id": "root", "children": [{ "use": "ribbon", "with": { "width": )json" +
	       std::to_string(width) + R"json(, "tabs": [
			{ "id": "home", "title": "Home", "groups": [
				{ "id": "clipboard", "title": "Clipboard", "items": [
					{ "title": "Paste", "size": "large", "run": "command", "with": { "what": "paste" } },
					{ "title": "Cut", "run": "command", "with": { "what": "cut" } },
					{ "title": "Copy", "run": "command", "with": { "what": "copy" } }
				] },
				{ "id": "font", "title": "Font", "items": [
					{ "title": "Bold", "checked": "{{state.bold}}" },
					{ "title": "Underline", "enabled": false, "run": "command", "with": { "what": "underline" } }
				] },
				{ "id": "editing", "title": "Editing", "items": [
					{ "title": "Find", "size": "large", "run": "command", "with": { "what": "find" } },
					{ "title": "Replace", "size": "large", "run": "command", "with": { "what": "replace" } }
				] }
			] },
			{ "id": "insert", "title": "Insert", "groups": [
				{ "id": "media", "title": "Media", "items": [{ "title": "Picture", "size": "large" }] }
			] }
		] } }] }
	})json";
}

/// @brief A ribbon built in a headless 720 by 300 window and laid out.
struct Ribboned {
	ManualClock clock;                                                                ///< The window's clock.
	std::unique_ptr<affresco::Window> window = createHeadlessWindow(720, 300, clock); ///< The window.
	affresco::Screen screen;                                                          ///< The screen.

	/// @brief Builds it.
	/// @param width The ribbon's width.
	explicit Ribboned(int width = 720) : screen(affresco::Screen::fromString(ribbonScreen(width))) {
		screen.build(*window);
		settle(*window, clock);
	}

	/// @brief Whether a shown node has a role and a name.
	/// @param role The role.
	/// @param label The name.
	/// @return true if one does.
	bool shows(std::string_view role, std::string_view label) const {
		return findAccessible(window->root(), role, label) != nullptr;
	}

	/// @brief Clicks a shown node by role and name, required.
	/// @param role The role.
	/// @param label The name.
	void click(std::string_view role, std::string_view label) {
		const affresco::Panel* found = findAccessible(window->root(), role, label);
		REQUIRE(found);
		clickOn(*window, *found);
		settle(*window, clock);
	}
};

} // namespace

TEST_CASE("format: a ribbon shows its tab's groups of buttons, which run their actions, and the checker accepts it") {
	Ribboned ribbon;
	CHECK(ribbon.shows("tablist", "Ribbon"));
	CHECK(ribbon.shows("toolbar", "Home"));
	CHECK(ribbon.shows("group", "Clipboard"));
	CHECK(ribbon.shows("group", "Editing"));
	const affresco::Panel* bold = findAccessible(ribbon.window->root(), "button", "Bold");
	REQUIRE(bold);
	CHECK(bold->accessibleState().selected.value_or(false));
	ribbon.click("button", "Paste");
	CHECK(state(ribbon.screen)["last"] == "paste");
	ribbon.click("button", "Copy");
	CHECK(state(ribbon.screen)["last"] == "copy");
	ribbon.click("button", "Underline"); // disabled
	CHECK(state(ribbon.screen)["last"] == "copy");

	const json document = json::parse(ribbonScreen(720));
	const auto findings = affresco::format::check(document, document["state"]);
	REQUIRE(findings);
	for(const auto& finding : *findings) CAPTURE(finding.text());
	CHECK(findings->empty());
}

TEST_CASE("format: a ribbon's tabs switch its groups, and it collapses to its tabs") {
	Ribboned ribbon;
	ribbon.click("tab", "Insert");
	CHECK(ribbon.shows("group", "Media"));
	CHECK_FALSE(ribbon.shows("group", "Clipboard"));
	CHECK(state(ribbon.screen)["ribbon"]["tab"] == "insert");

	ribbon.click("button", "Collapse the ribbon");
	CHECK_FALSE(ribbon.shows("toolbar", "Insert"));
	ribbon.click("tab", "Home"); // a tab shows the groups again
	CHECK(ribbon.shows("group", "Clipboard"));
	CHECK(state(ribbon.screen)["ribbon"]["collapsed"] == false);
}

TEST_CASE("format: a narrow ribbon folds its last groups into buttons whose menus run their items") {
	Ribboned ribbon(300);
	CHECK(ribbon.shows("group", "Clipboard"));
	CHECK_FALSE(ribbon.shows("group", "Editing"));
	CHECK(ribbon.shows("button", "Editing"));
	CHECK_FALSE(ribbon.shows("button", "Find"));
	ribbon.click("button", "Editing");
	ribbon.click("menuitem", "Replace");
	CHECK(state(ribbon.screen)["last"] == "replace");

	Ribboned wide(720);
	CHECK_FALSE(wide.shows("button", "Editing"));
	CHECK(wide.shows("button", "Find"));
}
