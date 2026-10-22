/// @file
/// @brief detail::PdfPainter: Painter calls as PDF operators.

#include "PdfPainter.hpp"

#include "affresco/render/Bitmap.hpp"

#include "PdfShapes.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace affresco::detail {

namespace {

/// The slant of synthetic italic, as the rasteriser applies it: tan(11 degrees).
constexpr float italicSlant = 0.19438f;
/// How many layers draw a blurred shadow.
constexpr int shadowLayers = 6;

/// @brief A rectangle grown on every side.
/// @param rect The rectangle.
/// @param by How much (negative shrinks).
/// @return It.
Rect grown(const Rect& rect, float by) noexcept {
	return {rect.left - by, rect.top - by, rect.right + by, rect.bottom + by};
}

/// @brief A colour glyph's texels, cut from its atlas page and no longer premultiplied.
/// @param font The font.
/// @param glyph The glyph, on a colour page.
/// @param width Set to its width in texels.
/// @param height Set to its height.
/// @return Its pixels, 4 bytes each; empty if it has none.
std::vector<std::uint8_t> cutGlyph(const Font& font, const Font::Glyph& glyph, int& width, int& height) {
	const int side = font.atlasSize(glyph.page, AtlasKind::color);
	const std::span<const std::uint8_t> page = font.atlasPixels(glyph.page, AtlasKind::color);
	const auto texel = [side](float uv) { return static_cast<int>(std::lround(uv * static_cast<float>(side))); };
	const int left = texel(glyph.uv.left);
	const int top = texel(glyph.uv.top);
	width = texel(glyph.uv.right) - left;
	height = texel(glyph.uv.bottom) - top;
	const auto stride = static_cast<std::size_t>(side);
	if(width <= 0 || height <= 0 || left < 0 || top < 0 || left + width > side || top + height > side ||
	   page.size() < stride * stride * 4)
		return {};
	std::vector<std::uint8_t> rgba(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4);
	std::size_t to = 0;
	for(int y = top; y < top + height; ++y)
		for(int x = left; x < left + width; ++x, to += 4) {
			const std::size_t from = (static_cast<std::size_t>(y) * stride + static_cast<std::size_t>(x)) * 4;
			const int alpha = page[from + 3];
			for(std::size_t channel = 0; channel < 3; ++channel) // the atlas is premultiplied, images aren't
				rgba[to + channel] =
				    alpha == 0 ? 0 : static_cast<std::uint8_t>(std::min(255, page[from + channel] * 255 / alpha));
			rgba[to + 3] = static_cast<std::uint8_t>(alpha);
		}
	return rgba;
}

} // namespace

PdfPainter::PdfPainter(std::string& content, PdfResources& resources, std::size_t limit)
    : content_(content), resources_(resources), limit_(limit) {}

void PdfPainter::flushClip() {
	if(!pendingClip_) return;
	appendRect(content_, *pendingClip_);
	content_ += "W n\n";
	pendingClip_.reset();
}

void PdfPainter::checkLimit() const {
	if(content_.size() > limit_)
		throw std::runtime_error("PDF page content larger than its limit (" + std::to_string(limit_) + " bytes)");
}

void PdfPainter::setColor(Color color) {
	if(const std::string state = resources_.alpha(color.a); !state.empty()) content_ += "/" + state + " gs ";
	for(const float channel : {color.r, color.g, color.b}) appendNumber(content_, std::clamp(channel, 0.0f, 1.0f));
	content_ += "rg ";
	for(const float channel : {color.r, color.g, color.b}) appendNumber(content_, std::clamp(channel, 0.0f, 1.0f));
	content_ += "RG\n";
}

void PdfPainter::fill(const std::string& path, Color color, std::string_view op) {
	if(color.a <= 0.0f || path.empty()) return;
	content_ += "q ";
	setColor(color);
	content_ += path;
	content_ += op;
	content_ += " Q\n";
	checkLimit();
}

void PdfPainter::fillRect(const Rect& rect, Color color) {
	flushClip();
	std::string path;
	appendRect(path, rect);
	fill(path, color, "f");
}

void PdfPainter::fillRoundedRect(const Rect& rect, const CornerRadii& radii, Color color) {
	flushClip();
	std::string path;
	appendRoundedRect(path, rect, radii);
	fill(path, color, "f");
}

void PdfPainter::strokeRoundedRect(const Rect& rect, const CornerRadii& radii, float width, Color color) {
	flushClip();
	std::string path;
	appendRoundedRect(path, rect, radii);
	const Rect inner = grown(rect, -width);
	if(inner.width() > 0.0f && inner.height() > 0.0f) appendRoundedRect(path, inner, insetRadii(radii, width));
	fill(path, color, "f*"); // even-odd: the ring between them
}

