/// @file
/// @brief Tests that need a window: pictures of a screen for peers ("render"), of main and of a proposal, drawn
/// without changing what the person sees.
///
/// Built into affrescoui_window_tests: drawing needs an OpenGL context, so a display or Xvfb. The windows are never
/// shown.

#include "Helpers.hpp"

#include <affresco/affresco.hpp>

#include <GLFW/glfw3.h>
#include <doctest/doctest.h>

#include <algorithm>
#include <string>

using affresco::json;
using affresco::test::MemoryChannel;

namespace {

/// @brief A screen with a white board the agent may only propose changes to, and a studio that reviews.
constexpr const char* boardScreen = R"json({
	"format": "affrescoui-screen",
	"version": 1,
	"window": { "title": "pictures", "width": 120, "height": 80 },
	"state": { "count": 0 },
	"permissions": { "allow": ["studio"], "ask": ["agent(#board)"], "reviewers": ["studio"] },
	"root": { "type": "panel", "id": "main", "color": "#202020", "children": [
		{ "type": "panel", "id": "board", "color": "#ffffff", "width": "50%", "height": "50%" }
	] }
})json";

/// @brief Sends a message from a peer and returns the answer carrying its id.
/// @param channel The peer's channel.
/// @param message The message, with an "id".
/// @return The answer.
json ask(MemoryChannel& channel, const json& message) {
	channel.sent.clear();
	channel.receive(message.dump());
	const auto answer =
	    std::ranges::find(channel.sent, message["id"], [](const json& m) { return m.value("id", json()); });
	REQUIRE(answer != channel.sent.end());
	return *answer;
}

/// @brief Whether any message of a type was sent.
/// @param channel The channel.
/// @param type The type.
bool sentAny(const MemoryChannel& channel, const std::string& type) {
	return std::ranges::any_of(channel.sent, [&](const json& m) { return m.value("type", "") == type; });
}

} // namespace

TEST_CASE("screen: a peer gets pictures of main and of a proposal, and the person's view stays as it was") {
	affresco::Application app;
	MemoryChannel agent, studio; // before the screen: a peer's channel outlives it
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	affresco::Screen screen = affresco::Screen::fromString(boardScreen);
	affresco::Window& window = screen.createWindow(app);
	window.render();

	screen.connect(agent, "agent");
	screen.connect(studio, "studio");
	const json proposed = ask(agent, {{"type", "revision"}, {"id", 1}, {"set", {{"#board/color", "#ff0000"}}}});
	REQUIRE(proposed["type"] == "proposed");
	const int proposal = proposed["proposal"];
	const std::string document = screen.documentJson();

	const json main = ask(agent, {{"type", "render"}, {"id", 2}});
	REQUIRE(main["type"] == "image");
	CHECK(main["width"] == 120);
	CHECK(main["height"] == 80);
	CHECK(main["data"].get<std::string>().starts_with("iVBORw0KGgo")); // the PNG signature, in base64

	studio.sent.clear();
	const json withProposal = ask(agent, {{"type", "render"}, {"id", 3}, {"proposal", proposal}, {"scale", 2}});
	REQUIRE(withProposal["type"] == "image");
	CHECK(withProposal["width"] == 240);
	CHECK(withProposal["proposal"] == proposal);
	CHECK_FALSE(sentAny(studio, "review")); // the reviewer saw nothing happen
	CHECK_FALSE(screen.previewedProposal());
	CHECK(screen.documentJson() == document);

	// The same proposal at scale 1 differs from main: the board is red in it
	const json small = ask(agent, {{"type", "render"}, {"id", 4}, {"proposal", proposal}});
	CHECK(small["data"] != main["data"]);
	CHECK(ask(agent, {{"type", "render"}, {"id", 5}})["data"] == main["data"]); // main is drawn the same again

	// While the person previews the proposal, a picture of main leaves their preview in place
	REQUIRE(screen.previewProposal(proposal));
	studio.sent.clear();
	CHECK(ask(agent, {{"type", "render"}, {"id", 6}})["data"] == main["data"]);
	CHECK(ask(agent, {{"type", "render"}, {"id", 7}, {"proposal", proposal}})["data"] == small["data"]);
	CHECK(screen.previewedProposal() == proposal);
	CHECK_FALSE(sentAny(studio, "review"));

	const json missing = ask(agent, {{"type", "render"}, {"id", 8}, {"proposal", proposal + 1}});
	CHECK(missing["type"] == "rejected");
	CHECK(screen.previewedProposal() == proposal);
}

