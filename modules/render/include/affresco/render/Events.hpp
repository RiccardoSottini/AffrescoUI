#pragma once

/// @file
/// @brief The typed input events a Window routes to panels, and what the desktop tells it: files dropped, the display's
/// scale and the system's colour scheme.

#include "affresco/render/Geometry.hpp"
#include "affresco/render/Input.hpp"

#include <string>
#include <vector>

namespace affresco {

/// @brief A key was pressed, repeated or released.
struct KeyEvent {
	Key key = Key::Unknown;        ///< The key.
	int scancode = 0;              ///< Platform-specific key code.
	Action action = Action::Press; ///< Press, repeat or release.
	Mods mods;                     ///< Modifier keys held.
};

/// @brief A Unicode character typed by the user, already composed by the OS keyboard layout.
struct CharEvent {
	char32_t codepoint = 0; ///< The character.
};

/// @brief The pointer moved.
struct MouseMoveEvent {
	Vec2 position; ///< In window units.
};

/// @brief A mouse button was pressed or released.
struct MouseButtonEvent {
	MouseButton button = MouseButton::Left; ///< The button.
	Action action = Action::Press;          ///< Press or release.
	Mods mods;                              ///< Modifier keys held.
	Vec2 position;                          ///< Pointer position, in window units.
};

/// @brief The wheel or trackpad scrolled.
struct ScrollEvent {
	Vec2 offset;   ///< Scroll amount per axis.
	Vec2 position; ///< Pointer position, in window units.
};

/// @brief The pointer entered or left the window.
struct CursorEnterEvent {
	bool entered = false; ///< true when it entered.
};

/// @brief The window was resized.
struct ResizeEvent {
	int width = 0;  ///< In window units.
	int height = 0; ///< In window units.
};

/// @brief Files were dropped on the window from the OS (a file manager, the desktop).
struct DropEvent {
	std::vector<std::string> paths; ///< The files and folders, as absolute UTF-8 paths.
	Vec2 position;                  ///< Where, in window units.
};

/// @brief Whether the system shows light or dark windows.
enum class ColorScheme { Light, Dark };

/// @brief The window's display or the system's appearance changed: it moved to a screen of another scale, or the user
/// switched to dark mode or back.
struct DisplayEvent {
	float scale = 1.0f;                           ///< Pixels per window unit.
	ColorScheme colorScheme = ColorScheme::Light; ///< The system's colour scheme.
};

} // namespace affresco
