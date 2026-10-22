#pragma once

/// @file
/// @brief detail::Renderer: what a Window draws its panels with. gl::OpenGlRenderer (OpenGL 4.1 core) is the one
/// implementation; a headless window has none.

#include "platform/Platform.hpp"

#include "affresco/render/Geometry.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace affresco {

class Panel;

namespace detail {

/// @brief What one frame shows: a laid-out tree, then its overlays, then the tooltip; and what the window knows changed
/// since the last frame it drew, besides what the panels say themselves.
struct Frame {
	Panel& root;                       ///< The tree, laid out; the renderer keeps its geometry up to date.
	std::span<Panel* const> overlays;  ///< Overlay panels, in drawing order (they are skipped inside the tree).
	Panel* tooltip = nullptr;          ///< The tooltip to draw last, or nullptr.
	int width = 0;                     ///< The window's width, in window units.
	int height = 0;                    ///< The window's height, in window units.
	std::uint64_t layoutPass = 0;      ///< The window's last layout pass: panels it laid out may have moved.
	std::uint64_t paintGeneration = 0; ///< A new generation records every panel again (Window::invalidate()).
	std::optional<Rect> damage;        ///< Where panels were that are gone, in window units.
	bool damageAll = false;            ///< Everything is damage: the window was resized, or invalidated.
};

/// @brief Draws a window's frames. Deciding whether a frame is needed, and laying it out, is the Window's job; the
/// renderer keeps each panel's geometry and draws again only where something changed (with a full frame as the
/// fallback), so the pixels are those a full frame would have.
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

	/// @brief Draws a frame into an image of its own from nothing: every panel recorded afresh, nothing retained used
	/// or kept. What the frames drawn from retained geometry and damage must look like (tests compare them).
	/// @param frame The frame.
	/// @param size The image's size, in pixels.
	/// @return Tightly packed 8-bit RGB, top row first.
	[[nodiscard]] virtual std::vector<std::uint8_t> drawFromScratch(const Frame& frame, platform::PixelSize size) = 0;

	/// @brief How many draw calls the last frame made, for Window::frameStatistics().
	/// @return The count, or 0 if this renderer doesn't count them.
	[[nodiscard]] virtual std::size_t drawCalls() const noexcept { return 0; }
};

} // namespace detail
} // namespace affresco
