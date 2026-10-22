#pragma once

/// @file
/// @brief gl::Surfaces: the framebuffers of the GPU surfaces a window draws, and the drawing into them.

#include "gl/GlObjects.hpp"

#include "affresco/render/GpuSurface.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace affresco::detail {
struct SurfaceState;
} // namespace affresco::detail

namespace affresco::gl {

/// @brief Renderbuffer object lifetime.
struct RenderbufferTraits {
	static GLuint create() {
		GLuint id = 0;
		glGenRenderbuffers(1, &id);
		return id;
	}
	static void destroy(GLuint id) noexcept { glDeleteRenderbuffers(1, &id); }
};

using Renderbuffer = Object<RenderbufferTraits>; ///< An owned renderbuffer.

/// @brief A texture and a framebuffer drawing into it.
struct Target {
	Texture texture;                       ///< The texture.
	Framebuffer framebuffer;               ///< The framebuffer.
	int width = 0;                         ///< Its width, in pixels.
	int height = 0;                        ///< Its height, in pixels.
	PassFormat format = PassFormat::rgba8; ///< Its format.
};

/// @brief The GPU surfaces of a window: each made the first time it is drawn, drawn into its framebuffer when its
/// state's version changed, and released by the first collect() after its panel is gone. Main thread, in the
/// window's context.
class Surfaces {
public:
	Surfaces() = default;
	~Surfaces();
	Surfaces(const Surfaces&) = delete;
	Surfaces& operator=(const Surfaces&) = delete;

	/// @brief A surface's texture, drawn again first if it changed.
	/// @param shared The panel's surface.
	/// @return Its texture, or 0 if it has nothing to draw (no size, or it failed).
	[[nodiscard]] GLuint place(const std::shared_ptr<detail::SurfaceState>& shared);
	/// @brief Releases the surfaces whose panels are gone. Call once per frame.
	void collect();
	/// @brief How many surfaces there are.
	[[nodiscard]] std::size_t count() const noexcept { return entries_.size(); }

	/// @brief The bytes a pass of this size and format takes.
	/// @param width Its width.
	/// @param height Its height.
	/// @param format Its format.
	/// @return width * height * bytes per texel.
	[[nodiscard]] static std::size_t bytesOf(int width, int height, PassFormat format) noexcept;

private:
	/// @brief A named pass.
	struct Pass {
		std::string name; ///< Its name.
		Target target;    ///< Its texture.
	};
	/// @brief A surface on the GPU.
	struct Entry {
		std::weak_ptr<detail::SurfaceState> state; ///< Its panel's: once that is gone, so is this.
		std::uint64_t surface = 0;                 ///< The state's surface counter it was made for.
		std::unique_ptr<GpuSurface> instance;      ///< The host's surface.
		Target color;                              ///< Its framebuffer and colour texture.
		Renderbuffer depth;                        ///< Its depth and stencil, if it asked for them.
		std::vector<Pass> passes;                  ///< Its passes, by name.
		float scale = 0.0f;                        ///< The scale it was prepared for.
		std::uint64_t version = 0;                 ///< The state's version last drawn.
		bool prepared = false;                     ///< prepare() ran for its size.
	};
	class Frame;

	/// @brief A panel's entry, emptied first when it was made for another panel or another surface.
	/// @param shared The panel's surface.
	/// @return The entry.
	Entry& entryFor(const std::shared_ptr<detail::SurfaceState>& shared);
	/// @brief Makes the surface if needed and draws it into its framebuffer, at this size; disables it on a failure.
	/// @param entry The entry.
	/// @param state Its state.
	/// @param width The width, in pixels.
	/// @param height The height, in pixels.
	/// @return Whether it drew.
	static bool draw(Entry& entry, detail::SurfaceState& state, int width, int height);
	/// @brief Makes or resizes an entry's framebuffer.
	/// @param entry The entry.
	/// @param state Its state.
	/// @param width The width, in pixels.
	/// @param height The height, in pixels.
	/// @return Whether it changed (prepare() must run again).
	static bool fit(Entry& entry, const detail::SurfaceState& state, int width, int height);
	/// @brief Draws a surface into its framebuffer, keeping the window's state.
	/// @param entry The entry.
	/// @param state Its state.
	/// @return Whether it drew without an error.
	static bool render(Entry& entry, detail::SurfaceState& state);
	/// @brief Disables a failed surface: logs once, releases it and frees its framebuffer.
	/// @param entry The entry.
	/// @param state Its state.
	/// @param reason What went wrong.
	static void fail(Entry& entry, detail::SurfaceState& state, const std::string& reason);
	/// @brief Releases an entry's surface (its release() in a guard; exceptions logged).
	/// @param entry The entry.
	static void release(Entry& entry) noexcept;

	std::unordered_map<const detail::SurfaceState*, Entry> entries_; ///< By state.
};

/// @brief Makes or remakes a target.
/// @param target The target.
/// @param width Its width, in pixels.
/// @param height Its height, in pixels.
/// @param format Its format.
void makeTarget(Target& target, int width, int height, PassFormat format);

} // namespace affresco::gl