void PdfPainter::drawShadow(const Rect& rect, const CornerRadii& radii, const Shadow& shadow) {
	if(!shadow.isVisible()) return;
	const Rect moved{rect.left + shadow.offset.x, rect.top + shadow.offset.y, rect.right + shadow.offset.x,
	                 rect.bottom + shadow.offset.y};
	const int layers = shadow.blur > 0.0f ? shadowLayers : 1;
	Color layer = shadow.color;
	// n layers of alpha a' cover their common area with 1 - (1 - a')^n = a
	layer.a = 1.0f - std::pow(1.0f - std::min(shadow.color.a, 0.999f), 1.0f / static_cast<float>(layers));
	for(int i = 0; i < layers; ++i) {
		const float by = shadow.blur * ((static_cast<float>(i) + 0.5f) / static_cast<float>(layers) - 0.5f);
		const Rect shape = grown(moved, by);
		if(shape.width() <= 0.0f || shape.height() <= 0.0f) continue;
		const auto grow = [by](float radius) { return std::max(0.0f, radius + by); };
		std::string path;
		appendRoundedRect(path, shape,
		                  {grow(radii.topLeft), grow(radii.topRight), grow(radii.bottomRight), grow(radii.bottomLeft)});
		fill(path, layer, "f");
	}
}

void PdfPainter::fillMesh(std::span<const MeshVertex> quads, Vec2 offset, Color color) {
	flushClip();
	std::string path;
	if(appendMeshOutline(path, quads, offset)) fill(path, color, "f");
}

void PdfPainter::placeImage(const std::string& name, const Rect& rect) {
	content_ += "q ";
	// The image's first row is at the top of its unit square: flip it, as the page's y points down
	appendMatrix(content_, {rect.width(), 0.0f, 0.0f, -rect.height(), rect.left, rect.bottom});
	content_ += "cm /" + name + " Do Q\n";
	checkLimit();
}

void PdfPainter::drawImage(const Bitmap& bitmap, const Rect& rect) {
	flushClip();
	placeImage(resources_.image({nullptr, bitmap.id()}, bitmap.width(), bitmap.height(), bitmap.pixels()), rect);
}

void PdfPainter::drawText(const Font& font, std::u32string_view text, Vec2 origin, Color color) {
	flushClip();
	if(color.a <= 0.0f || text.empty()) return;
	const Font::ShapedText& shaped = font.shape(text);
	content_ += "q ";
	setColor(color);
	drawOutlineGlyphs(font, shaped, origin);
	drawColorGlyphs(font, shaped, origin);
	content_ += "Q\n";
	checkLimit();
}

void PdfPainter::drawOutlineGlyphs(const Font& font, const Font::ShapedText& shaped, Vec2 origin) {
	const std::vector<Font::FaceSource> faces = font.faceSources();
	const FontStyle style = font.style();
	std::optional<std::size_t> current;
	content_ += "BT\n";
	for(const Font::ShapedGlyph& placed : shaped.glyphs) {
		const std::optional<Font::GlyphSource> source = font.glyphSource(*placed.glyph);
		if(!source || source->face >= faces.size() || faces[source->face].color) continue;
		const std::size_t face = resources_.fonts().face(faces[source->face].bytes);
		resources_.fonts().use(face, source->index);
		if(current != face) {
			const float size = faces[source->face].scale * static_cast<float>(resources_.fonts().unitsPerEm(face));
			content_ += "/" + resources_.fonts().name(face) + " ";
			appendNumber(content_, size);
			content_ += "Tf ";
			if(style.bold) { // the outline stroked as wide as the rasteriser thickens it
				appendNumber(content_, size / 24.0f);
				content_ += "w 2 Tr ";
			}
			current = face;
		}
		// Glyph space points up, the page's y down: flip it back, and slant it for italic
		appendMatrix(content_,
		             {1.0f, 0.0f, style.italic ? italicSlant : 0.0f, -1.0f, origin.x + placed.x, origin.y + placed.y});
		std::array<char, 16> glyph{};
		std::snprintf(glyph.data(), glyph.size(), "Tm <%04X> Tj\n", source->index & 0xFFFFu);
		content_ += glyph.data();
	}
	content_ += "ET\n";
}

void PdfPainter::drawColorGlyphs(const Font& font, const Font::ShapedText& shaped, Vec2 origin) {
	for(const Font::ShapedGlyph& placed : shaped.glyphs) {
		const Font::Glyph& glyph = *placed.glyph;
		if(!glyph.color) continue;
		int width = 0;
		int height = 0;
		const std::vector<std::uint8_t> rgba = cutGlyph(font, glyph, width, height);
		if(rgba.empty()) continue;
		const Rect where{origin.x + placed.x + glyph.quad.left, origin.y + placed.y + glyph.quad.top,
		                 origin.x + placed.x + glyph.quad.right, origin.y + placed.y + glyph.quad.bottom};
		placeImage(resources_.image({&glyph, 0}, width, height, rgba), where);
	}
}

} // namespace affresco::detail
