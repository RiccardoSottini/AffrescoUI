/// @file
/// @brief gl::OpenGlRenderer: draws a window's frames with OpenGL 4.1 core, through a gl::QuadRenderer, into canvases
/// that keep what was drawn, so a frame draws only where something changed.

#include "gl/OpenGlRenderer.hpp"
#include "gl/Context.hpp"
#include "gl/QuadRenderer.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace affresco::gl {

namespace {

/// @brief A frame drawn only where it changed gives way to a full frame when the change covers more of the window
/// than this: drawing everything then costs about the same, and needs no culling.
constexpr float fullFrameShare = 0.6f;

/// @brief The pixels a region of the window covers, rounded out and a pixel wider on each side, so every pixel a quad
/// touching the region could change is in it.
/// @param region The region, in window units.
/// @param frame The frame (its size, in window units).
/// @param size The framebuffer's size.
/// @return The pixels, counted from the bottom as OpenGL does, within the framebuffer.
PixelRect pixelsOf(const Rect& region, const detail::Frame& frame, platform::PixelSize size) {
	const float scaleX = static_cast<float>(size.width) / static_cast<float>(std::max(frame.width, 1));
	const float scaleY = static_cast<float>(size.height) / static_cast<float>(std::max(frame.height, 1));
	const int left = std::max(static_cast<int>(std::floor(region.left * scaleX)) - 1, 0);
	const int right = std::min(static_cast<int>(std::ceil(region.right * scaleX)) + 1, size.width);
	const int top = std::max(static_cast<int>(std::floor(region.top * scaleY)) - 1, 0);
	const int bottom = std::min(static_cast<int>(std::ceil(region.bottom * scaleY)) + 1, size.height);
	return {left, size.height - bottom, std::max(right - left, 0), std::max(bottom - top, 0)};
}

/// @brief The region of the window some pixels cover: what a frame drawn in them must replay.
/// @param pixels The pixels.
/// @param frame The frame.
/// @param size The framebuffer's size.
/// @return The region, in window units.
Rect regionOf(const PixelRect& pixels, const detail::Frame& frame, platform::PixelSize size) {
	const float unitsX = static_cast<float>(frame.width) / static_cast<float>(std::max(size.width, 1));
	const float unitsY = static_cast<float>(frame.height) / static_cast<float>(std::max(size.height, 1));
	const int top = size.height - pixels.y - pixels.height;
	return {static_cast<float>(pixels.x) * unitsX, static_cast<float>(top) * unitsY,
	        static_cast<float>(pixels.x + pixels.width) * unitsX, static_cast<float>(top + pixels.height) * unitsY};
}

/// @brief The OpenGL renderer: a QuadRenderer for the panels, and canvases for the window and for screenshots.
class OpenGlRenderer final : public detail::Renderer {
public:
	void draw(const detail::Frame& frame, platform::PixelSize size) override {
		render(screen_, frame, size);
		screen_.canvas.copyToWindow(output_);
	}

	[[nodiscard]] std::vector<std::uint8_t> readBack(platform::PixelSize size) override {
		return readBackBuffer(size.width, size.height);
	}

	[[nodiscard]] std::size_t drawCalls() const noexcept override { return quads_.drawCalls(); }

	void setOutputFramebuffer(unsigned framebuffer) noexcept override { output_ = framebuffer; }

	[[nodiscard]] std::vector<std::uint8_t> drawOffscreen(const detail::Frame& frame,
	                                                      platform::PixelSize size) override {
		render(offscreen_, frame, size);
		return offscreen_.canvas.pixels();
	}

	[[nodiscard]] std::vector<std::uint8_t> drawFromScratch(const detail::Frame& frame,
	                                                        platform::PixelSize size) override {
		Canvas canvas;
		canvas.resize(size.width, size.height);
		canvas.addStencil();
		canvas.bind();
		beginFrame(size.width, size.height);
		quads_.drawFromScratch(frame);
		return canvas.pixels();
	}

private:
	/// @brief A canvas, and what changed since it was last drawn.
	struct Target {
		Canvas canvas;              ///< What was drawn.
		std::optional<Rect> damage; ///< What changed since, in window units.
		bool full = true;           ///< Everything changed (or the canvas is new).
		int shaders = -1;           ///< The shaders its pixels were drawn with: shapes (1), colour glyphs (2).
	};

	/// @brief Adds what changed to a target.
	/// @param target The target.
	/// @param frame The frame, with what the window knows changed.
	/// @param refreshed What refreshing the geometry found changed.
	static void addDamage(Target& target, const detail::Frame& frame, const detail::Refreshed& refreshed) {
		if(refreshed.damage) detail::addTo(target.damage, *refreshed.damage);
		if(frame.damage) detail::addTo(target.damage, *frame.damage);
		target.full = target.full || frame.damageAll;
	}

	/// @brief Brings a target up to date: the frame's geometry refreshed, the damage given to every target, then this
	/// one drawn where it changed, or whole when it is new, most of it changed, or the frame needs other shaders.
	/// @param target The target.
	/// @param frame The frame.
	/// @param size The target's size, in pixels.
	void render(Target& target, const detail::Frame& frame, platform::PixelSize size) {
		// The text's raster scale is the target's: a screenshot at another scale than the display records again
		const float pixelScale = static_cast<float>(size.width) / static_cast<float>(std::max(frame.width, 1));
		const detail::Refreshed refreshed = quads_.refresh(frame, pixelScale);
		addDamage(screen_, frame, refreshed);
		addDamage(offscreen_, frame, refreshed);

		const int shaders = (refreshed.shapes ? 1 : 0) + (refreshed.color ? 2 : 0);
		const bool created = target.canvas.resize(size.width, size.height);
		if(refreshed.transformed) target.canvas.addStencil();
		bool full = target.full || created || shaders != target.shaders;
		std::optional<PixelRect> pixels;
		if(!full && target.damage) {
			pixels = pixelsOf(*target.damage, frame, size);
			const float share = static_cast<float>(pixels->width) * static_cast<float>(pixels->height) /
			                    (static_cast<float>(size.width) * static_cast<float>(size.height));
			full = share > fullFrameShare;
		}
		target.canvas.bind();
		if(full) {
			beginFrame(size.width, size.height);
			quads_.draw(frame, std::nullopt, refreshed.shapes, refreshed.color);
		} else if(pixels && pixels->width > 0 && pixels->height > 0) {
			beginRegion(size.width, size.height, *pixels);
			quads_.draw(frame, regionOf(*pixels, frame, size), refreshed.shapes, refreshed.color);
			endRegion();
		}
		target.full = false;
		target.damage.reset();
		target.shaders = shaders;
	}

	QuadRenderer quads_;  ///< Draws the panels.
	Target screen_;       ///< The window's frames, copied to the window.
	Target offscreen_;    ///< Screenshots at a fixed scale.
	unsigned output_ = 0; ///< Where the window's frames are copied: see setOutputFramebuffer().
};

} // namespace

std::unique_ptr<detail::Renderer> createRenderer(platform::ProcAddressLoader loader) {
	if(!loadFunctions(loader)) throw std::runtime_error("AffrescoUI: could not load the OpenGL functions");
	return std::make_unique<OpenGlRenderer>();
}

} // namespace affresco::gl
