#pragma once

#include "sgl/Geometry.hpp"
#include "sgl/Input.hpp"

namespace sgl {

struct KeyEvent {
	Key key = Key::Unknown;
	int scancode = 0;
	Action action = Action::Press;
	Mods mods;
};

// A Unicode character typed by the user (already composed by the OS keyboard layout)
struct CharEvent {
	char32_t codepoint = 0;
};

struct MouseMoveEvent {
	Vec2 position;
};

struct MouseButtonEvent {
	MouseButton button = MouseButton::Left;
	Action action = Action::Press;
	Mods mods;
	Vec2 position;
};

struct ScrollEvent {
	Vec2 offset;
	Vec2 position;
};

struct CursorEnterEvent {
	bool entered = false;
};

struct ResizeEvent {
	int width = 0;
	int height = 0;
};

} // namespace sgl
