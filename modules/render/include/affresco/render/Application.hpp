#pragma once

/// @file
/// @brief Application: owns the windowing system and every window, and runs the event loop.

#include "affresco/core/Executor.hpp"
#include "affresco/render/Clock.hpp"
#include "affresco/render/Window.hpp"

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace affresco {

namespace platform {
class Platform;
}

/// @brief Owns the windowing system (GLFW) and every Window, and runs the event loop.
///
/// One instance at a time. It is the Executor other modules post main-thread work to. Its windows read the time from
/// one SteadyClock.
class Application final : public Executor {
public:
	/// @brief Starts GLFW.
	/// @throws std::logic_error If another Application exists.
	/// @throws std::runtime_error If GLFW can't start.
	Application();
	~Application() override;

	Application(const Application&) = delete;
	Application& operator=(const Application&) = delete;
	Application(Application&&) = delete;
	Application& operator=(Application&&) = delete;

	/// @brief Opens a window with an OpenGL 4.1 core context.
	/// @param title The title.
	/// @param width Width in window units.
	/// @param height Height in window units.
	/// @return The window, owned by the application.
	/// @throws std::runtime_error If the window or its context can't be created.
	Window& createWindow(std::string title, int width, int height);

	/// @brief Runs the event loop until every window is closed or quit() is called.
	///
	/// Sleeps while idle and draws only what changed. `AFFRESCOUI_SCREENSHOT=<file.ppm>` saves the first frame of each
	/// window and returns instead; with `AFFRESCOUI_SCREENSHOT_AFTER=<ms>`, after running normally for that long.
	/// @return EXIT_SUCCESS, or EXIT_FAILURE if a screenshot failed.
	int run();

	/// @brief Makes run() return.
	void quit();

	/// @brief Runs a task on the main thread, inside run(). Safe to call from any thread.
	/// @param task The work to run.
	void post(std::function<void()> task) override;

private:
	// Declaration order matters: the windows go before the windowing system
	std::unique_ptr<platform::Platform> platform_; ///< The windowing system.
	SteadyClock clock_;                            ///< Every window's clock.
	std::vector<std::unique_ptr<Window>> windows_; ///< In creation order.
	bool running_ = false;                         ///< Cleared by quit().

	/// @brief Runs the tasks posted so far.
	void runPostedTasks();

	std::mutex tasksMutex_;                    ///< Guards tasks_.
	std::vector<std::function<void()>> tasks_; ///< Posted, not yet run.
};

} // namespace affresco
