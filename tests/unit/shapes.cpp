/// @file
/// @brief Tests that need a window: the shape properties of a screen (radius, borderWidth, borderColor, shadow), how
/// they are read and reset, and the border colour's transition.
///
/// Built into affrescoui_window_tests, with timers.cpp: a screen applies its properties once it has a window.

#include <affresco/affresco.hpp>

#include <GLFW/glfw3.h>
#include <doctest/doctest.h>

#include <chrono>
#include <memory>
#include <string>
#include <thread>

namespace {

/// @brief A screen with one node, "card", that has every shape property.
constexpr std::string_view cardScreen = R"json({
	"format": "affrescoui-screen",
	"version": 1,
	"state": { "on": false },
	"root": { "type": "panel", "children": [
		{ "id": "card", "width": 100, "height": 60,
		  "radius": { "topLeft": 4, "bottomRight": 12 },
		  "borderWidth": 2,
		  "borderColor": "{{state.on ? '#ff0000' : '#00ff00'}}",
		  "transition": { "borderColor": 200 },
		  "shadow": { "x": 1, "y": 2, "blur": 3, "color": "#00000080" } }
	] }
})json";

/// @brief Applies a revision that should fail, and returns its error.
/// @param screen The screen.
/// @param revision The revision.
/// @return The error text.
std::string rejection(affresco::Screen& screen, std::string_view revision) {
	std::string error;
	CHECK_FALSE(screen.applyRevision(revision, &error));
	return error;
}

} // namespace

TEST_CASE("format: shape properties apply, and go back to the defaults when removed") {
	affresco::Application app;
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	affresco::Screen screen = affresco::Screen::fromString(cardScreen);
	affresco::Window& window = screen.createWindow(app);
	window.render();

	affresco::Panel* card = screen.find("card");
	REQUIRE(card);
	CHECK(card->cornerRadii() == affresco::CornerRadii{.topLeft = 4.0f, .bottomRight = 12.0f});
	CHECK(card->borderWidth() == 2.0f);
	CHECK(card->borderColor() == affresco::Color::parseHex("#00ff00").value_or(affresco::Color{}));
	CHECK(card->shadow() ==
	      affresco::Shadow{{1.0f, 2.0f}, 3.0f, affresco::Color::parseHex("#00000080").value_or(affresco::Color{})});

	REQUIRE(screen.applyRevision(R"({"set": {"#card/radius": 8, "#card/shadow": {"y": 4}}})"));
	window.render();
	CHECK(card->cornerRadii() == affresco::CornerRadii::all(8.0f));
	CHECK(card->shadow() == affresco::Shadow{{0.0f, 4.0f}, 0.0f, affresco::colors::black.withAlpha(0.25f)});

	REQUIRE(screen.applyRevision(R"({"remove": ["#card/radius", "#card/borderWidth", "#card/shadow"]})"));
	window.render();
	CHECK(card->cornerRadii().isSquare());
	CHECK(card->borderWidth() == 0.0f);
	CHECK_FALSE(card->shadow().isVisible());
}

TEST_CASE("format: malformed shape properties are rejected, naming the place") {
	affresco::Application app;
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	affresco::Screen screen = affresco::Screen::fromString(cardScreen);
	screen.createWindow(app).render();

	CHECK(rejection(screen, R"({"set": {"#card/radius": -1}})").find("radius: expected a number of pixels") !=
	      std::string::npos);
	CHECK(rejection(screen, R"({"set": {"#card/radius": {"top": 4}}})").find("radius.top: unknown corner") !=
	      std::string::npos);
	CHECK(rejection(screen, R"({"set": {"#card/borderWidth": "thick"}})").find("borderWidth: expected a number") !=
	      std::string::npos);
	CHECK(rejection(screen, R"({"set": {"#card/shadow": {"spread": 2}}})").find("shadow.spread: unknown property") !=
	      std::string::npos);
	CHECK(rejection(screen, R"({"set": {"#card/shadow": {"blur": -4}}})").find("shadow.blur: expected a number") !=
	      std::string::npos);
	CHECK(rejection(screen, R"({"set": {"#card/shadow": {"x": 1e300}}})").find("shadow.x: number too large") !=
	      std::string::npos);
	CHECK(rejection(screen, R"({"set": {"#card/transition": {"radius": 100}}})").find("transition.radius") !=
	      std::string::npos);
}

// The transition's timer points at the screen's runtime, like the colour's: it must stop with the screen
TEST_CASE("format: a border colour transition animates, and stops when its screen is destroyed") {
	affresco::Application app;
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	auto screen = std::make_unique<affresco::Screen>(affresco::Screen::fromString(cardScreen));
	affresco::Window& window = screen->createWindow(app);
	window.render();
	CHECK_FALSE(window.secondsToNextTimer());

	REQUIRE(screen->applyRevision(R"({"set": {"/state/on": true}})"));
	window.render();
	REQUIRE(window.secondsToNextTimer()); // on its way from green to red
	CHECK(screen->find("card")->borderColor() == affresco::Color::parseHex("#00ff00").value_or(affresco::Color{}));

	screen.reset();
	CHECK_FALSE(window.secondsToNextTimer());
	window.render();
}

// A state ending or a revision removing the colour mid-way must not leave the old target behind
TEST_CASE("format: a border colour removed during its transition goes back to the default") {
	affresco::Application app;
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	affresco::Screen screen = affresco::Screen::fromString(cardScreen);
	affresco::Window& window = screen.createWindow(app);
	window.render();

	REQUIRE(screen.applyRevision(R"({"set": {"/state/on": true}})"));
	window.render();
	REQUIRE(window.secondsToNextTimer()); // on its way from green to red

	REQUIRE(screen.applyRevision(R"({"remove": ["#card/borderColor"]})"));
	window.render();
	for(int frame = 0; frame < 20 && window.secondsToNextTimer(); ++frame) {
		std::this_thread::sleep_for(std::chrono::milliseconds(40));
		window.render();
	}
	CHECK_FALSE(window.secondsToNextTimer());
	CHECK(screen.find("card")->borderColor() == affresco::colors::black);
}
