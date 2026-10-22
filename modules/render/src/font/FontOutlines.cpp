/// @file
/// @brief Font::faceSources() and Font::glyphSource(): the files and glyph indices behind a font's glyphs, for vector
/// output such as a PDF that embeds the fonts.

#include "affresco/render/Font.hpp"

#include "font/FontData.hpp"

namespace affresco {

std::vector<Font::FaceSource> Font::faceSources() const {
	std::vector<FaceSource> sources;
	sources.reserve(data_->faces.size());
	for(const detail::FontFace& face : data_->faces) sources.push_back({face.bytes(), face.scale(), face.hasColor()});
	return sources;
}

std::optional<Font::GlyphSource> Font::glyphSource(const Glyph& glyph) const noexcept {
	const Glyph* const address = &glyph;
	// Latin-1 holds copies: find the face the way the font chose it when it loaded
	if(address >= latin1_.data() && address < latin1_.data() + latin1_.size()) {
		const auto codepoint = static_cast<char32_t>(firstCodepoint + static_cast<char32_t>(address - latin1_.data()));
		const std::optional<std::size_t> face = data_->faceFor(codepoint);
		const char32_t drawn = face ? codepoint : U'?';
		const std::size_t from = face.value_or(data_->faceFor(U'?').value_or(0));
		return GlyphSource{from, data_->faces[from].glyphIndex(drawn)};
	}
	for(const auto& [key, stored] : data_->glyphs)
		if(&stored == address)
			return GlyphSource{static_cast<std::size_t>(key >> 32), static_cast<unsigned>(key & 0xFFFFFFFFu)};
	return std::nullopt;
}

} // namespace affresco
