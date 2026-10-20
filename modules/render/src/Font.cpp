/// @file
/// @brief Font: loading, Latin-1 at load, glyphs on demand, measuring and wrapping, the atlas. Shaping is in
/// text/Shaping.cpp.

#include "affresco/render/Font.hpp"
#include "affresco/core/Log.hpp"

#include "text/FontData.hpp"
#include "text/FontInspector.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>

namespace affresco {

namespace {

constexpr int smallestPage = 256; ///< The first page starts at this size and doubles until Latin-1 fits.

/// @brief Copies a rendered glyph's rows into an atlas page; colour ones from BGRA to RGBA.
/// @param bitmap The glyph.
/// @param page The page.
/// @param left Its first column there.
/// @param top Its first row there.
void copyInto(const FT_Bitmap& bitmap, detail::GlyphAtlas::Page& page, int left, int top) noexcept {
	const auto stride = static_cast<std::size_t>(page.size) * static_cast<std::size_t>(page.bytesPerTexel);
	for(unsigned row = 0; row < bitmap.rows; ++row) {
		const unsigned char* from =
		    bitmap.buffer + static_cast<std::size_t>(row) * static_cast<std::size_t>(bitmap.pitch);
		std::uint8_t* to = page.pixels.data() + (static_cast<std::size_t>(top) + row) * stride +
		                   static_cast<std::size_t>(left) * static_cast<std::size_t>(page.bytesPerTexel);
		if(page.bytesPerTexel == 1) {
			std::memcpy(to, from, bitmap.width);
			continue;
		}
		for(unsigned column = 0; column < bitmap.width; ++column, from += 4, to += 4) {
			to[0] = from[2];
			to[1] = from[1];
			to[2] = from[0];
			to[3] = from[3];
		}
	}
}

/// @brief Where one line of a paragraph ends, and where the next one starts.
struct LineBreak {
	std::size_t end;  ///< One past the line's last code point.
	std::size_t next; ///< The next line's first code point: past a space or a '\n' that ends this one.
};

/// @brief Breaks the line that starts at @p start: as many code points as fit @p maxWidth (at least one), cut back to
/// the last space when the paragraph goes on.
/// @param text The whole text.
/// @param advances The paragraph's advances, one per code point.
/// @param paragraphStart Where the paragraph starts in @p text.
/// @param paragraphEnd Where it ends: at a '\n', or at the end of @p text.
/// @param start Where the line starts.
/// @param maxWidth How wide it may be.
/// @return Its end, and where the next line starts.
LineBreak breakLine(std::u32string_view text, std::span<const float> advances, std::size_t paragraphStart,
                    std::size_t paragraphEnd, std::size_t start, float maxWidth) noexcept {
	std::size_t end = start;       // end of the line being built
	std::size_t lastSpace = start; // last space seen on it (start = none)
	float width = 0.0f;
	while(end < paragraphEnd) {
		const float advance = advances[end - paragraphStart];
		// A code point with no width of its own continues its cluster (a mark, the rest of a ligature): never cut
		if(width + advance > maxWidth && end > start && advance > 0.0f) break;
		if(text[end] == U' ') lastSpace = end;
		width += advance;
		++end;
	}

	if(end == paragraphEnd && end < text.size()) return {end, end + 1};            // past the '\n'
	if(end < paragraphEnd && lastSpace > start) return {lastSpace, lastSpace + 1}; // the space itself is dropped
	return {end, end};
}

} // namespace

/* Font::Data: faces and glyphs */

std::optional<std::size_t> Font::Data::faceFor(char32_t codepoint) const noexcept {
	for(std::size_t face = 0; face < faces.size(); ++face)
		if(faces[face].glyphIndex(codepoint) != 0) return face;
	return std::nullopt;
}

std::optional<std::size_t> Font::Data::colorFaceFor(char32_t codepoint) const noexcept {
	for(std::size_t face = 0; face < faces.size(); ++face)
		if(faces[face].hasColor() && faces[face].glyphIndex(codepoint) != 0) return face;
	return std::nullopt;
}

const Font::Glyph& Font::Data::glyph(std::size_t face, unsigned index) {
	const std::uint64_t key = (static_cast<std::uint64_t>(face) << 32) | index;
	if(const auto found = glyphs.find(key); found != glyphs.end()) return found->second;
	const std::optional<Glyph> added = rasterise(face, index, true);
	return glyphs.emplace(key, added ? *added : *question).first->second;
}

std::optional<Font::Glyph> Font::Data::rasterise(std::size_t face, unsigned index, bool mayAddPage) {
	const detail::FontFace& from = faces[face];
	Glyph glyph{Rect{}, Rect{}, from.advance(index), 0, false};

	// A glyph that can't be rendered (a broken outline) draws nothing, and still moves the pen
	const auto bitmap = from.render(index);
	if(!bitmap || bitmap->pixels->width == 0 || bitmap->pixels->rows == 0 || bitmap->pixels->pitch < 0)
		return glyph; // spaces have no texels
	const auto width = static_cast<int>(bitmap->pixels->width), height = static_cast<int>(bitmap->pixels->rows);

	const AtlasKind kind = bitmap->color ? AtlasKind::color : AtlasKind::coverage;
	constexpr int padding = detail::GlyphAtlas::padding;
	const auto spot = atlas.place(kind, width + padding, height + padding, mayAddPage);
	if(!spot) return std::nullopt;

	detail::GlyphAtlas::Page& page = atlas.pages(kind)[spot->page];
	const int left = spot->x + padding, top = spot->y + padding;
	copyInto(*bitmap->pixels, page, left, top);
	page.changes.push_back({left, top, left + width, top + height});

	const auto texel = 1.0f / static_cast<float>(page.size);
	glyph.quad = Rect{static_cast<float>(bitmap->left), static_cast<float>(-bitmap->top),
	                  static_cast<float>(bitmap->left + width), static_cast<float>(height - bitmap->top)};
	glyph.uv = Rect{static_cast<float>(left) * texel, static_cast<float>(top) * texel,
	                static_cast<float>(left + width) * texel, static_cast<float>(top + height) * texel};
	glyph.page = spot->page;
	glyph.color = bitmap->color;
	return glyph;
}

/* Loading */

Font::Font() : data_(std::make_unique<Data>()) {}

Font::~Font() = default;

std::shared_ptr<const Font> Font::load(const std::filesystem::path& path, float pixelHeight,
                                       std::span<const std::filesystem::path> fallbacks) {
	detail::FontFiles files; // the faces keep their files; nothing else shares them
	return load(path, pixelHeight, fallbacks, {}, files);
}

std::shared_ptr<const Font> Font::load(const std::filesystem::path& path, float pixelHeight,
                                       std::span<const std::filesystem::path> fallbacks,
                                       std::span<const std::filesystem::path> optional, detail::FontFiles& files) {
	static std::atomic<std::uint64_t> nextId = 1;

	std::shared_ptr<Font> font(new Font());
	Data& data = *font->data_;
	data.library = detail::startFreeType();
	data.faces.reserve(1 + fallbacks.size() + optional.size());
	using SizeOf = detail::FontFace::SizeOf;
	data.faces.emplace_back(data.library.get(), files.read(path), pixelHeight, SizeOf::height, path);
	const float em = data.faces.front().emPixels(); // fallbacks get the font's em, so their text looks as large
	for(const std::filesystem::path& fallback : fallbacks)
		data.faces.emplace_back(data.library.get(), files.read(fallback), em, SizeOf::em, fallback);
	for(const std::filesystem::path& extra : optional) {
		try {
			data.faces.emplace_back(data.library.get(), files.read(extra), em, SizeOf::em, extra);
		} catch(const std::runtime_error& error) {
			log(LogLevel::Warning, std::string("AffrescoUI: skipping a system font: ") + error.what());
		}
	}
	data.buffer.reset(hb_buffer_create());
	if(!hb_buffer_allocation_successful(data.buffer.get())) throw std::bad_alloc();

	font->id_ = nextId++;
	font->pixelHeight_ = pixelHeight;
	font->ascent_ = data.faces.front().ascent();
	font->lineHeight_ = data.faces.front().lineHeight();

	// Smallest square page (power of two) that fits Latin-1
	for(int size = smallestPage; size <= detail::GlyphAtlas::largestPage; size *= 2)
		if(font->bakeLatin1(size)) return font;

	throw std::runtime_error("AffrescoUI: font too large to fit an atlas: " + path.string());
}

bool Font::bakeLatin1(int size) {
	Data& data = *data_;
	data.atlas = detail::GlyphAtlas(size, static_cast<int>(std::ceil(pixelHeight_)) + detail::GlyphAtlas::padding);
	data.glyphs.clear();

	// '?' first: what the rest falls back to. If no face has it either, the font's missing-glyph box
	constexpr std::size_t question = U'?' - firstCodepoint;
	std::array<bool, latin1Count> found{};
	for(std::size_t i = 0; i < latin1Count; ++i) {
		const char32_t codepoint = firstCodepoint + static_cast<char32_t>(i);
		const auto face = data.faceFor(codepoint);
		if(!face && i != question) continue;
		const std::size_t from = face.value_or(0);
		const unsigned index = data.faces[from].glyphIndex(codepoint);
		const std::uint64_t key = (static_cast<std::uint64_t>(from) << 32) | index;
		if(const auto same = data.glyphs.find(key); same != data.glyphs.end()) {
			latin1_[i] = same->second;
		} else {
			const std::optional<Glyph> glyph = data.rasterise(from, index, false);
			if(!glyph) return false; // the page is too small
			latin1_[i] = data.glyphs.emplace(key, *glyph).first->second;
		}
		found[i] = true;
	}
	for(std::size_t i = 0; i < latin1Count; ++i)
		if(!found[i]) latin1_[i] = latin1_[question];

	data.question = &latin1_[question];
	data.spaceAdvance = latin1_[U' ' - firstCodepoint].advance;
	data.atlas.forgetChanges(); // Latin-1 is the first page's content: nothing to upload twice
	return true;
}

/* Glyphs */

// Small enough to inline: Latin-1 never leaves this path
const Font::Glyph& Font::glyph(char32_t codepoint) const noexcept {
	const char32_t index = codepoint - firstCodepoint; // control characters wrap around to large values
	if(index < latin1Count) [[likely]]
		return latin1_[index];
	return glyphBeyondLatin1(codepoint);
}

const Font::Glyph& Font::glyphBeyondLatin1(char32_t codepoint) const noexcept {
	Data& data = *data_;
	if(codepoint < firstCodepoint) return *data.question; // control characters

	if(const auto found = data.nominal.find(codepoint); found != data.nominal.end()) return *found->second;

	try {
		const auto face = data.faceFor(codepoint);
		const Glyph* glyph = face ? &data.glyph(*face, data.faces[*face].glyphIndex(codepoint)) : data.question;
		data.nominal.emplace(codepoint, glyph);
		return *glyph;
	} catch(...) {
		return *data.question; // out of memory: draw '?' rather than end the program
	}
}

bool Font::hasGlyph(char32_t codepoint) const noexcept {
	return codepoint >= firstCodepoint && data_->faceFor(codepoint).has_value();
}

/* Shaped text */

const Font::ShapedText& Font::shape(std::u32string_view text) const {
	try {
		return data_->shape(text);
	} catch(const std::bad_alloc&) {
		data_->uncached = {};
		return data_->uncached;
	}
}

float Font::measure(std::u32string_view text) const noexcept {
	try {
		return data_->measure(text).width;
	} catch(...) {
		data_->measured = {};
		return 0.0f;
	}
}

std::vector<std::u32string_view> Font::wrap(std::u32string_view text, float maxWidth) const {
	std::vector<std::u32string_view> lines;
	lines.reserve(4); // most texts wrap into a few lines: one allocation, rather than one each time the vector grows

	// Each paragraph (up to a '\n') is measured once; its lines are views into it, broken by its advances
	const std::vector<float>* paragraph = nullptr;
	std::size_t paragraphStart = 0, paragraphEnd = 0;
	std::size_t start = 0; // where the line being built starts
	while(start < text.size()) {
		if(!paragraph || start > paragraphEnd) {
			paragraphStart = start;
			paragraphEnd = std::min(text.find(U'\n', start), text.size());
			paragraph = &data_->measure(text.substr(start, paragraphEnd - start)).advances;
		}

		const auto [end, next] = breakLine(text, *paragraph, paragraphStart, paragraphEnd, start, maxWidth);
		lines.push_back(text.substr(start, end - start));
		start = next;
	}

	return lines;
}

std::size_t Font::memoryBytes() const noexcept {
	const Data& data = *data_;
	constexpr std::size_t node = 32; // a hash map node's pointers and hash, roughly
	return sizeof(Font) + data.atlas.bytes() + data.glyphs.size() * (sizeof(Glyph) + node) +
	       data.nominal.size() * (sizeof(const Glyph*) + node) + data.words.bytes() + data.lines.bytes() +
	       data.uncached.glyphs.capacity() * sizeof(ShapedGlyph) + (data.plain ? data.plain->bytes() : 0);
}

/* What tests see */

void detail::FontInspector::setPlainTextEnabled(const Font& font, bool enabled) {
	font.data_->plainText().setEnabled(enabled);
}

std::size_t detail::FontInspector::plainCharacters(const Font& font) {
	return font.data_->plainText().plainCharacters();
}

bool detail::FontInspector::isPlain(const Font& font, std::u32string_view word) {
	std::vector<float> advances(word.size());
	float pen = 0.0f;
	return font.data_->plainText().layOut(word, advances, 0.0f, pen, font.data_->buffer.get()) == word.size();
}

/* Atlas */

std::size_t Font::atlasPages(AtlasKind kind) const noexcept { return data_->atlas.pages(kind).size(); }

int Font::atlasSize(std::size_t page, AtlasKind kind) const noexcept { return data_->atlas.pages(kind)[page].size; }

std::span<const std::uint8_t> Font::atlasPixels(std::size_t page, AtlasKind kind) const noexcept {
	return data_->atlas.pages(kind)[page].pixels;
}

std::uint64_t Font::atlasVersion(std::size_t page, AtlasKind kind) const noexcept {
	return data_->atlas.pages(kind)[page].changes.size();
}

Font::AtlasRegion Font::atlasChangesSince(std::size_t page, std::uint64_t version, AtlasKind kind) const noexcept {
	const std::vector<AtlasRegion>& changes = data_->atlas.pages(kind)[page].changes;
	AtlasRegion all;
	for(std::size_t i = static_cast<std::size_t>(version); i < changes.size(); ++i) {
		const AtlasRegion& change = changes[i];
		all = all.empty() ? change
		                  : AtlasRegion{std::min(all.left, change.left), std::min(all.top, change.top),
		                                std::max(all.right, change.right), std::max(all.bottom, change.bottom)};
	}
	return all;
}

Vec2 Font::whiteTexel(std::size_t page) const noexcept {
	// The shared corner of the 2x2 white block: bilinear filtering there reads pure white
	const auto texel = 1.0f / static_cast<float>(data_->atlas.pages(AtlasKind::coverage)[page].size);
	return {texel, texel};
}

} // namespace affresco
