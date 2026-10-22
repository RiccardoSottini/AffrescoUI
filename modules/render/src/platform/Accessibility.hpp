#pragma once

/// @file
/// @brief The bridge from a window's accessibility tree (affresco/render/Accessibility.hpp) to the platform's API:
/// NSAccessibility on macOS, UI Automation on Windows, AT-SPI on Linux, through AccessKit (decision D8).

#include <memory>

namespace affresco {

class Window;

namespace platform {

/// @brief Hands one window's accessibility tree to the platform's API, and the actions asked of it to the panels. It
/// works only while an assistive technology is reading the window, and sends only what changed since the last frame.
/// Every call is on the main thread; the platform may ask for the tree or an action from another thread, which only
/// reads the last tree sent or queues the action for the next poll().
class AccessibilityAdapter {
public:
	virtual ~AccessibilityAdapter() = default;

	/// @brief At the start of each pass of the event loop: runs the actions asked for, follows the window's focus.
	virtual void poll() = 0;
	/// @brief After a frame is drawn: sends the elements that changed.
	virtual void frameDrawn() = 0;
	/// @brief The focused panel changed: sends it now.
	virtual void focusChanged() = 0;
};

/// @brief The adapter for a window on this platform.
/// @param window The window, shown on screen (not headless); it outlives the adapter.
/// @return The adapter, or nullptr where windows expose nothing (configured with AFFRESCOUI_ACCESSIBILITY=OFF).
[[nodiscard]] std::unique_ptr<AccessibilityAdapter> createAccessibilityAdapter(Window& window);

} // namespace platform
} // namespace affresco
