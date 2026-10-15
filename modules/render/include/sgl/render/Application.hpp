#pragma once

/// @file
/// @brief Application: owns the windowing system and every window, and runs the event loop.

#include "sgl/core/Executor.hpp"
#include "sgl/render/Window.hpp"

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace sgl {

/// @brief Owns the windowing system (GLFW) and every Window, and runs the event loop.
///
/// One instance at a time. It is the Executor other modules post main-thread work to.
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
	/// Sleeps while idle and draws only what changed. `SGLGUI_SCREENSHOT=<file.ppm>` saves the first frame of each
	/// window and returns instead; with `SGLGUI_SCREENSHOT_AFTER=<ms>`, after running normally for that long.
	/// @return EXIT_SUCCESS, or EXIT_FAILURE if a screenshot failed.
	int run();

	/// @brief Makes run() return.
	void quit();

	/// @brief Runs a task on the main thread, inside run(). Safe to call from any thread.
	/// @param task The work to run.
	void post(std::function<void()> task) override;

private:
	std::vector<std::unique_ptr<Window>> windows_; ///< In creation order.
	bool running_ = false;                         ///< Cleared by quit().

	/// @brief Runs the tasks posted so far.
	void runPostedTasks();

	std::mutex tasksMutex_;                    ///< Guards tasks_.
	std::vector<std::function<void()>> tasks_; ///< Posted, not yet run.
};

} // namespace sgl
