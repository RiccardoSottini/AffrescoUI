#pragma once

/// @file
/// @brief detail::Renderer: what a Window draws its panels with. gl::OpenGlRenderer (OpenGL 4.1 core) is the one
/// implementation; a headless window has none.

#include "platform/Platform.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace affresco {

class Panel;

namespace detail {

/// @brief What one frame shows: a laid-out tree, then its overlays, then the tooltip.
struct Frame {
	const Panel& root;                ///< The tree, laid out.
	std::span<Panel* const> overlays; ///< Overlay panels, in drawing order (they are skipped inside the tree).
	const Panel* tooltip = nullptr;   ///< The tooltip to draw last, or nullptr.
	int width = 0;                    ///< The window's width, in window units.
	int height = 0;                   ///< The window's height, in window units.
};

/// @brief Draws a window's frames. It draws what it is given, every time: deciding whether a frame is needed, and
/// laying it out, is the Window's job.
///
/// Created for one window, in that window's OpenGL context, which is current whenever a function below is called
/// (and when it is destroyed). Main thread only.
class Renderer {
public:
	virtual ~Renderer() = default;

	Renderer() = default;
	Renderer(const Renderer&) = delete;
	Renderer& operator=(const Renderer&) = delete;
	Renderer(Renderer&&) = delete;
	Renderer& operator=(Renderer&&) = delete;

	/// @brief Clears the window's back buffer and draws a frame into it.
	/// @param frame The frame.
	/// @param size The framebuffer's size.
	virtual void draw(const Frame& frame, platform::PixelSize size) = 0;

	/// @brief Reads the window's back buffer, e.g. after draw().
	/// @param size The framebuffer's size.
	/// @return Tightly packed 8-bit RGB, top row first.
	[[nodiscard]] virtual std::vector<std::uint8_t> readBack(platform::PixelSize size) = 0;

	/// @brief Draws a frame into an image of its own, whatever the window's size on the display, and reads it.
	/// @param frame The frame.
	/// @param size The image's size, in pixels.
	/// @return Tightly packed 8-bit RGB, top row first.
	[[nodiscard]] virtual std::vector<std::uint8_t> drawOffscreen(const Frame& frame, platform::PixelSize size) = 0;
};

} // namespace detail
} // namespace affresco
