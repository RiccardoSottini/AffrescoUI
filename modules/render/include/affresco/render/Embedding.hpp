#pragma once

/// @file
/// @brief affresco::Embedding: a Window drawn into an OpenGL context the host application owns, with no window or event
/// loop of its own. The host makes its context current, forwards its input, and calls frame() or renderTexture().
///
/// The embedding rule (what the library does to the host's OpenGL state): every call below that draws saves the state
/// it changes and restores it before returning (see Embedding::frame()). Only the GL objects the library creates are
/// left behind, and they are deleted with the Embedding, in the same context.

#include "affresco/core/Executor.hpp"
#include "affresco/render/Clock.hpp"
#include "affresco/render/Events.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace affresco {

class Window;

/// @brief Looks an OpenGL function of the host's context up by name (e.g. glfwGetProcAddress, SDL_GL_GetProcAddress).
using GlFunctionLoader = void* (*)(const char* name);

/// @brief A Window inside the host's OpenGL context. It is also the Executor its screen posts work to: posted tasks
/// run in runTasks(), frame() and renderTexture(), on the thread that owns it.
///
/// One thread: the one on which the host's context is current whenever a function is called (post() excepted, which
/// is safe from any thread). Its OpenGL objects belong to that context, which must be current when it is destroyed.
class Embedding final : public Executor {
public:
	/// @brief Creates the window and its renderer in the current context.
	/// @param loader The host's function loader; the context must be OpenGL 4.1 core or later, and current.
	/// @param clock The clock its timers read; must outlive it.
	/// @param title The window's title (what the screen reads and accessibility names).
	/// @param width The width, in window units, until the first frame gives one.
	/// @param height The height, in window units, until the first frame gives one.
	/// @throws std::runtime_error If the OpenGL functions or the shaders can't be loaded.
	Embedding(GlFunctionLoader loader, const Clock& clock, std::string title, int width, int height);

	/// @brief Deletes the window and the library's OpenGL objects; the host's context must be current.
	~Embedding() override;

	Embedding(const Embedding&) = delete;
	Embedding& operator=(const Embedding&) = delete;
	Embedding(Embedding&&) = delete;
	Embedding& operator=(Embedding&&) = delete;

	/// @brief The window: the host routes its input to it (Window::routeMouseMove() and the like).
	/// @return It.
	[[nodiscard]] Window& window() noexcept;

	/// @brief Queues a task for the owning thread. Safe to call from any thread.
	/// @param task The work.
	void post(std::function<void()> task) override;

	/// @brief Runs the posted tasks, then the window's timers and deferred tasks, without drawing.
	void runTasks();

	/// @brief Gives the window a size: in window units, its pixels divided by the scale.
	/// @param width In pixels.
	/// @param height In pixels.
	/// @param scale Pixels per window unit (2 on most HiDPI displays).
	/// @return false if a value is out of range (nothing is changed then).
	bool resize(int width, int height, float scale);

	/// @brief Tells the window whether the host shows light or dark, and whether it asks for less motion.
	/// @param scheme The scheme.
	/// @param reducedMotion true for less motion.
	void setAppearance(ColorScheme scheme, bool reducedMotion);

	/// @brief Runs the tasks, draws what changed into the library's texture, and draws that texture over the host's
	/// framebuffer (blended: premultiplied alpha), its whole viewport. Restores every piece of OpenGL state it changes:
	/// the framebuffer bindings, viewport, scissor, blending, colour mask, depth, stencil, culling, sRGB, program,
	/// vertex array, array buffer, active texture and the 2D textures of units 0 and 1, and the pixel store alignments.
	/// @param width The host framebuffer's width, in pixels.
	/// @param height Its height, in pixels.
	/// @param scale Pixels per window unit.
	/// @param framebuffer The framebuffer to draw over (0: the default one).
	/// @return false if a value is out of range.
	bool frame(int width, int height, float scale, unsigned framebuffer);

	/// @brief Like frame(), but leaves the picture in the library's texture: RGBA8, premultiplied alpha, its first row
	/// at the bottom (as OpenGL samples it). Valid until the next call or the Embedding's end.
	/// @param width In pixels.
	/// @param height In pixels.
	/// @param scale Pixels per window unit.
	/// @return The texture's OpenGL name, or 0 if a value is out of range.
	[[nodiscard]] unsigned renderTexture(int width, int height, float scale);

	/// @brief Renders like renderTexture() and reads the picture back.
	/// @param width In pixels.
	/// @param height In pixels.
	/// @param scale Pixels per window unit.
	/// @return Tightly packed 8-bit RGBA, premultiplied, top row first; empty if a value is out of range.
	[[nodiscard]] std::vector<std::uint8_t> renderPixels(int width, int height, float scale);

	/// @brief The largest width or height accepted, in pixels.
	static constexpr int maximumSize = 16384;

private:
	struct Impl;
	std::unique_ptr<Impl> impl_; ///< The window, its native stand-in, the texture and the compositor.
};

} // namespace affresco