TEST_CASE("screen: a picture of a missing proposal leaves the person's preview as it was") {
	affresco::Application app;
	MemoryChannel agent; // before the screen: a peer's channel outlives it
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	affresco::Screen screen = affresco::Screen::fromString(boardScreen);
	affresco::Window& window = screen.createWindow(app);
	window.render();
	screen.connect(agent, "agent");

	// Showing the proposal counts once, in the preview's own state
	const json counter = {
	    {"type", "panel"}, {"id", "counter"}, {"onShow", {{{"set", "state.count"}, {"to", "{{state.count + 1}}"}}}}};
	const json proposed =
	    ask(agent, {{"type", "revision"}, {"id", 1}, {"set", {{"#board/children", json::array({counter})}}}});
	REQUIRE(proposed["type"] == "proposed");
	const int proposal = proposed["proposal"];
	REQUIRE(screen.previewProposal(proposal));
	window.render();
	const std::string previewed = screen.stateJson();
	REQUIRE(json::parse(previewed)["count"] == 1);

	CHECK(ask(agent, {{"type", "render"}, {"id", 2}, {"proposal", proposal + 1}})["type"] == "rejected");
	CHECK(screen.previewedProposal() == proposal);
	CHECK(screen.stateJson() == previewed); // not previewed again from main's state
}

// Known bug, found in review: putting the preview back defers the proposal's onHide steps (and main's onShow and watch
// steps) to the next frame, when the picture is done, so they run on main's live state without review. Discarding the
// tasks deferred while drawing needs a Window API (the render module). Unskip when it exists.
TEST_CASE("screen: a picture of a proposal runs none of its steps on main" * doctest::skip(true)) {
	affresco::Application app;
	MemoryChannel agent; // before the screen: a peer's channel outlives it
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	affresco::Screen screen = affresco::Screen::fromString(boardScreen);
	affresco::Window& window = screen.createWindow(app);
	window.render();
	screen.connect(agent, "agent");

	const json spy = {{"type", "panel"}, {"id", "spy"}, {"onHide", {{{"set", "state.count"}, {"to", 99}}}}};
	const json proposed =
	    ask(agent, {{"type", "revision"}, {"id", 1}, {"set", {{"#board/children", json::array({spy})}}}});
	REQUIRE(proposed["type"] == "proposed");
	REQUIRE(ask(agent, {{"type", "render"}, {"id", 2}, {"proposal", proposed["proposal"]}})["type"] == "image");
	window.render(); // runs what was deferred
	CHECK(json::parse(screen.stateJson())["count"] == 0);
}

// Known bug, found in review: steps main deferred just before a picture of a proposal (here a revision's onShow) run
// inside the proposal's preview, so their changes to the state are put back with it and lost. Running the pending tasks
// before switching needs a Window API (the render module). Unskip when it exists.
TEST_CASE("screen: steps main deferred before a picture of a proposal still change main" * doctest::skip(true)) {
	affresco::Application app;
	MemoryChannel agent, studio; // before the screen: a peer's channel outlives it
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	affresco::Screen screen = affresco::Screen::fromString(boardScreen);
	affresco::Window& window = screen.createWindow(app);
	window.render();
	screen.connect(agent, "agent");
	screen.connect(studio, "studio");

	const json proposed = ask(agent, {{"type", "revision"}, {"id", 1}, {"set", {{"#board/color", "#00ff00"}}}});
	REQUIRE(proposed["type"] == "proposed");
	const json shown = {{"type", "panel"}, {"id", "shown"}, {"onShow", {{{"set", "state.count"}, {"to", 5}}}}};
	REQUIRE(ask(studio, {{"type", "revision"}, {"id", 2}, {"set", {{"#main/children/-", shown}}}})["type"] ==
	        "applied");
	REQUIRE(ask(agent, {{"type", "render"}, {"id", 3}, {"proposal", proposed["proposal"]}})["type"] == "image");
	window.render();
	CHECK(json::parse(screen.stateJson())["count"] == 5);
}
