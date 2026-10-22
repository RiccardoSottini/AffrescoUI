#pragma once

/// @file
/// @brief Operations on the current OpenGL context, so that no code outside src/gl calls OpenGL directly. Everything in
/// src/gl calls only what OpenGL 4.1 core and OpenGL ES 3.0 share, so one renderer runs on both backends.

#include <cstdint>
#include <vector>

namespace affresco::gl {

using ProcAddressLoader = void* (*)(const char* name); ///< Looks an OpenGL function up by name.

/// @brief Loads the functions of the current context: OpenGL 4.1 core, or OpenGL ES 3.0 (AFFRESCOUI_GL_BACKEND=gles3).
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

/// @brief A rectangle of a framebuffer's pixels, counted from its bottom-left corner as OpenGL does.
struct PixelRect {
	int x = 0;      ///< Its left edge.
	int y = 0;      ///< Its bottom edge.
	int width = 0;  ///< Its width.
	int height = 0; ///< Its height.
};

/// @brief Starts drawing only within a region of the current framebuffer: viewport, blending, the region cleared to
/// transparent black, and everything drawn clipped to it (scissor) until endRegion().
/// @param framebufferWidth In pixels.
/// @param framebufferHeight In pixels.
/// @param region The region.
void beginRegion(int framebufferWidth, int framebufferHeight, const PixelRect& region);

/// @brief Ends beginRegion(): drawing is no longer clipped.
void endRegion();

/// @brief An image that keeps what was drawn into it from frame to frame, so the next frame draws only what changed:
/// the window's frames go through one, then are copied to the window; screenshots for tests have their own.
///
/// Created and destroyed in its window's context (main thread).
class Canvas {
public:
	Canvas() = default;
	/// @brief Deletes the image.
	~Canvas();
	Canvas(const Canvas&) = delete;
	Canvas& operator=(const Canvas&) = delete;
	Canvas(Canvas&&) = delete;
	Canvas& operator=(Canvas&&) = delete;

	/// @brief Gives it a size, creating the image again if it has another one.
	/// @param width In pixels.
	/// @param height In pixels.
	/// @return true if the image was created: what was drawn is gone.
	bool resize(int width, int height);
	/// @brief Gives it a depth and stencil buffer (the clips of turned and skewed panels, gl::StencilClips), once.
	/// Call after resize(): it keeps it at each size from then on.
	void addStencil();
	/// @brief Draws into it from now on.
	void bind() const;
	/// @brief Reads what was drawn, then draws into the window again.
	/// @return Tightly packed 8-bit RGB, top row first.
	[[nodiscard]] std::vector<std::uint8_t> pixels() const;
	/// @brief Copies it to the window's back buffer, then draws into the window again.
	/// @param target The framebuffer to copy to: the window's (0), or an embedding's.
	void copyToWindow(unsigned target = 0) const;

private:
	int width_ = 0, height_ = 0; ///< In pixels.
	unsigned framebuffer_ = 0;   ///< The framebuffer object; 0 before the first resize().
	unsigned color_ = 0;         ///< Its colour renderbuffer.
	unsigned depthStencil_ = 0;  ///< Its depth and stencil renderbuffer; 0 before addStencil().

	/// @brief Gives the depth and stencil renderbuffer the canvas's size, attached to its framebuffer (bound).
	void storeStencil() const;
};

} // namespace affresco::gl
