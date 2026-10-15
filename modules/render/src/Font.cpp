#include "sgl/render/Font.hpp"

#include <stb_truetype.h>

#include <algorithm>
#include <atomic>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace sgl {

namespace {

constexpr int whiteRows = 2; ///< Rows reserved at the top of the atlas for the 2x2 white block.

/// @brief Reads a whole file.
/// @param path The file.
/// @return Its bytes.
/// @throws std::runtime_error If it can't be opened.
std::vector<unsigned char> readFile(const std::filesystem::path& path) {
	std::ifstream file(path, std::ios::binary);
	if(!file) throw std::runtime_error("sGLGUI: could not open font file " + path.string());
	return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

} // namespace

std::shared_ptr<const Font> Font::load(const std::filesystem::path& path, float pixelHeight) {
	static std::atomic<std::uint64_t> nextId = 1;

	const std::vector<unsigned char> data = readFile(path);

	stbtt_fontinfo info;
	if(!stbtt_InitFont(&info, data.data(), stbtt_GetFontOffsetForIndex(data.data(), 0)))
		throw std::runtime_error("sGLGUI: not a usable TrueType font: " + path.string());

	std::shared_ptr<Font> font(new Font());
	font->id_ = nextId++;
	font->pixelHeight_ = pixelHeight;

	int ascent = 0, descent = 0, lineGap = 0;
	stbtt_GetFontVMetrics(&info, &ascent, &descent, &lineGap);
	const float scale = stbtt_ScaleForPixelHeight(&info, pixelHeight);
	font->ascent_ = static_cast<float>(ascent) * scale;
	font->lineHeight_ = static_cast<float>(ascent - descent + lineGap) * scale;

	// Smallest square atlas (power of two) that fits the whole range
	constexpr int glyphCount = static_cast<int>(lastCodepoint - firstCodepoint + 1);
	std::vector<stbtt_packedchar> packed(glyphCount);

	for(int size = 256; size <= 4096; size *= 2) {
		std::vector<std::uint8_t> atlas(static_cast<std::size_t>(size) * static_cast<std::size_t>(size), 0);

		// Glyphs are packed below the reserved rows
		stbtt_pack_context context;
		if(!stbtt_PackBegin(&context, atlas.data() + whiteRows * size, size, size - whiteRows, size, 1, nullptr))
			continue;
		const int packedOk = stbtt_PackFontRange(&context, data.data(), 0, pixelHeight,
		                                         static_cast<int>(firstCodepoint), glyphCount, packed.data());
		stbtt_PackEnd(&context);
		if(!packedOk) continue;

		atlas[0] = atlas[1] = atlas[static_cast<std::size_t>(size)] = atlas[static_cast<std::size_t>(size) + 1] = 255;

		const auto texel = 1.0f / static_cast<float>(size);
		font->glyphs_.reserve(packed.size());
		for(const stbtt_packedchar& p : packed) {
			font->glyphs_.push_back(Glyph{
			    Rect{p.xoff, p.yoff, p.xoff2, p.yoff2},
			    Rect{static_cast<float>(p.x0) * texel, static_cast<float>(p.y0 + whiteRows) * texel,
				     static_cast<float>(p.x1) * texel, static_cast<float>(p.y1 + whiteRows) * texel},
			    p.xadvance,
			});
		}

		font->atlasSize_ = size;
		font->atlas_ = std::move(atlas);
		return font;
	}

	throw std::runtime_error("sGLGUI: font too large to fit an atlas: " + path.string());
}

const Font::Glyph& Font::glyph(char32_t codepoint) const noexcept {
	if(codepoint < firstCodepoint || codepoint > lastCodepoint) codepoint = U'?';
	return glyphs_[codepoint - firstCodepoint];
}

float Font::measure(std::u32string_view text) const noexcept {
	float width = 0.0f;
	for(char32_t cp : text) width += glyph(cp).advance;
	return width;
}

std::vector<std::u32string_view> Font::wrap(std::u32string_view text, float maxWidth) const {
	std::vector<std::u32string_view> lines;

	while(!text.empty()) {
		std::size_t end = 0;       // end of the line being built
		std::size_t lastSpace = 0; // last space seen on it (0 = none)
		float width = 0.0f;

		while(end < text.size() && text[end] != U'\n') {
			const float advance = glyph(text[end]).advance;
			if(width + advance > maxWidth && end > 0) break;
			if(text[end] == U' ') lastSpace = end;
			width += advance;
			++end;
		}

		std::size_t next = end; // where the following line starts
		if(end < text.size() && text[end] == U'\n') {
			next = end + 1;
		} else if(end < text.size() && lastSpace > 0) {
			end = lastSpace; // break at the last space; the space itself is dropped
			next = lastSpace + 1;
		}

		lines.push_back(text.substr(0, end));
		text.remove_prefix(next);
	}

	return lines;
}

Vec2 Font::whiteTexel() const noexcept {
	// The shared corner of the 2x2 white block: bilinear filtering there reads pure white
	const auto texel = 1.0f / static_cast<float>(atlasSize_);
	return {texel, texel};
}

} // namespace sgl
