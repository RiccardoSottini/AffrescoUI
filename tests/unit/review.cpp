/// @file
/// @brief Tests that need a window: review steps in a screen, and control steps while a proposal is previewed.
///
/// Built into affrescoui_window_tests (see timers.cpp): the steps run on a window, the windows are never shown.

#include <affresco/affresco.hpp>

#include <GLFW/glfw3.h>
#include <doctest/doctest.h>

#include <chrono>
#include <string>
#include <thread>

namespace {

/// @brief Waits long enough for the "every" timers in these tests (16 ms) to be due.
void waitForTimers() { std::this_thread::sleep_for(std::chrono::milliseconds(40)); }

/// @brief A screen where the agent's changes wait for review, with a native "save", review actions, a button that
/// saves, and a timer that saves.
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
		{ "type": "panel", "id": "board", "every": { "ms": 16, "do": "save" } }
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

/// @brief Clicks a window at a point, as the user would.
/// @param window The window.
/// @param x Horizontal position.
/// @param y Vertical position.
void click(affresco::Window& window, float x, float y) {
	for(const affresco::Action action : {affresco::Action::Press, affresco::Action::Release})
		window.routeMouseButton({affresco::MouseButton::Left, action, {}, {x, y}});
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

// Before, a previewed proposal's document ran its control steps from timers: with a timer it added, a proposal
// accepted itself when the reviewer only previewed it
TEST_CASE("format: while a proposal is previewed, control steps run only from the user's input") {
	affresco::Application app;
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	affresco::Screen screen = affresco::Screen::fromString(reviewScreen);
	int saves = 0;
	screen.onCall("save", [&](const std::string&) {
		++saves;
		return std::string();
	});
	affresco::Window& window = screen.createWindow(app);
	window.render();
	waitForTimers();
	window.render();
	CHECK(saves > 0); // the screen's own timer saves

	REQUIRE(screen.previewProposal(proposeAsAgent(screen)));
	saves = 0;
	for(int frame = 0; frame < 3; ++frame) {
		waitForTimers();
		window.render();
	}
	CHECK(saves == 0); // the timer's "call" is refused while the proposal is shown

	click(window, 10.0f, 10.0f);
	window.render();
	CHECK(saves == 1); // the user's own click still saves

	screen.endPreview();
	waitForTimers();
	window.render();
	CHECK(saves > 1);
}
