#pragma once

/// @file
/// @brief The typed input events a Window routes to panels.

#include "affresco/render/Geometry.hpp"
#include "affresco/render/Input.hpp"

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

} // namespace affresco
