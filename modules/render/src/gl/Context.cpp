#include "gl/Context.hpp"

#include <glad/glad.h>

#include <algorithm>
#include <cstddef>

namespace affresco::gl {

bool loadFunctions(ProcAddressLoader loader) { return gladLoadGLLoader(loader) != 0; }

void beginFrame(int framebufferWidth, int framebufferHeight) {
	glViewport(0, 0, framebufferWidth, framebufferHeight);

	// Standard alpha blending, so translucent and transparent panels show what is behind them
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);
}

namespace {

/// @brief Reads the current read buffer.
/// @param buffer GL_BACK, or a colour attachment.
/// @param width In pixels.
/// @param height In pixels.
/// @return Tightly packed 8-bit RGB, top row first.
std::vector<std::uint8_t> readPixels(GLenum buffer, int width, int height) {
	const auto rowSize = static_cast<std::size_t>(width) * 3;
	const auto rows = static_cast<std::size_t>(height);
	std::vector<std::uint8_t> pixels(rowSize * rows);

	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadBuffer(buffer);
	glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

	// OpenGL returns the bottom row first: flip to the usual top-down order
	for(std::size_t top = 0, bottom = rows - 1; top < bottom; ++top, --bottom) {
		std::swap_ranges(pixels.begin() + static_cast<std::ptrdiff_t>(top * rowSize),
		                 pixels.begin() + static_cast<std::ptrdiff_t>((top + 1) * rowSize),
		                 pixels.begin() + static_cast<std::ptrdiff_t>(bottom * rowSize));
	}

	return pixels;
}

} // namespace

std::vector<std::uint8_t> readBackBuffer(int width, int height) { return readPixels(GL_BACK, width, height); }

OffscreenTarget::OffscreenTarget(int width, int height) : width_(width), height_(height) {
	glGenRenderbuffers(1, &color_);
	glBindRenderbuffer(GL_RENDERBUFFER, color_);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, width, height);

	glGenFramebuffers(1, &framebuffer_);
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color_);
}

OffscreenTarget::~OffscreenTarget() {
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDeleteFramebuffers(1, &framebuffer_);
	glDeleteRenderbuffers(1, &color_);
}

std::vector<std::uint8_t> OffscreenTarget::pixels() const { return readPixels(GL_COLOR_ATTACHMENT0, width_, height_); }

} // namespace affresco::gl
