#pragma once

/// @file
/// @brief Keyboard and mouse identifiers. Values match GLFW, so they convert with a static_cast.

#include <cstdint>

namespace affresco {

/// @brief A key, named after its position on a US layout.
enum class Key : int {
	Unknown = -1,
	Space = 32,
	Apostrophe = 39,
	Comma = 44,
	Minus = 45,
	Period = 46,
	Slash = 47,
	Num0 = 48,
	Num1 = 49,
	Num2 = 50,
	Num3 = 51,
	Num4 = 52,
	Num5 = 53,
	Num6 = 54,
	Num7 = 55,
	Num8 = 56,
	Num9 = 57,
	Semicolon = 59,
	Equal = 61,
	A = 65,
	B = 66,
	C = 67,
	D = 68,
	E = 69,
	F = 70,
	G = 71,
	H = 72,
	I = 73,
	J = 74,
	K = 75,
	L = 76,
	M = 77,
	N = 78,
	O = 79,
	P = 80,
	Q = 81,
	R = 82,
	S = 83,
	T = 84,
	U = 85,
	V = 86,
	W = 87,
	X = 88,
	Y = 89,
	Z = 90,
	LeftBracket = 91,
	Backslash = 92,
	RightBracket = 93,
	GraveAccent = 96,
	World1 = 161,
	World2 = 162,
	Escape = 256,
	Enter = 257,
	Tab = 258,
	Backspace = 259,
	Insert = 260,
	Delete = 261,
	Right = 262,
	Left = 263,
	Down = 264,
	Up = 265,
	PageUp = 266,
	PageDown = 267,
	Home = 268,
	End = 269,
	CapsLock = 280,
	ScrollLock = 281,
	NumLock = 282,
	PrintScreen = 283,
	Pause = 284,
	F1 = 290,
	F2 = 291,
	F3 = 292,
	F4 = 293,
	F5 = 294,
	F6 = 295,
	F7 = 296,
	F8 = 297,
	F9 = 298,
	F10 = 299,
	F11 = 300,
	F12 = 301,
	F13 = 302,
	F14 = 303,
	F15 = 304,
	F16 = 305,
	F17 = 306,
	F18 = 307,
	F19 = 308,
	F20 = 309,
	F21 = 310,
	F22 = 311,
	F23 = 312,
	F24 = 313,
	F25 = 314,
	Keypad0 = 320,
	Keypad1 = 321,
	Keypad2 = 322,
	Keypad3 = 323,
	Keypad4 = 324,
	Keypad5 = 325,
	Keypad6 = 326,
	Keypad7 = 327,
	Keypad8 = 328,
	Keypad9 = 329,
	KeypadDecimal = 330,
	KeypadDivide = 331,
	KeypadMultiply = 332,
	KeypadSubtract = 333,
	KeypadAdd = 334,
	KeypadEnter = 335,
	KeypadEqual = 336,
	LeftShift = 340,
	LeftControl = 341,
	LeftAlt = 342,
	LeftSuper = 343,
	RightShift = 344,
	RightControl = 345,
	RightAlt = 346,
	RightSuper = 347,
	Menu = 348,
};

/// @brief A mouse button.
enum class MouseButton : int {
	Button1 = 0,
	Button2 = 1,
	Button3 = 2,
	Button4 = 3,
	Button5 = 4,
	Button6 = 5,
	Button7 = 6,
	Button8 = 7,
	Left = Button1,   ///< Primary.
	Right = Button2,  ///< Secondary.
	Middle = Button3, ///< Wheel.
};

/// @brief What happened to a key or button.
enum class Action : int {
	Release = 0, ///< Released.
	Press = 1,   ///< Pressed.
	Repeat = 2,  ///< Held down (keys only).
};

/// @brief Mouse cursor shapes a panel can ask for while the pointer is over it.
enum class Cursor {
	Arrow,            ///< System default.
	IBeam,            ///< Text input.
	Hand,             ///< Clickable.
	ResizeHorizontal, ///< Horizontal drag handle.
	ResizeVertical,   ///< Vertical drag handle.
};

/// @brief Modifier keys held during a key or mouse button event.
struct Mods {
	int bits = 0; ///< GLFW_MOD_* bits.

	/// @brief Whether Shift was held.
	[[nodiscard]] constexpr bool shift() const noexcept { return bits & 0x0001; }
	/// @brief Whether Control was held.
	[[nodiscard]] constexpr bool control() const noexcept { return bits & 0x0002; }
	/// @brief Whether Alt (Option) was held.
	[[nodiscard]] constexpr bool alt() const noexcept { return bits & 0x0004; }
	/// @brief Whether Super (Cmd on macOS) was held.
	[[nodiscard]] constexpr bool super() const noexcept { return bits & 0x0008; }
	/// @brief Whether Caps Lock was on.
	[[nodiscard]] constexpr bool capsLock() const noexcept { return bits & 0x0010; }
	/// @brief Whether Num Lock was on.
	[[nodiscard]] constexpr bool numLock() const noexcept { return bits & 0x0020; }

	/// @brief Equal when every member is equal.
	constexpr bool operator==(const Mods&) const noexcept = default;
};

} // namespace affresco
