/// @file
/// @brief Tests that need a window: review steps in a screen.
///
/// Built into affrescoui_window_tests (see timers.cpp): the steps run on a window, the windows are never shown.

#include <affresco/affresco.hpp>

#include <GLFW/glfw3.h>
#include <doctest/doctest.h>

#include <string>

namespace {

/// @brief A screen where the agent's changes wait for review, with a native "save" and review actions.
constexpr const char* reviewScreen = R"json({
	"format": "affrescoui-screen",
	"version": 1,
	"permissions": { "allow": ["studio"], "ask": ["agent"], "reviewers": ["studio"] },
	"actions": {
		"save": [{ "call": "save" }],
		"approve": [{ "accept": "{{proposals[0].id}}", "head": "{{proposals[0].head}}" }],
		"approveUnseen": [{ "accept": "{{proposals[0].id}}" }]
	},
	"root": { "type": "panel", "children": [
		{ "type": "panel", "id": "saveButton", "x": 0, "y": 0, "width": 100, "height": 50, "onClick": "save" },
		{ "type": "panel", "id": "board" }
	] }
})json";

/// @brief Proposes a change as the agent.
/// @param screen The screen.
/// @return The proposal's id.
int proposeAsAgent(affresco::Screen& screen) {
	const affresco::RevisionResult result =
	    screen.submitRevision(R"({"author": "agent", "message": "Recolour", "set": {"#board/color": "#ff0000"}})");
	REQUIRE(result.status == affresco::RevisionStatus::Proposed);
	return result.proposal;
}

} // namespace

// Before, an "accept" step approved whatever the proposal had become: it named no head
TEST_CASE("format: an accept step names the head the user reviewed") {
	affresco::Application app;
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	affresco::Screen screen = affresco::Screen::fromString(reviewScreen);
	screen.onCall("save", [](const std::string&) { return std::string(); });
	affresco::Window& window = screen.createWindow(app);
	window.render();
	proposeAsAgent(screen);

	screen.runAction("approveUnseen", window.root(), "{}");
	window.render();
	CHECK(screen.proposals().size() == 1);
	CHECK(screen.revision() == 0);

	screen.runAction("approve", window.root(), "{}");
	window.render();
	CHECK(screen.proposals().empty());
	CHECK(screen.revision() == 1);
}
