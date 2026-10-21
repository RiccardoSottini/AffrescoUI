#pragma once

/// @file
/// @brief gl::SubtreeTextures: the textures that cached subtrees (Panel::setCached()) are drawn into.

#include "gl/GlObjects.hpp"

#include <cstddef>
#include <cstdint>
#include <unordered_map>

namespace affresco::gl {

/// @brief Where a cached subtree's texture lies in the framebuffer, and the frame it was drawn for.
struct SubtreeArea {
	int left = 0;              ///< Its left edge, in framebuffer pixels.
	int top = 0;               ///< Its top edge, in framebuffer pixels from the top.
	int width = 0;             ///< Its width, in pixels.
	int height = 0;            ///< Its height, in pixels.
	int framebufferWidth = 0;  ///< The framebuffer's width, in pixels.
	int framebufferHeight = 0; ///< The framebuffer's height, in pixels.
	int frameWidth = 0;        ///< The window's width, in window units.
	int frameHeight = 0;       ///< The window's height, in window units.

	/// @brief Equal when every member is equal.
	bool operator==(const SubtreeArea&) const = default;
};

/// @brief The textures of a window's cached subtrees, by their geometry's id (detail::PanelGeometry::id).
///
/// Each is a framebuffer-sized-or-smaller RGBA texture with premultiplied texels, behind a framebuffer object. Their
/// pixels together stay within maxPixels: a subtree that would pass it is drawn directly instead. A texture not used
/// between two collect() calls is freed (its panel is gone, hidden or no longer cached). Main thread, in the window's
/// context.
class SubtreeTextures {
public:
	/// @brief The texels all the textures may hold (256 MiB of RGBA).
	static constexpr std::size_t maxPixels = std::size_t{64} << 20;

	/// @brief A subtree's texture and what it holds.
	struct Entry {
		Texture texture;           ///< The texture.
		Framebuffer framebuffer;   ///< Draws into it.
		SubtreeArea area;          ///< What it was drawn for.
		std::uint64_t version = 0; ///< The subtree's version it holds (detail::PanelGeometry::subtreeVersion).
		bool drawn = false;        ///< It holds a drawing.
		bool used = false;         ///< Used since the last collect().
	};

	/// @brief A subtree's texture, sized for an area (its old drawing is lost when the size changes), and marked used.
	/// @param id The subtree's geometry id.
	/// @param area Where it is drawn.
	/// @return The entry, or nullptr when it would pass maxPixels.
	Entry* acquire(std::uint64_t id, const SubtreeArea& area);
	/// @brief Frees the textures not used since the last call.
	void collect();

private:
	/// @brief Gives an entry a texture of a size, attached to its framebuffer.
	/// @param entry The entry.
	/// @param width Its width, in texels.
	/// @param height Its height, in texels.
	static void allocate(Entry& entry, int width, int height);

	std::unordered_map<std::uint64_t, Entry> entries_; ///< By geometry id.
	std::size_t pixels_ = 0;                           ///< The texels they hold.
};

} // namespace affresco::gl
