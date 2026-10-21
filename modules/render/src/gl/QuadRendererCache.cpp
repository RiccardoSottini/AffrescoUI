/// @file
/// @brief gl::QuadRenderer's cached subtrees (Panel::setCached()): each drawn into a texture of its own when it
/// changes, and as one quad of that texture in every frame.

#include "gl/QuadRenderer.hpp"

#include "affresco/render/Panel.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace affresco::gl {

namespace {

/// @brief Where a rectangle lies in the framebuffer, in whole pixels holding it, cut to the framebuffer.
/// @param reach The rectangle, in window units.
/// @param scale Pixels per window unit, across and down.
/// @param framebufferWidth The framebuffer's width, in pixels.
/// @param framebufferHeight The framebuffer's height, in pixels.
/// @return Its area; empty (zero width or height) when it is outside.
SubtreeArea pixelArea(const Rect& reach, Vec2 scale, int framebufferWidth, int framebufferHeight) noexcept {
	const auto edge = [](float value, float factor, int limit, bool up) {
		const float pixels = up ? std::ceil(value * factor) : std::floor(value * factor);
		return static_cast<int>(std::clamp(pixels, 0.0f, static_cast<float>(limit)));
	};
	const int left = edge(reach.left, scale.x, framebufferWidth, false);
	const int top = edge(reach.top, scale.y, framebufferHeight, false);
	const int right = edge(reach.right, scale.x, framebufferWidth, true);
	const int bottom = edge(reach.bottom, scale.y, framebufferHeight, true);
	return {left, top, right - left, bottom - top, framebufferWidth, framebufferHeight, 0, 0};
}

} // namespace

bool QuadRenderer::replayCached(const Panel& panel, const detail::PanelGeometry& geometry) {
	if(frameWidth_ <= 0 || frameHeight_ <= 0) return false;
	const Vec2 scale{static_cast<float>(framebufferWidth_) / static_cast<float>(frameWidth_),
	                 static_cast<float>(framebufferHeight_) / static_cast<float>(frameHeight_)};
	SubtreeArea area =
	    pixelArea(detail::reachOf(panel.bounds(), geometry.record), scale, framebufferWidth_, framebufferHeight_);
	if(area.width <= 0 || area.height <= 0) return true; // off screen: nothing to draw
	area.frameWidth = frameWidth_;
	area.frameHeight = frameHeight_;

	SubtreeTextures::Entry* entry = subtrees_.acquire(geometry.id, area);
	if(!entry) return false;
	if(!entry->drawn || entry->version != geometry.subtreeVersion || entry->area != area)
		renderCached(panel, geometry, *entry, area);

	// One quad over its area; the texture's bottom row (v = 0) is the area's bottom
	if(batches_.empty()) useTexture(whiteTexture_.id(), {0.5f, 0.5f});
	useColorTexture(entry->texture.id(), nullptr, 0);
	const float left = static_cast<float>(area.left) / scale.x, top = static_cast<float>(area.top) / scale.y;
	const float right = static_cast<float>(area.left + area.width) / scale.x;
	const float bottom = static_cast<float>(area.top + area.height) / scale.y;
	constexpr std::uint16_t low = 0, high = 65535;
	const auto corner = [](float x, float y, std::uint16_t u, std::uint16_t v) {
		return detail::Vertex{x,   y,    u,    v, 255, 255, 255,
		                      255, 0.0f, 0.0f, 0, 0,   0,   static_cast<std::uint16_t>(detail::QuadKind::colorGlyph)};
	};
	vertices_.push_back(corner(left, top, low, high));
	vertices_.push_back(corner(right, top, high, high));
	vertices_.push_back(corner(right, bottom, high, low));
	vertices_.push_back(corner(left, bottom, low, low));
	++batches_.back().quadCount;
	return true;
}

// The frame so far is set aside whole, so the subtree's batches start afresh and the frame carries on as it was
void QuadRenderer::renderCached(const Panel& panel, const detail::PanelGeometry& geometry,
                                SubtreeTextures::Entry& entry, const SubtreeArea& area) {
	if(passes_.size() <= passDepth_) passes_.emplace_back();
	Pass& outer = passes_[passDepth_++];
	std::swap(outer.vertices, vertices_);
	std::swap(outer.batches, batches_);
	vertices_.clear();
	batches_.clear();
	std::optional<Rect> region = std::exchange(region_, std::nullopt); // the texture holds the whole subtree
	const std::uint32_t textPage = textPage_;

	replayContent(panel, geometry.record);
	submitInto(geometry, entry, area);

	std::swap(outer.vertices, vertices_);
	std::swap(outer.batches, batches_);
	region_ = region;
	textPage_ = textPage;
	--passDepth_;
	entry.version = geometry.subtreeVersion;
	entry.area = area;
	entry.drawn = true;
}

// The viewport is the framebuffer's, moved so the area lands on the texture: the quads keep their window units
void QuadRenderer::submitInto(const detail::PanelGeometry& geometry, SubtreeTextures::Entry& entry,
                              const SubtreeArea& area) {
	GLint framebuffer = 0;
	std::array<GLint, 4> viewport{};
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer);
	glGetIntegerv(GL_VIEWPORT, viewport.data());
	const bool scissor = glIsEnabled(GL_SCISSOR_TEST) == GL_TRUE;

	glBindFramebuffer(GL_FRAMEBUFFER, entry.framebuffer.id());
	glDisable(GL_SCISSOR_TEST);
	glViewport(-area.left, area.height + area.top - area.framebufferHeight, area.framebufferWidth,
	           area.framebufferHeight);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	// Colour as the frame blends it; coverage added up, so the texels come out premultiplied
	glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	submit(area.frameWidth, area.frameHeight, geometry.subtreeShapes, geometry.subtreeColor);

	if(passDepth_ == 1) glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // else back in an outer subtree's pass
	glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(framebuffer));
	glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
	if(scissor) glEnable(GL_SCISSOR_TEST);
}

} // namespace affresco::gl
