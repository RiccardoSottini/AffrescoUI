/// @file
/// @brief gl::SubtreeTextures: cached subtrees' textures, sized on demand within a budget, freed once unused.

#include "gl/SubtreeTextures.hpp"

namespace affresco::gl {

namespace {

/// @brief The texels of an area.
/// @param area The area.
std::size_t pixelsOf(const SubtreeArea& area) noexcept {
	return static_cast<std::size_t>(area.width) * static_cast<std::size_t>(area.height);
}

} // namespace

SubtreeTextures::Entry* SubtreeTextures::acquire(std::uint64_t id, const SubtreeArea& area) {
	const auto found = entries_.find(id);
	const std::size_t held = found == entries_.end() || !found->second.texture.id() ? 0 : pixelsOf(found->second.area);
	const bool resized =
	    held == 0 || found->second.area.width != area.width || found->second.area.height != area.height;
	if(resized && pixels_ - held + pixelsOf(area) > maxPixels) {
		if(found != entries_.end()) {
			pixels_ -= held;
			entries_.erase(found);
		}
		return nullptr;
	}

	Entry& entry = found == entries_.end() ? entries_[id] : found->second;
	if(resized) {
		allocate(entry, area.width, area.height);
		pixels_ = pixels_ - held + pixelsOf(area);
		entry.drawn = false;
	}
	entry.area.width = area.width; // the rest of the area is set once drawn
	entry.area.height = area.height;
	entry.used = true;
	return &entry;
}

void SubtreeTextures::collect() {
	std::erase_if(entries_, [this](auto& item) {
		Entry& entry = item.second;
		if(entry.used) {
			entry.used = false;
			return false;
		}
		pixels_ -= pixelsOf(entry.area);
		return true;
	});
}

void SubtreeTextures::allocate(Entry& entry, int width, int height) {
	entry.texture = Texture::create();
	glBindTexture(GL_TEXTURE_2D, entry.texture.id());
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	// Texel for pixel: the quad covers exactly the pixels it was drawn from
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	if(!entry.framebuffer.id()) entry.framebuffer = Framebuffer::create();
	GLint previous = 0;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous);
	glBindFramebuffer(GL_FRAMEBUFFER, entry.framebuffer.id());
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, entry.texture.id(), 0);
	glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previous));
}

} // namespace affresco::gl
