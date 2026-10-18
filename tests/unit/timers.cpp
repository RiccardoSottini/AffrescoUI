/// @file
/// @brief Tests that need a window: Window's timers, and the timers a screen sets on its window.
///
/// Built as affrescoui_window_tests, apart from the unit tests: a Window needs an OpenGL context, so these run where
/// the rendering check runs (a display, or Xvfb). The windows are never shown.

#include <affresco/affresco.hpp>

#include <GLFW/glfw3.h>
#include <doctest/doctest.h>

#include <chrono>
#include <cstdint>
#include <memory>
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
