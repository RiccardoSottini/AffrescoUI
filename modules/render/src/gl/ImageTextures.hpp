#pragma once

/// @file
/// @brief gl::ImageTextures: the textures of the images a window draws: small ones packed into shared pages.

#include "gl/GlObjects.hpp"

#include "affresco/render/Bitmap.hpp"
#include "affresco/render/Geometry.hpp"

#include "detail/ShelfPacker.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace affresco::gl {

/// @brief The textures of a window's images, uploaded the first time each is drawn.
///
/// An image at most largestPacked on each side goes to a shared page (pageSize square, RGBA), so that many small
/// images (icons, thumbnails, avatars) draw from one texture, in one batch; a larger one gets a texture of its own.
/// Each packed image has a one-texel border repeating its edge, so filtering never mixes in a neighbour. A texture is
/// freed by the first collect() after its image is gone, and a page once no image on it is alive. Main thread, in the
/// window's context.
class ImageTextures {
public:
	static constexpr int pageSize = 1024;     ///< The width and height of a shared page.
	static constexpr int largestPacked = 256; ///< The widest and tallest image packed into a page.

	/// @brief Where an image is on the GPU.
	struct Placement {
		GLuint texture; ///< Its texture.
		Rect uv;        ///< Its texture coordinates there.
	};

	/// @brief Where an image is, uploading it the first time.
	/// @param bitmap The image; shared, so that its texture can be freed once it is gone.
	/// @return Its texture and coordinates.
	[[nodiscard]] Placement place(const Bitmap& bitmap);
	/// @brief Frees the textures of images that are gone, and the pages nothing alive is on. Call once per frame.
	void collect();
	/// @brief How many textures there are: pages and images of their own.
	[[nodiscard]] std::size_t textures() const noexcept;

private:
	static constexpr std::size_t ownTexture = static_cast<std::size_t>(-1); ///< Entry::page of an unpacked image.

	/// @brief An image on the GPU.
	struct Entry {
		std::weak_ptr<const Bitmap> bitmap; ///< The image: once it is gone, so is this.
		std::size_t page = ownTexture;      ///< Its page, or ownTexture.
		Texture own;                        ///< Its own texture, if it isn't packed.
		Rect uv;                            ///< Its texture coordinates.
	};
	/// @brief A shared page.
	struct Page {
		Texture texture;                         ///< The page.
		detail::ShelfPacker packer{pageSize, 0}; ///< Where images go.
		std::size_t alive = 0;                   ///< How many of its images are alive.
	};

	/// @brief Packs an image into a page with room, or a new one.
	/// @param bitmap The image.
	/// @param entry Gets its page and coordinates.
	void pack(const Bitmap& bitmap, Entry& entry);

	std::unordered_map<std::uint64_t, Entry> entries_; ///< By Bitmap::id().
	std::vector<std::unique_ptr<Page>> pages_;         ///< Pages; null once freed, so indices stay.
};

} // namespace affresco::gl
