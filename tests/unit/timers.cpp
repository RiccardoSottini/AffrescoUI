/// @file
/// @brief Tests that need a window: Window's timers, and the timers a screen sets on its window.
///
/// Built as affrescoui_window_tests, apart from the unit tests: a Window needs an OpenGL context, so these run where
/// the rendering check runs (a display, or Xvfb). The windows are never shown.

#include <affresco/affresco.hpp>

#include <GLFW/glfw3.h>
#include <doctest/doctest.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

/// @brief Waits long enough for the timers in these tests to be due.
void waitForTimers() { std::this_thread::sleep_for(std::chrono::milliseconds(40)); }

} // namespace

TEST_CASE("render: a timer cancelled by an earlier one in the same batch doesn't run") {
	affresco::Application app;
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	affresco::Window& window = app.createWindow("timers", 200, 100);

	std::vector<std::string> ran;
	std::uint64_t second = 0;
	window.setTimer(0.0, [&] {
		ran.emplace_back("first");
		window.cancelTimer(second);
	});
	second = window.setTimer(0.0, [&] { ran.emplace_back("second"); });
	window.setTimer(0.0, [&] { ran.emplace_back("third"); });
	waitForTimers();
	window.render(); // all three are due in this frame

	CHECK(ran == std::vector<std::string>{"first", "third"});
	CHECK_FALSE(window.secondsToNextTimer());
}

// The tasks point at the screen's runtime: if they outlived it, the window would run them on freed memory
// (AddressSanitizer: heap-use-after-free)
TEST_CASE("format: a screen destroyed while its window lives on cancels the timers it set") {
	affresco::Application app;
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
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
	affresco::Window& window = screen->createWindow(app);
	window.render(); // runs onLoad: an animation frame, a debounce, an after and a focus are waiting
	REQUIRE(window.secondsToNextTimer());

	screen.reset();
	CHECK_FALSE(window.secondsToNextTimer());
	waitForTimers();
	window.render();
}

// A document can set thousands of timers ("after" in a "forEach"). Before, a frame searched all of them for the next
// due one, each time: 10,000 timers took 125 ms, and 50,000 more than a second
// The list of timers a frame runs is reachable for cancelTimer() only while the frame runs them, even when a task
// throws (AddressSanitizer with detect_stack_use_after_return=1: stack-use-after-return in cancelTimer)
TEST_CASE("render: a timer task that throws leaves the window's timers usable") {
	affresco::Application app;
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	affresco::Window& window = app.createWindow("timers", 200, 100);

	window.setTimer(0.0, [] { throw std::runtime_error("a task failed"); });
	waitForTimers();
	CHECK_THROWS_AS(window.render(), std::runtime_error);

	bool ran = false;
	const std::uint64_t cancelled = window.setTimer(0.0, [&] { ran = true; });
	window.cancelTimer(cancelled);
	waitForTimers();
	window.render();
	CHECK_FALSE(ran);
}

TEST_CASE("render: a frame runs many due timers in order, in time that grows with their number") {
	affresco::Application app;
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	affresco::Window& window = app.createWindow("timers", 200, 100);

	constexpr int count = 50000;
	std::vector<int> ran;
	ran.reserve(count);
	for(int i = 0; i < count; ++i) window.setTimer(0.0, [&ran, i] { ran.push_back(i); });
	waitForTimers();

	const auto start = std::chrono::steady_clock::now();
	window.render();
	CHECK(std::chrono::steady_clock::now() - start < std::chrono::seconds(1));
	REQUIRE(ran.size() == count);
	CHECK(std::ranges::is_sorted(ran)); // earliest first, and in the order they were set when due at the same time
}

// The window can't cancel a deferred task, so a task of a screen destroyed before the next frame must do nothing
// (AddressSanitizer: heap-use-after-free in the "onLoad" task)
TEST_CASE("format: a screen destroyed before its window's next frame leaves no deferred task running") {
	affresco::Application app;
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	auto screen = std::make_unique<affresco::Screen>(affresco::Screen::fromString(R"json({
		"format": "affrescoui-screen",
		"version": 1,
		"state": { "y": 0 },
		"onLoad": [{ "set": "state.y", "to": 1 }],
		"root": { "type": "panel" }
	})json"));
	affresco::Window& window = screen->createWindow(app); // "onLoad" is deferred to the first frame

	screen.reset();
	window.render();
}

// A declared channel that can't connect waits to try again on a window timer, which points at the screen
// (AddressSanitizer: heap-use-after-free in the reconnect task)
TEST_CASE("screen: a screen destroyed while its window lives on cancels its channels' reconnect timers") {
	affresco::Application app;
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	auto screen = std::make_unique<affresco::Screen>(affresco::Screen::fromString(R"json({
		"format": "affrescoui-screen",
		"version": 1,
		"channels": { "nobody": { "socket": "/nonexistent-affrescoui-test/nobody.sock", "reconnect": { "after": 1 } } },
		"root": { "type": "panel" }
	})json"));
	affresco::Window& window = screen->createWindow(app); // nothing listens there: a reconnect is waiting
	REQUIRE(window.secondsToNextTimer());

	screen.reset();
	CHECK_FALSE(window.secondsToNextTimer());
	waitForTimers();
	window.render();
}
