/// @file
/// @brief gl::OpenGlRenderer: draws a window's frames with OpenGL 4.1 core, through a gl::QuadRenderer.

#include "gl/OpenGlRenderer.hpp"
#include "gl/Context.hpp"
#include "gl/QuadRenderer.hpp"

#include <stdexcept>

namespace affresco::gl {

namespace {

/// @brief The OpenGL renderer: a QuadRenderer for the panels, and the frame's setup and read-back around it.
class OpenGlRenderer final : public detail::Renderer {
public:
	void draw(const detail::Frame& frame, platform::PixelSize size) override {
		beginFrame(size.width, size.height);
		quads_.draw(frame.root, frame.overlays, frame.tooltip, frame.width, frame.height);
	}

	[[nodiscard]] std::vector<std::uint8_t> readBack(platform::PixelSize size) override {
		return readBackBuffer(size.width, size.height);
	}

	[[nodiscard]] std::vector<std::uint8_t> drawOffscreen(const detail::Frame& frame,
	                                                      platform::PixelSize size) override {
		const OffscreenTarget target(size.width, size.height);
		draw(frame, size);
		return target.pixels();
	}

private:
	QuadRenderer quads_; ///< Draws the panels.
};

} // namespace

std::unique_ptr<detail::Renderer> createRenderer(platform::ProcAddressLoader loader) {
	if(!loadFunctions(loader)) throw std::runtime_error("AffrescoUI: could not load the OpenGL functions");
	return std::make_unique<OpenGlRenderer>();
}

} // namespace affresco::gl
