/// @file
/// @brief The host's window on a change: what a proposal or a commit changes, file by file (VERSION_CONTROL.md, "The
/// changes, file by file"). It is the host's own screen, fed only by the host from the version control, so nothing in
/// the change it shows can alter it: while a proposal is previewed the running app's document is that proposal.

#pragma once

#include <affresco/affresco.hpp>

#include <filesystem>
#include <memory>

namespace host {

/// @brief Opens the window when the app asks (`{"call": "changes.open"}`) or the person presses Ctrl+Shift+D
/// (Cmd+Shift+D on macOS), which works whatever a previewed proposal has done to the app's own buttons.
class ChangesWindow {
public:
	/// @brief Makes the calls and the shortcut available to an app.
	/// @param app The application.
	/// @param screen The app's screen; must outlive this.
	/// @param window The app's window; must outlive this.
	/// @param file The window's screen file (examples/shared/host/changes.screen.json).
	ChangesWindow(affresco::Application& app, affresco::Screen& screen, affresco::Window& window,
	              std::filesystem::path file);
	~ChangesWindow();

	ChangesWindow(const ChangesWindow&) = delete;            ///< It holds a connection to the window's keys.
	ChangesWindow& operator=(const ChangesWindow&) = delete; ///< It holds a connection to the window's keys.

	/// @brief The window's screen, once it has been opened; for tests.
	/// @return It, or nullptr.
	[[nodiscard]] affresco::Screen* opened() const noexcept;

private:
	struct Impl;
	std::unique_ptr<Impl> impl_; ///< What it holds.
};

} // namespace host
