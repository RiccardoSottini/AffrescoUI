/// @file
/// @brief The format's runtime on a headless window: a shipped template built, laid out and clicked, with no display.

#include "Headless.hpp"
#include "Templates.hpp"

#include <affresco/affresco.hpp>

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>

using affresco::json;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief Loads a shipped template, as affrescoui_launcher does but without its data folder (no history or state file):
/// an app (an app.json beside it) with its themes, models and seed records.
/// @param name The template, e.g. "kanban".
/// @return The screen.
affresco::Screen loadTemplate(const std::string& name) {
	const std::filesystem::path directory = templateDirectory(AFFRESCOUI_EXAMPLES_DIRECTORY, name);
	affresco::Screen screen = [&] {
		if(std::filesystem::exists(directory / "app.json"))
			return affresco::Screen::fromApp(directory / "app.json", {}, affresco::Screen::History::InMemory);
		const std::ifstream file(directory / "index.screen.json");
		REQUIRE(file);
		std::stringstream text;
		text << file.rdbuf();
		return affresco::Screen::fromString(text.str(), directory);
	}();
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
	const ManualClock clock;
	const auto window = createHeadlessWindow(1280, 800, clock);
	affresco::Screen screen = loadTemplate("kanban");
	screen.build(*window);
	window->render();

	affresco::Panel* members = screen.find("members");
	affresco::Panel* menu = screen.find("memberMenu");
	REQUIRE(members);
	REQUIRE(menu);
	CHECK(members->isShown());
	CHECK(members->frame().width() == 140.0f);
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

// Before, the panels and the window's signals still called into a destroyed screen, so the next input used freed
// memory (AddressSanitizer: heap-use-after-free in the runtime's handlers)
TEST_CASE("format: a screen destroyed while its window lives on takes back everything that calls into it") {
	ManualClock clock;
	const auto window = createHeadlessWindow(1280, 800, clock);
	auto screen = std::make_unique<affresco::Screen>(loadTemplate("kanban"));
	screen->build(*window);
	window->render();
	const affresco::Panel* members = screen->find("members");
	REQUIRE(members);
	const affresco::Vec2 at{members->frame().left + 4.0f, members->frame().top + 4.0f};
	window->setFocus(screen->find("members"));

	screen.reset();
	window->routeMouseMove(at);
	for(const affresco::Action action : {affresco::Action::Press, affresco::Action::Release})
		window->routeMouseButton({affresco::MouseButton::Left, action, {}, at});
	window->routeKey({affresco::Key::Escape, 0, affresco::Action::Press, {}});
	window->routeChar({U'x'});
	window->routeScroll({{0.0f, -1.0f}, at});
	window->routeMouseMove({1.0f, 1.0f});
	window->setSize(1000, 700);
	clock.advance(1000.0);
	window->render();
	CHECK_FALSE(window->secondsToNextTimer());
}

TEST_CASE("format: the shipped templates build on a headless window") {
	for(const std::string name :
	    {"kanban", "smart-home", "music-player", "habit-tracker", "sign-in", "launcher", "code-editor"}) {
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

namespace {

/// @brief A screen with an open overlay that "dismiss"es, whose parent is the root (so it has no anchor of its own).
constexpr const char* overlayScreen = R"json({
	"format": "affrescoui-screen", "version": 2,
	"state": { "open": true },
	"root": { "id": "main", "children": [
		{ "id": "menu", "width": 100, "height": 50,
		  "overlay": { "placement": "center", "open": "state.open", "dismiss": "outside" } }
	] }
})json";

} // namespace

// Before, the overlay's parent stood for its anchor, which toggles it itself: the root holds every point, so no click
// was outside it
TEST_CASE("format: an overlay whose parent is the root closes on a click outside it") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(800, 600, clock);
	affresco::Screen screen = affresco::Screen::fromString(overlayScreen);
	screen.build(*window);
	window->render();
	REQUIRE(screen.find("menu"));
	REQUIRE(screen.find("menu")->isShown());

	clickOn(*window, *screen.find("menu")); // inside: it stays
	window->render();
	CHECK(json::parse(screen.stateJson())["open"] == true);

	window->routeMouseButton({affresco::MouseButton::Left, affresco::Action::Press, {}, {5.0f, 5.0f}});
	window->routeMouseButton({affresco::MouseButton::Left, affresco::Action::Release, {}, {5.0f, 5.0f}});
	window->render();
	CHECK(json::parse(screen.stateJson())["open"] == false);
	CHECK_FALSE(screen.find("menu")->isShown());
}

TEST_CASE("format: Escape closes the open overlays when nothing has the focus") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(800, 600, clock);
	affresco::Screen screen = affresco::Screen::fromString(overlayScreen);
	screen.build(*window);
	window->render();
	REQUIRE_FALSE(window->focusedPanel());

	window->routeKey({affresco::Key::Escape, 0, affresco::Action::Press, {}});
	window->render();
	CHECK(json::parse(screen.stateJson())["open"] == false);
}

// Modello's screens name a surface its own program registers: in another host (affrescoui_launcher, the tools) the
// screen still opens, the surface empty
TEST_CASE("format: a surface no program registered here shows empty, and the screen opens") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(400, 300, clock);
	affresco::Screen screen = affresco::Screen::fromString(R"({
		"format": "affrescoui-screen", "version": 2,
		"root": { "id": "root", "children": [{ "type": "surface", "id": "view", "fill": 0, "surface": "nowhere" }] }
	})");
	CHECK_NOTHROW(screen.build(*window));
	window->render();
	CHECK(screen.find("view"));
	const auto other = createHeadlessWindow(400, 300, clock);
	affresco::Screen unnamed = affresco::Screen::fromString(R"({
		"format": "affrescoui-screen", "version": 2,
		"root": { "id": "root", "children": [{ "type": "surface", "id": "view" }] }
	})");
	CHECK_THROWS_AS(unnamed.build(*other), affresco::DocumentError); // names none
}
