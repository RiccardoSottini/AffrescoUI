/// @file
/// @brief The format's runtime on a headless window: a shipped template built, laid out and clicked, with no display.

#include "Headless.hpp"

#include <affresco/affresco.hpp>

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

using affresco::json;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief Loads a shipped template, as affrescoui_host does but without its data folder (no history or state file).
/// @param name The template, e.g. "kanban".
/// @return The screen.
affresco::Screen loadTemplate(const std::string& name) {
	const std::filesystem::path directory = AFFRESCOUI_SCREENS_DIRECTORY;
	const std::ifstream file(directory / (name + ".screen.json"));
	REQUIRE(file);
	std::stringstream text;
	text << file.rdbuf();
	affresco::Screen screen = affresco::Screen::fromString(text.str(), directory);
	screen.setPlaceholderTypes(true); // as the host does: its own types aren't registered here
	return screen;
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

} // namespace

TEST_CASE("format: the Kanban template builds on a headless window, and a click opens its members menu") {
	ManualClock clock;
	const auto window = createHeadlessWindow(1280, 800, clock);
	affresco::Screen screen = loadTemplate("kanban");
	screen.build(*window);
	window->render();

	affresco::Panel* members = screen.find("members");
	affresco::Panel* menu = screen.find("memberMenu");
	REQUIRE(members);
	REQUIRE(menu);
	CHECK(members->isShown());
	CHECK(members->frame().width() == 136.0f);
	CHECK_FALSE(menu->isShown());

	clickOn(*window, *members);
	window->render();
	CHECK(json::parse(screen.stateJson())["membersOpen"] == true);
	CHECK(menu->isShown());

	// A click outside the open menu closes it ("dismiss": "outside")
	window->routeMouseButton({affresco::MouseButton::Left, affresco::Action::Press, {}, {5.0f, 700.0f}});
	window->routeMouseButton({affresco::MouseButton::Left, affresco::Action::Release, {}, {5.0f, 700.0f}});
	window->render();
	CHECK(json::parse(screen.stateJson())["membersOpen"] == false);
	CHECK_FALSE(menu->isShown());
}

// The code editor is left out: its "code" type is the host's (examples/CodeEditor.cpp)
TEST_CASE("format: the shipped templates build on a headless window") {
	for(const std::string name : {"kanban", "smart-home", "music-player", "habit-tracker", "sign-in", "studio"}) {
		CAPTURE(name);
		ManualClock clock;
		const auto window = createHeadlessWindow(1280, 800, clock);
		affresco::Screen screen = loadTemplate(name);
		screen.build(*window);
		window->render();
		clock.advance(1000.0); // timers, animations and deferred steps run on the manual clock
		window->render();
		CHECK(window->root().children().size() > 0);
	}
}
