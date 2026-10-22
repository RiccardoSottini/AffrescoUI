/// @file
/// @brief What the window hands each platform's accessibility API (render's Accessibility, format's
/// AccessibilityProperties.cpp): ids that last, values and ranges, a text field's selection, the states "expanded",
/// "selected", "disabled" and "live", the actions each element allows, and doing them. The platform's own API is read
/// back in tests/unit/window/accessibility_*.cpp.

#include "Headless.hpp"

#include <affresco/affresco.hpp>

#include <doctest/doctest.h>

#include <string>

using affresco::json;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief A screen with a slider, a checkbox, a field, a tree item that expands, a tab, a toast and a disabled button.
constexpr const char* statesScreen = R"json({
	"format": "affrescoui-screen", "version": 1,
	"state": { "volume": 4, "dark": false, "name": "Ada", "open": false, "saved": 0 },
	"root": { "id": "root", "children": [{ "id": "form", "type": "column", "fill": true, "children": [
		{ "id": "volume", "type": "slider", "bind": "state.volume", "min": 0, "max": 10, "step": 1,
		  "label": "Volume", "width": 200, "height": 20 },
		{ "id": "dark", "type": "toggle", "bind": "state.dark", "label": "Dark mode" },
		{ "id": "name", "type": "textarea", "bind": "state.name", "label": "Name", "width": 100, "height": 24 },
		{ "id": "folder", "role": "treeitem", "label": "src", "isOpen": "{{state.open}}", "width": 100,
		  "height": 20, "onClick": [{ "set": "state.open", "to": "{{!state.open}}" }] },
		{ "id": "tab", "role": "tab", "label": "main.cpp", "selected": true, "width": 80, "height": 20 },
		{ "id": "toast", "type": "label", "text": "Saved", "live": "polite" },
		{ "id": "save", "role": "button", "label": "Save", "disabled": true, "width": 80, "height": 30,
		  "onClick": [{ "increment": "state.saved" }] }
	] }] }
})json";

/// @brief A screen built in a headless window.
struct Built {
	ManualClock clock;                        ///< The window's time.
	std::unique_ptr<affresco::Window> window; ///< The window.
	affresco::Screen screen;                  ///< The screen.

	/// @brief Builds a screen.
	/// @param text The screen.
	explicit Built(const char* text)
	    : window(createHeadlessWindow(400, 300, clock)), screen(affresco::Screen::fromString(text)) {
		screen.build(*window);
		window->render();
	}

	/// @brief The live state.
	/// @return It.
	[[nodiscard]] json state() const { return json::parse(screen.stateJson()); }

	/// @brief The element of a node, read from the window's tree.
	/// @param id The node's id.
	/// @return Its element; one without a panel if it isn't in the tree.
	[[nodiscard]] affresco::AccessibleNode element(const std::string& id) {
		const affresco::Panel* panel = screen.find(id);
		const affresco::AccessibleNode tree = affresco::accessibilityTree(window->root());
		for(const affresco::AccessibleNode& child : tree.children)
			if(panel && child.panel == panel) return child;
		return {};
	}
};

/// @brief Whether an element allows an action.
/// @param node The element.
/// @param action The action.
/// @return Whether its flags have it.
bool allows(const affresco::AccessibleNode& node, affresco::AccessibleAction action) {
	return (node.actions & static_cast<std::uint8_t>(action)) != 0;
}

} // namespace

TEST_CASE("accessibility: ids last while their panel lives, and find it again") {
	Built built(statesScreen);
	const affresco::AccessibleNode volume = built.element("volume");
	REQUIRE(volume.panel);
	CHECK(volume.id > 1u);       // 1 is the window's element
	CHECK(volume.id % 2u == 0u); // odd ids are a text field's text run
	CHECK(volume.id != built.element("dark").id);
	built.window->render();
	CHECK(built.element("volume").id == volume.id);
	CHECK(affresco::findByAccessibleId(built.window->root(), volume.id) == volume.panel);
	CHECK(affresco::findByAccessibleId(built.window->root(), volume.id + 2u) == nullptr);
}

TEST_CASE("accessibility: values, ranges, a field's selection and bounds") {
	Built built(statesScreen);
	const affresco::AccessibleNode volume = built.element("volume");
	REQUIRE(volume.range);
	CHECK(volume.range->value == doctest::Approx(4.0));
	CHECK(volume.range->minimum == doctest::Approx(0.0));
	CHECK(volume.range->maximum == doctest::Approx(10.0));
	CHECK(volume.range->step == doctest::Approx(1.0));
	CHECK(volume.bounds.right - volume.bounds.left == doctest::Approx(200.0F));
	CHECK(built.element("dark").checked == false);
	const affresco::AccessibleNode name = built.element("name");
	CHECK(name.role == "textbox");
	CHECK(name.value == "Ada");
	REQUIRE(name.selection);
	CHECK(name.selection->first <= name.selection->second);
	CHECK(name.selection->second <= 3u);
}

TEST_CASE("accessibility: isOpen, selected, disabled and live come from the screen") {
	Built built(statesScreen);
	const affresco::AccessibleNode folder = built.element("folder");
	CHECK(folder.state.expanded == false);
	CHECK(allows(folder, affresco::AccessibleAction::Expand));
	CHECK_FALSE(allows(folder, affresco::AccessibleAction::Collapse));
	CHECK(built.element("tab").state.selected == true);
	CHECK_FALSE(built.element("volume").state.selected.has_value());
	CHECK(built.element("toast").state.live == affresco::LiveRegion::Polite);
	const affresco::AccessibleNode save = built.element("save");
	CHECK(save.state.disabled);
	CHECK(save.actions == 0); // a disabled button can't be pressed

	CHECK_THROWS(affresco::Screen::fromString(R"json({"format": "affrescoui-screen", "version": 1,
		"root": { "id": "root", "live": "loud" }})json")
	                 .build(*built.window));
}

TEST_CASE("accessibility: actions run as a click or a step would") {
	Built built(statesScreen);
	affresco::Window& window = *built.window;
	CHECK(
	    affresco::performAccessibleAction(window, *built.screen.find("volume"), affresco::AccessibleAction::Increment));
	window.render();
	CHECK(built.state()["volume"] == 5);
	CHECK(affresco::performAccessibleAction(window, *built.screen.find("dark"), affresco::AccessibleAction::Press));
	window.render();
	CHECK(built.state()["dark"] == true);
	CHECK(affresco::performAccessibleAction(window, *built.screen.find("folder"), affresco::AccessibleAction::Expand));
	window.render();
	CHECK(built.state()["open"] == true);
	const affresco::AccessibleNode folder = built.element("folder");
	CHECK(folder.state.expanded == true);
	CHECK(allows(folder, affresco::AccessibleAction::Collapse));
	CHECK_FALSE(
	    affresco::performAccessibleAction(window, *built.screen.find("save"), affresco::AccessibleAction::Press));
	CHECK(built.state()["saved"] == 0);
	CHECK(affresco::performAccessibleAction(window, *built.screen.find("name"), affresco::AccessibleAction::Focus));
	CHECK(window.focusedPanel() == built.screen.find("name"));
	CHECK(built.element("name").focused);
}
