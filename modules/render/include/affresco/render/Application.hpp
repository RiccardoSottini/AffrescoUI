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
class NativeWindow;
class Platform;
}

/// @brief How an Application opens its windows, for programs that drive them themselves instead of a person: tests
/// and automation (affresco::automation plays scenarios on hidden windows, on a clock it moves by hand).
struct ApplicationOptions {
	/// @brief The clock every window reads; must outlive the Application. nullptr: the system's steady clock.
	const Clock* clock = nullptr;
	/// @brief Opens windows hidden: never on screen, drawn offscreen only (Window::saveScreenshot). The environment
	/// variable AFFRESCOUI_HIDDEN=1 turns it on for every Application of a process (hiddenWindowsRequested()).
	bool hiddenWindows = false;
};

/// @brief Whether a value of the environment variable AFFRESCOUI_HIDDEN asks for hidden windows, so that tests and
/// harnesses run every AffrescoUI program without a window on screen, in the Dock or in the app switcher.
/// @param value Its value, or nullptr when it is unset.
/// @return true unless it is unset, empty or "0".
[[nodiscard]] bool hiddenWindowsRequested(const char* value) noexcept;

/// @brief Owns the windowing system (GLFW) and every Window, and runs the event loop.
///
/// One instance at a time. It is the Executor other modules post main-thread work to. Its windows read the time from
/// one clock: a SteadyClock, unless ApplicationOptions gives another.
class Application final : public Executor {
public:
	/// @brief Starts GLFW, with windows shown and on the system's steady clock.
	/// @throws std::logic_error If another Application exists.
	/// @throws std::runtime_error If GLFW can't start.
	Application();
	/// @brief Starts GLFW, with windows opened as the options say.
	/// @param options The clock and whether windows are hidden.
	/// @throws std::logic_error If another Application exists.
	/// @throws std::runtime_error If GLFW can't start.
	explicit Application(ApplicationOptions options);
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
	/// @brief Opens a second window whose OpenGL context shares its objects (textures, buffers) with another's, and
	/// which draws with that window's default font: its glyphs are those already rasterised (SCREEN_FORMAT.md, "Second
	/// windows"). A presenter's audience view, a detached panel.
	/// @param title The title.
	/// @param width Width in window units.
	/// @param height Height in window units.
	/// @param sharing A window of this application (a headless one shares nothing).
	/// @return The window, owned by the application.
	/// @throws std::runtime_error If the window or its context can't be created.
	Window& createWindow(std::string title, int width, int height, const Window& sharing);

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
	/// @brief Takes a new native window, with a renderer in its context (current), as a Window.
	/// @param native The native window.
	/// @param title Its title.
	/// @param width Width in window units.
	/// @param height Height in window units.
	/// @return The window, owned by the application.
	Window& adopt(std::unique_ptr<platform::NativeWindow> native, std::string title, int width, int height);

	/// @brief Tells every window the system's colour scheme and motion setting, if they changed.
	void followSystem();

	// Declaration order matters: the windows go before the windowing system
	std::unique_ptr<platform::Platform> platform_; ///< The windowing system.
	SteadyClock steadyClock_;                      ///< The clock, unless the options gave another.
	const Clock* clock_ = &steadyClock_;           ///< Every window's clock; never null.
	std::vector<std::unique_ptr<Window>> windows_; ///< In creation order.
	bool running_ = false;                         ///< Cleared by quit().

	/// @brief Runs the tasks posted so far.
	void runPostedTasks();

	std::mutex tasksMutex_;                    ///< Guards tasks_.
	std::vector<std::function<void()>> tasks_; ///< Posted, not yet run.
};

} // namespace affresco
