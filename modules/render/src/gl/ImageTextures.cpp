/// @file
/// @brief gl::ImageTextures: uploading images, packed with an edge border or alone, and freeing them when they are
/// gone.

#include "gl/ImageTextures.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <optional>

namespace affresco::gl {

namespace {

/// @brief Makes an RGBA texture, linearly filtered, clamped at its edges.
/// @param width Its width.
/// @param height Its height.
/// @param pixels Its texels, top row first, or nullptr to leave them undefined.
/// @return The texture.
Texture createTexture(int width, int height, const std::uint8_t* pixels) {
	Texture texture = Texture::create();
	glBindTexture(GL_TEXTURE_2D, texture.id());
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	return texture;
}

/// @brief An image with a one-texel border that repeats its edge texels.
/// @param bitmap The image.
/// @return (width + 2) * (height + 2) RGBA texels, top row first.
std::vector<std::uint8_t> withBorder(const Bitmap& bitmap) {
	const auto width = static_cast<std::size_t>(bitmap.width()), height = static_cast<std::size_t>(bitmap.height());
	const std::size_t stride = (width + 2) * 4;
	std::vector<std::uint8_t> texels(stride * (height + 2));
	const std::uint8_t* pixels = bitmap.pixels().data();
	for(std::size_t row = 0; row < height + 2; ++row) {
		const std::size_t from = std::clamp<std::size_t>(row, 1, height) - 1; // the edge rows twice
		std::uint8_t* to = texels.data() + row * stride;
		std::memcpy(to + 4, pixels + from * width * 4, width * 4);
		std::memcpy(to, to + 4, 4);                           // the left edge
		std::memcpy(to + (width + 1) * 4, to + width * 4, 4); // the right edge
	}
	return texels;
}

} // namespace

ImageTextures::Placement ImageTextures::place(const Bitmap& bitmap) {
	Entry& entry = entries_[bitmap.id()];
	if(entry.bitmap.expired()) { // new (an image's id is never reused)
		entry.bitmap = bitmap.weak_from_this();
		if(bitmap.width() <= largestPacked && bitmap.height() <= largestPacked) {
			pack(bitmap, entry);
		} else {
			entry.own = createTexture(bitmap.width(), bitmap.height(), bitmap.pixels().data());
			entry.uv = {0.0f, 0.0f, 1.0f, 1.0f};
		}
	}
	return {entry.page == ownTexture ? entry.own.id() : pages_[entry.page]->texture.id(), entry.uv};
}

void ImageTextures::pack(const Bitmap& bitmap, Entry& entry) {
	const int width = bitmap.width() + 2, height = bitmap.height() + 2;
	std::optional<std::array<int, 2>> spot;
	std::size_t index = 0;
	for(; index < pages_.size() && !spot; ++index)
		if(pages_[index]) spot = pages_[index]->packer.place(width, height, 0);
	if(spot) {
		--index;
	} else { // a new page, in the place of a freed one if there is one
		const auto freed = std::ranges::find(pages_, nullptr);
		index = static_cast<std::size_t>(freed - pages_.begin());
		if(freed == pages_.end()) pages_.emplace_back();
		pages_[index] = std::make_unique<Page>(Page{createTexture(pageSize, pageSize, nullptr)});
		spot = pages_[index]->packer.place(width, height, 0);
	}

	Page& page = *pages_[index];
	const std::vector<std::uint8_t> texels = withBorder(bitmap);
	glBindTexture(GL_TEXTURE_2D, page.texture.id());
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexSubImage2D(GL_TEXTURE_2D, 0, (*spot)[0], (*spot)[1], width, height, GL_RGBA, GL_UNSIGNED_BYTE, texels.data());
	++page.alive;

	const float texel = 1.0f / static_cast<float>(pageSize);
	const auto left = static_cast<float>((*spot)[0] + 1), top = static_cast<float>((*spot)[1] + 1);
	entry.page = index;
	entry.uv = {left * texel, top * texel, (left + static_cast<float>(bitmap.width())) * texel,
	            (top + static_cast<float>(bitmap.height())) * texel};
}

void ImageTextures::collect() {
	std::erase_if(entries_, [this](const auto& item) {
		const Entry& entry = item.second;
		if(!entry.bitmap.expired()) return false;
		if(entry.page != ownTexture && --pages_[entry.page]->alive == 0) pages_[entry.page].reset();
		return true;
	});
}

std::size_t ImageTextures::textures() const noexcept {
	const auto own = std::ranges::count_if(entries_, [](const auto& item) { return item.second.page == ownTexture; });
	const auto pages = std::ranges::count_if(pages_, [](const auto& page) { return page != nullptr; });
	return static_cast<std::size_t>(own + pages);
}

} // namespace affresco::gl
