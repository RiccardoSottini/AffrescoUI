/// @file
/// @brief The runtime's scheduler on a headless window and a manual clock: "every", "after", "debounce", "animate",
/// colour transitions, and tasks deferred to the end of the event.

#include "Headless.hpp"

#include <affresco/affresco.hpp>

#include <doctest/doctest.h>

#include <memory>
#include <string>

// NOLINTBEGIN(bugprone-unchecked-optional-access): REQUIRE checks each optional first, and a missed one
// throws, failing the test

using affresco::json;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief A screen on a headless window, with a manual clock.
struct Scheduled {
	ManualClock clock;                                                                ///< Moved by the test.
	std::unique_ptr<affresco::Window> window = createHeadlessWindow(400, 300, clock); ///< Outlives the screen.
	affresco::Screen screen;                                                          ///< The screen.

	/// @brief Builds a screen and shows its first frame.
	/// @param document The screen.
	explicit Scheduled(std::string_view document) : screen(affresco::Screen::fromString(document)) {
		screen.build(*window);
		window->render();
	}

	/// @brief Moves the clock, then runs what is due and shows a frame.
	/// @param milliseconds How far.
	void advance(double milliseconds) {
		clock.advance(milliseconds);
		window->render();
	}

	/// @brief A value of the state.
	/// @param name Its key.
	/// @return The value.
	[[nodiscard]] json state(const char* name) const { return json::parse(screen.stateJson())[name]; }

	/// @brief Runs an action as if the root triggered it, then shows a frame.
	/// @param action The action.
	void run(std::string_view action) {
		screen.runAction(action, window->root());
		window->render();
	}
};

} // namespace

TEST_CASE("format: an \"every\" timer runs each period on the window's clock, until its node goes") {
	Scheduled s(R"json({"format": "affrescoui-screen", "version": 2, "state": { "ticks": 0 },
		"root": { "type": "panel", "children": [
			{ "id": "clock", "every": { "ms": 100, "do": [{ "increment": "state.ticks" }] } }
		] } })json");
	s.advance(99.0);
	CHECK(s.state("ticks") == 0);
	s.advance(1.0);
	CHECK(s.state("ticks") == 1);
	s.advance(100.0);
	s.advance(100.0);
	CHECK(s.state("ticks") == 3);

	REQUIRE(s.screen.applyRevision(R"({"remove": ["#clock"]})"));
	s.window->render();
	CHECK_FALSE(s.window->secondsToNextTimer());
	s.advance(1000.0);
	CHECK(s.state("ticks") == 3);
}

TEST_CASE("format: \"after\" runs once, and \"debounce\" only after the last of quick calls") {
	Scheduled s(R"json({"format": "affrescoui-screen", "version": 2, "state": { "after": 0, "debounced": 0 },
		"actions": {
			"later": [{ "after": 50, "do": [{ "increment": "state.after" }] }],
			"typed": [{ "debounce": 50, "do": [{ "increment": "state.debounced" }] }]
		},
		"root": { "type": "panel" } })json");
	s.run("later");
	s.advance(49.0);
	CHECK(s.state("after") == 0);
	s.advance(1.0);
	CHECK(s.state("after") == 1);
	s.advance(500.0);
	CHECK(s.state("after") == 1);

	s.run("typed");
	s.advance(40.0);
	s.run("typed"); // starts the wait again
	s.advance(40.0);
	CHECK(s.state("debounced") == 0);
	s.advance(10.0);
	CHECK(s.state("debounced") == 1);
	CHECK_FALSE(s.window->secondsToNextTimer());
}

TEST_CASE("format: an \"after\" too long or not a number waits as long as a delay may, or runs at once") {
	Scheduled s(R"json({"format": "affrescoui-screen", "version": 2, "state": { "big": 1e300, "ran": 0 },
		"actions": {
			"huge": [{ "after": "{{state.big * 10}}", "do": [{ "increment": "state.ran" }] }],
			"nan": [{ "after": "{{state.big * 10 - state.big * 10}}", "do": [{ "increment": "state.ran" }] }]
		},
		"root": { "type": "panel" } })json");
	s.run("huge");
	const auto wait = s.window->secondsToNextTimer();
	REQUIRE(wait);
	CHECK(*wait == doctest::Approx(2147483.647)); // a timer that never runs would stay on the window for good
	s.screen.runAction("nan", s.window->root());
	s.advance(0.0);
	CHECK(s.state("ran") == 1);
}

