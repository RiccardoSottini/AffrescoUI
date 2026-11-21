#pragma once

/// @file
/// @brief Application: owns the windowing system and every window, and runs the event loop.

#include "affresco/core/Executor.hpp"
#include "affresco/render/Clock.hpp"
#include "affresco/render/Window.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace affresco {

namespace platform {
class NativeWindow;
class Platform;
} // namespace platform

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

	/// @brief Makes run() return, once the task or event callback that calls it returns. Main thread only (from another
	/// thread, post() it).
	void quit();

	/// @brief Runs a task on the main thread, inside run(). Safe to call from any thread.
	/// @param task The work to run.
	void post(std::function<void()> task) override;

	/// @brief What woke the event loop from its wait.
	enum class Wake {
		Input,   ///< The windowing system delivered events: input, a resize, a redraw.
		Timer,   ///< A window's timer was due.
		Posted,  ///< A task was posted (post()).
		Unknown, ///< None of these: the system woke it for something that reached no window (or nothing woke it yet).
	};
	/// @brief How many times run()'s loop has come back from waiting, since the Application started: while nothing
	/// happens it waits, so an idle second turns it once per timer due and once per task posted (tests and an
	/// inspector read it).
	/// @return The count.
	[[nodiscard]] std::uint64_t turns() const noexcept { return turns_; }
	/// @brief How many of those turns had a reason (lastWake() after each).
	/// @param why The reason.
	/// @return The count.
	[[nodiscard]] std::uint64_t turns(Wake why) const { return turnsBy_.at(static_cast<std::size_t>(why)); }
	/// @brief Why the loop's last wait ended (turns()).
	/// @return Unknown before the first. When several came at once: Posted, then Input, then Timer.
	[[nodiscard]] Wake lastWake() const noexcept { return lastWake_; }

private:
	/// @brief Takes a new native window, with a renderer in its context (current), as a Window.
	/// @param native The native window.
	/// @param title Its title.
	/// @param width Width in window units.
	/// @param height Height in window units.
	/// @return The window, owned by the application.
	Window& adopt(std::unique_ptr<platform::NativeWindow> native, std::string title, int width, int height);

	/// @brief Tells every window the system's colour scheme and motion setting, if they changed. Where the system
	/// announces their changes (Platform::appearanceChanges()), they are read only after one.
	void followSystem();
	/// @brief Waits until there is input, a task posted or a timer due, and counts the turn (turns(), lastWake()).
	/// @param timeoutSeconds The longest wait, or std::nullopt to wait for an event.
	void wait(std::optional<double> timeoutSeconds);

	// Declaration order matters: the windows go before the windowing system
	std::unique_ptr<platform::Platform> platform_; ///< The windowing system.
	SteadyClock steadyClock_;                      ///< The clock, unless the options gave another.
	const Clock* clock_ = &steadyClock_;           ///< Every window's clock; never null.
	std::vector<std::unique_ptr<Window>> windows_; ///< In creation order.
	bool running_ = false;                         ///< Cleared by quit().
	std::uint64_t turns_ = 0;                      ///< See turns().
	Wake lastWake_ = Wake::Unknown;                ///< See lastWake().
	std::array<std::uint64_t, 4> turnsBy_{};       ///< turns(Wake), by Wake.
	std::optional<std::uint64_t> appearanceSeen_;  ///< The appearance changes followSystem() last read after.
	std::atomic<bool> posted_ = false;             ///< A task was posted since the last wait.

	/// @brief Runs the tasks posted so far.
	void runPostedTasks();

	std::mutex tasksMutex_;                    ///< Guards tasks_.
	std::vector<std::function<void()>> tasks_; ///< Posted, not yet run.
};

} // namespace affresco
