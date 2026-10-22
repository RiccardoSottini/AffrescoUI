/// @file
/// @brief Operations on the current context, in what OpenGL 4.1 core and OpenGL ES 3.0 share (Context.hpp).

#include "gl/Context.hpp"

#include <glad/glad.h>

#include <algorithm>
#include <cstddef>

namespace affresco::gl {

bool loadFunctions(ProcAddressLoader loader) {
#if defined(AFFRESCOUI_GLES3)
	return gladLoadGLES2Loader(loader) != 0;
#else
	return gladLoadGLLoader(loader) != 0;
#endif
}

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
	const auto columns = static_cast<std::size_t>(width);
	const auto rows = static_cast<std::size_t>(height);
	// RGBA: the one format OpenGL ES 3.0 always reads (RGB only if the driver offers it)
	std::vector<std::uint8_t> rgba(columns * rows * 4);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadBuffer(buffer);
	glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());

	// OpenGL returns the bottom row first: the usual top-down order, without the alpha
	std::vector<std::uint8_t> pixels(columns * rows * 3);
	for(std::size_t row = 0; row < rows; ++row) {
		const std::uint8_t* from = rgba.data() + (rows - 1 - row) * columns * 4;
		std::uint8_t* to = pixels.data() + row * columns * 3;
		for(std::size_t column = 0; column < columns; ++column, from += 4, to += 3) std::copy_n(from, 3, to);
	}
	return pixels;
}

} // namespace

std::vector<std::uint8_t> readBackBuffer(int width, int height) { return readPixels(GL_BACK, width, height); }

void beginRegion(int framebufferWidth, int framebufferHeight, const PixelRect& region) {
	glEnable(GL_SCISSOR_TEST);
	glScissor(region.x, region.y, region.width, region.height);
	beginFrame(framebufferWidth, framebufferHeight); // glClear honours the scissor: only the region is cleared
}

void endRegion() { glDisable(GL_SCISSOR_TEST); }

Canvas::~Canvas() {
	if(framebuffer_ == 0) return;
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDeleteFramebuffers(1, &framebuffer_);
	glDeleteRenderbuffers(1, &color_);
}

bool Canvas::resize(int width, int height) {
	if(framebuffer_ != 0 && width == width_ && height == height_) return false;
	if(framebuffer_ == 0) {
		glGenRenderbuffers(1, &color_);
		glGenFramebuffers(1, &framebuffer_);
	}
	width_ = width;
	height_ = height;
	glBindRenderbuffer(GL_RENDERBUFFER, color_);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, width, height);
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color_);
	return true;
}

void Canvas::bind() const { glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_); }

std::vector<std::uint8_t> Canvas::pixels() const {
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
	std::vector<std::uint8_t> result = readPixels(GL_COLOR_ATTACHMENT0, width_, height_);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	return result;
}

// The same size and format: a copy of every pixel as it is
void Canvas::copyToWindow() const {
	glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer_);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glBlitFramebuffer(0, 0, width_, height_, 0, 0, width_, height_, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

} // namespace affresco::gl
