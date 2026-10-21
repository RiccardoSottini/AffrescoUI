/// @file
/// @brief Accessibility (render's Accessibility, src/Properties.cpp, src/Reconciler.cpp): roles, names and
/// descriptions, the tree and finding by role, the focus ring shown only after the keyboard moved the focus, and
/// reduced motion making transitions and animations instant.

#include "Headless.hpp"

#include <affresco/affresco.hpp>

#include <doctest/doctest.h>

#include <string>

using affresco::json;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief A screen with a button, a toggle, a field, a hidden button and a panel with no role.
constexpr const char* formScreen = R"json({
	"format": "affrescoui-screen", "version": 1,
	"window": { "focusRing": "#ff0000" },
	"state": { "dark": false, "draft": "", "saved": 0, "x": 0 },
	"actions": { "save": [{ "increment": "state.saved" }] },
	"root": { "id": "root", "type": "column", "children": [
		{ "id": "save", "focusable": true, "width": 80, "height": 30, "onClick": [{ "run": "save" }],
		  "description": "Saves the file",
		  "children": [{ "id": "save-text", "type": "label", "text": "Save" }] },
		{ "id": "dark", "type": "toggle", "bind": "state.dark", "label": "Dark mode" },
		{ "id": "field", "type": "textarea", "bind": "state.draft", "width": 100, "height": 24, "tooltip": "Search" },
		{ "id": "gone", "visible": false, "role": "button", "label": "Gone" },
		{ "id": "plain", "width": 10, "height": 10, "children": [
			{ "id": "list", "role": "list", "label": "Files", "width": 10, "height": 10 }
		] }
	] }
})json";

/// @brief A screen built in a headless window.
struct Built {
	ManualClock clock;                          ///< The window's time.
	std::unique_ptr<affresco::Window> window;   ///< The window.
	affresco::Screen screen;                    ///< The screen.

	/// @brief Builds a screen.
	/// @param text The screen.
	explicit Built(const char* text)
	    : window(createHeadlessWindow(400, 300, clock)), screen(affresco::Screen::fromString(text)) {
		screen.build(*window);
		window->render();
	}
};

/// @brief Presses and releases a key without modifiers.
/// @param window The window.
/// @param key The key.
void press(affresco::Window& window, affresco::Key key) {
	window.routeKey({key, 0, affresco::Action::Press, {}});
	window.routeKey({key, 0, affresco::Action::Release, {}});
	window.render();
}

} // namespace

TEST_CASE("accessibility: roles, names and the tree") {
	Built built(formScreen);
	affresco::Panel& root = built.window->root();

	affresco::Panel* save = built.screen.find("save");
	REQUIRE(save);
	CHECK(affresco::accessibleRole(*save) == "button"); // focusable, with an onClick
	CHECK(affresco::accessibleName(*save) == "Save");   // the labels inside it
	CHECK(save->accessibleDescription() == "Saves the file");
	CHECK(affresco::accessibleRole(*built.screen.find("dark")) == "checkbox");
	CHECK(affresco::accessibleName(*built.screen.find("dark")) == "Dark mode");
	// A field's text is its value, not its name
	CHECK(affresco::accessibleRole(*built.screen.find("field")) == "textbox");
	CHECK(affresco::accessibleName(*built.screen.find("field")) == "Search");
	CHECK(affresco::accessibleRole(*built.screen.find("plain")).empty());

	const affresco::AccessibleNode tree = affresco::accessibilityTree(root);
	CHECK(tree.panel == nullptr);
	REQUIRE(tree.children.size() == 4); // the hidden button and the panel with no role are left out
	CHECK(tree.children[0].role == "button");
	REQUIRE(tree.children[0].children.size() == 1);
	CHECK(tree.children[0].children[0].role == "text");
	CHECK(tree.children[3].role == "list"); // in the role-less panel's place
	CHECK(tree.children[3].name == "Files");

	CHECK(affresco::findByRole(root, "button", "Save").size() == 1);
	CHECK(affresco::findByRole(root, "button", "Gone").empty());
	CHECK(affresco::findByRole(root, "checkbox").size() == 1);
}

TEST_CASE("accessibility: the focus ring shows after Tab, not after a click") {
	Built built(formScreen);
	affresco::Window& window = *built.window;
	REQUIRE(window.focusRing());
	CHECK(window.focusRing()->color.toHex() == "#ff0000");

	press(window, affresco::Key::Tab);
	REQUIRE(window.focusedPanel() != nullptr);
	CHECK(window.focusVisible());
	CHECK(affresco::accessibilityTree(window.root()).children[0].focused ==
	      (window.focusedPanel() == built.screen.find("save")));

	const affresco::Rect bounds = built.screen.find("field")->bounds();
	const affresco::Vec2 at{(bounds.left + bounds.right) / 2.0f, (bounds.top + bounds.bottom) / 2.0f};
	window.routeMouseButton({affresco::MouseButton::Left, affresco::Action::Press, {}, at});
	window.routeMouseButton({affresco::MouseButton::Left, affresco::Action::Release, {}, at});
	window.render();
	CHECK(window.focusedPanel() == built.screen.find("field"));
	CHECK_FALSE(window.focusVisible());
}

TEST_CASE("accessibility: window.reducedMotion makes animations instant") {
	Built built(R"json({
		"format": "affrescoui-screen", "version": 1,
		"state": { "x": 0 },
		"actions": { "go": [{ "animate": "state.x", "to": 100, "ms": 300 }] },
		"root": { "id": "root", "children": [
			{ "id": "motion", "type": "label", "text": "{{window.reducedMotion ? 'reduced' : 'full'}}" }
		] }
	})json");
	const auto text = [&] { return dynamic_cast<affresco::Label*>(built.screen.find("motion"))->text(); };
	CHECK(text() == "full");

	built.window->routeDisplay({1.0f, affresco::ColorScheme::Light, true});
	built.window->render();
	CHECK(built.window->reducedMotion());
	CHECK(text() == "reduced");
	built.screen.runAction("go", built.window->root());
	CHECK(json::parse(built.screen.stateJson())["x"] == 100);
}
