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

std::vector<std::uint8_t> readBackBuffer(int width, int height) {
	const auto rowSize = static_cast<std::size_t>(width) * 3;
	const auto rows = static_cast<std::size_t>(height);
	std::vector<std::uint8_t> pixels(rowSize * rows);

	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadBuffer(GL_BACK);
	glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

	// OpenGL returns the bottom row first: flip to the usual top-down order
	for(std::size_t top = 0, bottom = rows - 1; top < bottom; ++top, --bottom) {
		std::swap_ranges(pixels.begin() + static_cast<std::ptrdiff_t>(top * rowSize),
		                 pixels.begin() + static_cast<std::ptrdiff_t>((top + 1) * rowSize),
		                 pixels.begin() + static_cast<std::ptrdiff_t>(bottom * rowSize));
	}

	return pixels;
}

} // namespace affresco::gl
