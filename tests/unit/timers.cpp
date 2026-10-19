/// @file
/// @brief Window's timers, and the timers and deferred tasks a screen sets on its window, on a headless window and a
/// clock the tests move by hand (they once needed a real window, and slept).

#include "Headless.hpp"
#include "Helpers.hpp"

#include <affresco/affresco.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

TEST_CASE("render: a timer cancelled by an earlier one in the same batch doesn't run") {
	ManualClock clock;
	const auto window = createHeadlessWindow(200, 100, clock);

	std::vector<std::string> ran;
	std::uint64_t second = 0;
	window->setTimer(0.0, [&] {
		ran.emplace_back("first");
		window->cancelTimer(second);
	});
	second = window->setTimer(0.0, [&] { ran.emplace_back("second"); });
	window->setTimer(0.0, [&] { ran.emplace_back("third"); });
	window->render(); // all three are due in this frame

	CHECK(ran == std::vector<std::string>{"first", "third"});
	CHECK_FALSE(window->secondsToNextTimer());
}

// The tasks point at the screen's runtime: if they outlived it, the window would run them on freed memory
// (AddressSanitizer: heap-use-after-free)
TEST_CASE("format: a screen destroyed while its window lives on cancels the timers it set") {
	ManualClock clock;
	const auto window = createHeadlessWindow(200, 100, clock);
	auto screen = std::make_unique<affresco::Screen>(affresco::Screen::fromString(R"json({
		"format": "affrescoui-screen",
		"version": 1,
		"state": { "x": 0, "y": 0 },
		"onLoad": [
			{ "animate": "state.x", "to": 100, "ms": 1000 },
			{ "debounce": 10, "do": [{ "set": "state.y", "to": 1 }] },
			{ "after": 10, "do": [{ "set": "state.y", "to": 2 }] },
			{ "focus": "#field" }
		],
		"root": { "type": "panel", "every": { "ms": 10, "do": [{ "increment": "state.y" }] }, "children": [
			{ "type": "textarea", "id": "field" }
		] }
	})json"));
	screen->build(*window);
	window->render(); // runs onLoad: an animation frame, a debounce, an after and a focus are waiting
	REQUIRE(window->secondsToNextTimer());

	screen.reset();
	CHECK_FALSE(window->secondsToNextTimer());
	clock.advance(40.0);
	window->render();
}

// The list of timers a frame runs is reachable for cancelTimer() only while the frame runs them, even when a task
// throws (AddressSanitizer with detect_stack_use_after_return=1: stack-use-after-return in cancelTimer)
TEST_CASE("render: a timer task that throws leaves the window's timers usable") {
	ManualClock clock;
	const auto window = createHeadlessWindow(200, 100, clock);

	window->setTimer(0.0, [] { throw std::runtime_error("a task failed"); });
	CHECK_THROWS_AS(window->render(), std::runtime_error);

	bool ran = false;
	const std::uint64_t cancelled = window->setTimer(0.0, [&] { ran = true; });
	window->cancelTimer(cancelled);
	window->render();
	CHECK_FALSE(ran);
}

// A document can set thousands of timers ("after" in a "forEach"). Before, a frame searched all of them for the next
// due one, each time: 10,000 timers took 125 ms, and 50,000 more than a second
TEST_CASE("render: a frame runs many due timers in order, in time that grows with their number") {
	ManualClock clock;
	const auto window = createHeadlessWindow(200, 100, clock);

	constexpr int count = 50000;
	std::vector<int> ran;
	ran.reserve(count);
	for(int i = 0; i < count; ++i) window->setTimer(0.0, [&ran, i] { ran.push_back(i); });

	const auto start = std::chrono::steady_clock::now();
	window->render();
	CHECK(std::chrono::steady_clock::now() - start < std::chrono::seconds(1));
	REQUIRE(ran.size() == count);
	CHECK(std::ranges::is_sorted(ran)); // earliest first, and in the order they were set when due at the same time
}

// The window can't cancel a deferred task, so a task of a screen destroyed before the next frame must do nothing
// (AddressSanitizer: heap-use-after-free in the "onLoad" task)
TEST_CASE("format: a screen destroyed before its window's next frame leaves no deferred task running") {
	ManualClock clock;
	const auto window = createHeadlessWindow(200, 100, clock);
	auto screen = std::make_unique<affresco::Screen>(affresco::Screen::fromString(R"json({
		"format": "affrescoui-screen",
		"version": 1,
		"state": { "y": 0 },
		"onLoad": [{ "set": "state.y", "to": 1 }],
		"root": { "type": "panel" }
	})json"));
	screen->build(*window); // "onLoad" is deferred to the first frame

	screen.reset();
	window->render();
}

// A declared channel that can't connect waits to try again on a window timer, which points at the screen
// (AddressSanitizer: heap-use-after-free in the reconnect task)
TEST_CASE("screen: a screen destroyed while its window lives on cancels its channels' reconnect timers") {
	ManualClock clock;
	const auto window = createHeadlessWindow(200, 100, clock);
	affresco::test::QueueExecutor executor;
	auto screen = std::make_unique<affresco::Screen>(affresco::Screen::fromString(R"json({
		"format": "affrescoui-screen",
		"version": 1,
		"channels": { "nobody": { "socket": "/nonexistent-affrescoui-test/nobody.sock", "reconnect": { "after": 1 } } },
		"root": { "type": "panel" }
	})json"));
	screen->build(*window);
	screen->startChannels(executor); // nothing listens there: a reconnect is waiting
	REQUIRE(window->secondsToNextTimer());

	screen.reset();
	CHECK_FALSE(window->secondsToNextTimer());
	clock.advance(40.0);
	window->render();
	executor.runPosted();
}

// The animations and transitions of a screen read the window's clock, like its timers: they finish when it says so
TEST_CASE("format: an animate step moves the state on the window's clock") {
	ManualClock clock;
	const auto window = createHeadlessWindow(200, 100, clock);
	affresco::Screen screen = affresco::Screen::fromString(R"json({
		"format": "affrescoui-screen",
		"version": 1,
		"state": { "x": 0 },
		"onLoad": [{ "animate": "state.x", "to": 100, "ms": 100, "easing": "linear" }],
		"root": { "type": "panel" }
	})json");
	screen.build(*window);
	window->render(); // starts it

	const auto x = [&] { return affresco::json::parse(screen.stateJson())["x"].get<double>(); };
	clock.advance(16.0);
	window->render(); // the first frame, at 16 ms
	CHECK(x() == doctest::Approx(16.0));
	clock.advance(34.0);
	window->render();
	CHECK(x() == doctest::Approx(50.0));
	clock.advance(1000.0);
	window->render();
	CHECK(x() == 100.0);
	window->render();
	CHECK_FALSE(window->secondsToNextTimer());
}
