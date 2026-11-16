#pragma once

/// @file
/// @brief format::UpdateFrame: what every update does around the walk over the panels, before it (the screen, the
/// globals, the theme, the locale, the default font, the icon) and after it (watches, the title, the menu bar, the
/// focus ring, onUpdated).

#include "Reads.hpp"

#include <string>

namespace affresco::format {

struct Engine;

/// @brief The work of an update that isn't the walk over the panels: the window-wide values read before it, and what
/// follows from it after.
///
/// Invariants: begin() and end() run once per update, around Reconciler::updateRoot(); the window's size string it
/// keeps is the one the last begin() saw. Callers: the reconciler (Reconciler::update()). Main thread only.
class UpdateFrame {
public:
	/// @brief The frame of an engine's updates.
	/// @param engine The runtime; outlives this object.
	explicit UpdateFrame(Engine& engine) noexcept : engine_(engine) {}

	/// @brief Before the walk: keeps the screen an outlet shows, drops the globals, stales computed values on a resize,
	/// evaluates the theme and the locale, and gives the window its default font and its icon.
	/// @param changes What changed since the last update.
	/// @throws DocumentError If the icon can't be shown (the revision is refused, as an image node's would be).
	void begin(const StateChanges& changes);
	/// @brief After the walk: runs the "watch" handlers whose values changed, shows the title, the menu bar and the
	/// focus ring, and tells the application (Runtime::onUpdated).
	/// @param changes What changed since the last update.
	/// @param full Whether the walk visited everything.
	void end(const StateChanges& changes, bool full);

private:
	Engine& engine_;     ///< The runtime.
	std::string window_; ///< The window's size, scale and colour scheme last seen ("window" in computed values).
};

} // namespace affresco::format