TEST_CASE("format: at most 10000 \"after\" steps wait at once; one more is an error") {
	Scheduled s(R"json({"format": "affrescoui-screen", "version": 2, "state": { "ran": 0 },
		"actions": {
			"many": [{ "forEach": "{{range(6000)}}", "do": [{ "after": 10, "do": [{ "increment": "state.ran" }] }] }]
		},
		"root": { "type": "panel" } })json");
	s.run("many");
	s.run("many"); // each frame looks at every timer waiting
	s.advance(10.0);
	CHECK(s.state("ran") == 10000);
	CHECK_FALSE(s.window->secondsToNextTimer());
}

TEST_CASE("format: an animation started again replaces the one running") {
	Scheduled s(R"json({"format": "affrescoui-screen", "version": 2, "state": { "x": 0 },
		"actions": {
			"up": [{ "animate": "state.x", "to": 100, "ms": 100, "easing": "linear" }],
			"down": [{ "animate": "state.x", "to": 0, "ms": 100, "easing": "linear" }]
		},
		"root": { "type": "panel" } })json");
	s.run("up");
	s.advance(16.0);
	s.advance(16.0);
	s.advance(18.0);
	CHECK(s.state("x").get<double>() == doctest::Approx(50.0));
	s.run("down"); // from 50 back to 0
	s.advance(16.0);
	s.advance(34.0);
	CHECK(s.state("x").get<double>() == doctest::Approx(25.0));
	s.advance(100.0);
	CHECK(s.state("x") == 0);
	s.window->render();
	CHECK_FALSE(s.window->secondsToNextTimer());
}

TEST_CASE("format: a colour transition eases on the window's clock") {
	Scheduled s(R"json({"format": "affrescoui-screen", "version": 2, "state": { "on": false },
		"root": { "type": "panel", "children": [
			{ "id": "lamp", "color": "{{state.on ? '#ffffff' : '#000000'}}", "transition": { "color": 100 } }
		] } })json");
	affresco::Panel* lamp = s.screen.find("lamp");
	REQUIRE(lamp);
	REQUIRE(s.screen.applyRevision(R"({"set": {"/state/on": true}})"));
	s.window->render();
	CHECK(lamp->color() == affresco::colors::black); // starts where it was

	s.advance(50.0); // halfway: "out" easing is 1 - (1 - 0.5)^3 = 0.875 of the way
	CHECK(lamp->color().r == doctest::Approx(0.875).epsilon(0.01));
	s.advance(50.0);
	CHECK(lamp->color() == affresco::colors::white);
	CHECK_FALSE(s.window->secondsToNextTimer());
}

TEST_CASE("format: a double click is two clicks closer than 0.4 s on the window's clock") {
	Scheduled s(R"json({"format": "affrescoui-screen", "version": 2, "state": { "clicks": 0, "doubles": 0 },
		"root": { "type": "panel", "children": [
			{ "id": "button", "width": 100, "height": 50,
			  "onClick": { "increment": "state.clicks" }, "onDoubleClick": { "increment": "state.doubles" } }
		] } })json");
	const auto click = [&] {
		for(const affresco::Action action : {affresco::Action::Press, affresco::Action::Release})
			s.window->routeMouseButton({affresco::MouseButton::Left, action, {}, {10.0f, 10.0f}});
		s.window->render();
	};
	click();
	s.clock.advance(500.0);
	click();
	CHECK(s.state("doubles") == 0);
	s.clock.advance(300.0);
	click();
	CHECK(s.state("clicks") == 3);
	CHECK(s.state("doubles") == 1);
}

// NOLINTEND(bugprone-unchecked-optional-access)
