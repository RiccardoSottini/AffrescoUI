#pragma once
/// @file
/// @brief format::SecondWindows: the windows a screen's "windows" section declares, each showing a named image (a
/// node's snapshot), opened and closed by the "openWindow" and "closeWindow" steps.

#include "affresco/core/Json.hpp"
#include "affresco/core/Signal.hpp"

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>

namespace affresco {
class Application;
class ImageView;
class Window;
} // namespace affresco

namespace affresco::format {

struct Engine;

/// @brief The most windows a "windows" section declares.
inline constexpr std::size_t maxSecondWindows = 4;

/// @brief Checks a "windows" section: at most four windows, each `name → {title, width, height, image, background,
/// fullscreen, keys, onClick, onClose}` (SectionType::validate).
/// @param section The section.
/// @param at Its place, for errors.
/// @throws DocumentError If something is wrong.
void checkWindowsSection(const json& section, const std::string& at);

/// @brief The screen's other windows: each declared in the "windows" section, made when first opened (sharing the
/// screen's OpenGL context) and kept until the screen goes, hidden while closed, as windows can't be destroyed.
///
/// A window shows one named image, fitted and centred on its background: a node draws it with `snapshot`, so a slide
/// show, an audience view or a mirror is a part of the screen drawn a second time. Its key presses run the screen's
/// `keys` (unless `keys` is false) and a press of the mouse runs its `onClick`; closing it with its own button runs
/// its `onClose`. When the screen's window closes, so do they. Without an Application (headless: scenarios, checks,
/// previews) nothing is shown, but the `windows` variable follows the steps, so a screen's logic can be tested.
/// Invariants: at most maxSecondWindows windows; each entry of shown_ names one the section declares. Main thread only.
class SecondWindows {
public:
	/// @brief The windows of an engine.
	/// @param engine The runtime; outlives this object.
	explicit SecondWindows(Engine& engine) noexcept : engine_(engine) {}
	/// @brief Closes the windows, which stay hidden until the Application goes, and lets go of them.
	~SecondWindows();

	SecondWindows(const SecondWindows&) = delete;
	SecondWindows& operator=(const SecondWindows&) = delete;
	SecondWindows(SecondWindows&&) = delete;
	SecondWindows& operator=(SecondWindows&&) = delete;

	/// @brief Gives the Application that makes windows (Runtime::createWindow()); without one they are only recorded.
	/// @param application The application, or nullptr.
	void setApplication(Application* application) noexcept { application_ = application; }

	/// @brief Opens a declared window, or shows it again.
	/// @param name Its name in the "windows" section.
	/// @param fullscreen Whether it fills a display; nothing for its entry's "fullscreen".
	/// @param at The step's place, for errors.
	/// @throws DocumentError If the section doesn't declare it.
	void open(const std::string& name, std::optional<bool> fullscreen, const std::string& at);
	/// @brief Closes a window (nothing if it isn't open).
	/// @param name Its name.
	void close(const std::string& name);
	/// @brief Makes a window, or the screen's own ("main"), fill a display or not.
	/// @param name Its name, or "main".
	/// @param fullscreen true or false; nothing to toggle it.
	/// @param at The step's place, for errors.
	/// @throws DocumentError If there is no such window, or it isn't open.
	void setFullscreen(const std::string& name, std::optional<bool> fullscreen, const std::string& at);

	/// @brief The `windows` variable: `{main: {fullscreen}, <name>: {open, fullscreen}}` for each declared window.
	[[nodiscard]] json status() const;
	/// @brief Follows a new version of the document: windows no longer declared close, the others show their entry.
	void documentChanged();

private:
	/// @brief A declared window, once opened.
	struct Shown {
		Window* window = nullptr;     ///< The OS window, or nullptr headless.
		ImageView* view = nullptr;    ///< What shows its image, in its root.
		bool open = false;            ///< Whether it is open (what the variable says).
		bool fullscreen = false;      ///< Whether it fills a display (headless: what it was asked).
		Connection keyHook = 0;       ///< On its onKey.
		Connection buttonHook = 0;    ///< On its onMouseButton.
		std::uint64_t watchTimer = 0; ///< Its timer that closes it once the screen's window closes.
	};

	/// @brief Whether a window may start filling a display now: only while the person's press is delivered (to the
	/// screen's window or one of these), or the application's own code runs an action; else it reports the refusal.
	/// @param at The step's place, for the report.
	/// @return Whether it may.
	[[nodiscard]] bool mayFill(const std::string& at) const;
	/// @brief The entry of a declared window.
	/// @param name Its name.
	/// @return The entry, or nullptr.
	[[nodiscard]] const json* entry(const std::string& name) const;
	/// @brief Makes a window's OS window, its image and its hooks.
	/// @param name Its name.
	/// @param shown Its record.
	/// @param declared Its entry.
	void make(const std::string& name, Shown& shown, const json& declared);
	/// @brief Shows its entry's title, background and image.
	/// @param shown Its record, with a window.
	/// @param declared Its entry.
	void apply(Shown& shown, const json& declared) const;
	/// @brief Runs a window's handler, on the screen's root.
	/// @param name Its name.
	/// @param key "onClick" or "onClose".
	void runHandler(const std::string& name, const char* key);
	/// @brief Checks every half second, on the screen's window, whether the person closed a window with its button.
	void watch();
	/// @brief Closes a window once the screen's own is closing: it would keep the Application running.
	/// @param shown Its record.
	void watchScreen(Shown& shown);
	/// @brief Tells the screen the `windows` variable changed.
	void changed();

	Engine& engine_;                                  ///< The runtime.
	Application* application_ = nullptr;              ///< See setApplication().
	std::map<std::string, Shown, std::less<>> shown_; ///< The windows opened at least once, by name.
	std::uint64_t watchTimer_ = 0;                    ///< See watch(), on the screen's window; 0 when none is set.
};

} // namespace affresco::format
