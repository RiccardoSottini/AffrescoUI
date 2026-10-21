#pragma once

/// @file
/// @brief Bitmap: decoded pixels (PNG, JPEG, or given by the host), what an ImageView shows.

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <vector>

namespace affresco {

/// @brief An image: premultiplied RGBA pixels, 8 bits a channel, top row first. Immutable once made, so it can be made
/// on one thread and shown on another, and shared by every view that shows it.
///
/// Images come from files and from the bytes of PNG and JPEG files (decoded with stb_image) and SVG icons
/// (rasterised at twice their size), or from pixels an
/// application makes (Screen::setImage). Both are input: a file a revision names or bytes a peer sends can be
/// anything, so decoding is bounded by Limits, checked before any pixel is decoded. Each window uploads an image to
/// its own GPU context the first time it draws it, and frees it the first frame after the image is gone.
class Bitmap : public std::enable_shared_from_this<Bitmap> {
public:
	/// @brief What an image may be: a file larger, or an image wider, taller or with more pixels, is refused.
	struct Limits {
		std::size_t fileBytes = std::size_t{64} << 20; ///< The largest file read.
		int side = 8192;                               ///< The widest and tallest image.
		std::size_t pixels = std::size_t{1} << 24;     ///< The most pixels (64 MiB decoded): 4096 by 4096.
	};

	/// @brief Decodes a PNG, JPEG or SVG file.
	/// @param file A regular file of at most Limits::fileBytes.
	/// @param limits What the image may be.
	/// @return The image.
	/// @throws std::runtime_error If the file can't be read, is too large, or isn't an image within the limits.
	[[nodiscard]] static std::shared_ptr<const Bitmap> load(const std::filesystem::path& file, const Limits& limits);
	/// @brief Decodes the bytes of a PNG, JPEG or SVG file.
	/// @param bytes The bytes.
	/// @param limits What the image may be.
	/// @return The image.
	/// @throws std::runtime_error If they aren't an image within the limits.
	[[nodiscard]] static std::shared_ptr<const Bitmap> decode(std::span<const std::uint8_t> bytes,
	                                                          const Limits& limits);
	/// @brief Makes an image of pixels.
	/// @param width Its width, at least 1.
	/// @param height Its height, at least 1.
	/// @param rgba Red, green, blue and alpha per pixel, top row first, not premultiplied: width * height * 4 bytes.
	/// @param limits What the image may be.
	/// @return The image.
	/// @throws std::invalid_argument If the size is empty, beyond the limits, or doesn't match @p rgba.
	[[nodiscard]] static std::shared_ptr<const Bitmap>
	fromPixels(int width, int height, std::span<const std::uint8_t> rgba, const Limits& limits);

	/// @brief Frees the pixels.
	~Bitmap() = default;
	Bitmap(const Bitmap&) = delete;            ///< Shared through std::shared_ptr instead.
	Bitmap& operator=(const Bitmap&) = delete; ///< Shared through std::shared_ptr instead.

	/// @brief Its width, in pixels.
	[[nodiscard]] int width() const noexcept { return width_; }
	/// @brief Its height, in pixels.
	[[nodiscard]] int height() const noexcept { return height_; }
	/// @brief Its pixels: premultiplied red, green, blue and alpha, top row first.
	[[nodiscard]] std::span<const std::uint8_t> pixels() const noexcept { return pixels_; }
	/// @brief Unique per image made, e.g. to cache its GPU texture.
	[[nodiscard]] std::uint64_t id() const noexcept { return id_; }
	/// @brief How much memory it takes.
	[[nodiscard]] std::size_t memoryBytes() const noexcept { return sizeof(Bitmap) + pixels_.capacity(); }

private:
	/// @brief Lets make_shared reach the constructor, and nothing else.
	struct Key {};

public:
	/// @brief An image of pixels already premultiplied and checked; use the functions above.
	/// @param key Only Bitmap can make one.
	/// @param width Its width.
	/// @param height Its height.
	/// @param pixels Its pixels.
	Bitmap(Key key, int width, int height, std::vector<std::uint8_t> pixels);

private:
	int width_ = 0;                    ///< See width().
	int height_ = 0;                   ///< See height().
	std::vector<std::uint8_t> pixels_; ///< See pixels().
	std::uint64_t id_ = 0;             ///< See id().
};

} // namespace affresco
