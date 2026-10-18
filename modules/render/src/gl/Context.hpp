#pragma once

/// @file
/// @brief Operations on the current OpenGL context, so that no code outside src/gl calls OpenGL directly.

#include <cstdint>
#include <vector>

namespace affresco::gl {

using ProcAddressLoader = void* (*)(const char* name); ///< Looks an OpenGL function up by name.

/// @brief Loads the OpenGL 4.1 core functions for the current context.
/// @param loader Looks the functions up.
/// @return false if the driver doesn't provide them.
[[nodiscard]] bool loadFunctions(ProcAddressLoader loader);

/// @brief Starts a frame: full-framebuffer viewport, alpha blending, cleared to transparent black.
/// @param framebufferWidth In pixels.
/// @param framebufferHeight In pixels.
void beginFrame(int framebufferWidth, int framebufferHeight);

/// @brief Reads the back buffer.
/// @param width In pixels.
/// @param height In pixels.
/// @return Tightly packed 8-bit RGB, top row first.
[[nodiscard]] std::vector<std::uint8_t> readBackBuffer(int width, int height);

/// @brief Draws into an offscreen image instead of the window while it exists: frames of a fixed size, whatever the
/// display (screenshots for tests).
class OffscreenTarget {
public:
	/// @brief Creates the image and draws into it from now on.
	/// @param width In pixels.
	/// @param height In pixels.
	OffscreenTarget(int width, int height);
	/// @brief Draws into the window again.
	~OffscreenTarget();

	OffscreenTarget(const OffscreenTarget&) = delete;
	OffscreenTarget& operator=(const OffscreenTarget&) = delete;

	/// @brief Reads what was drawn.
	/// @return Tightly packed 8-bit RGB, top row first.
	[[nodiscard]] std::vector<std::uint8_t> pixels() const;

private:
	int width_, height_;       ///< In pixels.
	unsigned framebuffer_ = 0; ///< The framebuffer object.
	unsigned color_ = 0;       ///< Its colour renderbuffer.
};

} // namespace affresco::gl
