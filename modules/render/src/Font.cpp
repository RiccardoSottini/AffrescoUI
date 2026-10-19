#include "affresco/render/Font.hpp"

#include <stb_truetype.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <fstream>
#include <iterator>
#include <new>
#include <optional>
#include <stdexcept>
#include <unordered_map>

namespace affresco {

namespace {

constexpr int whiteRows = 2;            ///< Rows reserved at the top of each page for the 2x2 white block.
constexpr int padding = 1;              ///< Empty texels left and above each glyph, as stb_truetype's packer leaves.
constexpr int smallestPage = 256;       ///< The first page starts at this size and doubles until Latin-1 fits.
constexpr int largestPage = 4096;       ///< No page is larger.
constexpr int smallestAddedPage = 1024; ///< Pages added later are at least this large, so they fill slowly.
/// @brief The most texels the pages of one font may hold together: text naming many characters (e.g. from a peer)
/// can't grow the atlas without bound. Characters that don't fit draw as '?'.
constexpr std::size_t atlasLimit = std::size_t{64} << 20;
constexpr char32_t lastUnicode = 0x10FFFF; ///< The last code point Unicode has; anything above draws as '?'.

/// @brief Reads a whole file.
/// @param path The file.
/// @return Its bytes.
/// @throws std::runtime_error If it can't be opened.
std::vector<unsigned char> readFile(const std::filesystem::path& path) {
	std::ifstream file(path, std::ios::binary);
	if(!file) throw std::runtime_error("AffrescoUI: could not open font file " + path.string());
	return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

/// @brief Where a glyph's texels go.
struct Spot {
	std::uint32_t page; ///< The page.
	int x, y;           ///< Its top-left texel, padding included.
};

/// @brief One TrueType file and the scale that draws it at the font's pixel height.
struct Face {
	std::vector<unsigned char> data; ///< The file; `info` points into it, so it is never resized.
	stbtt_fontinfo info{};           ///< stb_truetype's view of it.
	float scale = 0.0f;              ///< Font units to pixels.
};

/// @brief One square page of the atlas, filled in shelves: rows of glyphs, each as tall as its tallest.
struct Page {
	/// @brief A row of glyphs.
	struct Shelf {
		int top;    ///< Its first row.
		int height; ///< Its rows.
		int right;  ///< The first free column.
	};

	int size = 0;                           ///< Width and height, in texels.
	std::vector<std::uint8_t> pixels;       ///< Coverage, top row first.
	std::vector<Font::AtlasRegion> changes; ///< One per glyph added after the page was made.
	std::vector<Shelf> shelves;             ///< Rows that glyphs are added to.
	int freeTop = whiteRows;                ///< The first row below every shelf.

	/// @brief Makes an empty page, with its white block.
	/// @param pageSize Width and height, in texels.
	explicit Page(int pageSize)
	    : size(pageSize), pixels(static_cast<std::size_t>(pageSize) * static_cast<std::size_t>(pageSize), 0) {
		pixels[0] = pixels[1] = pixels[static_cast<std::size_t>(size)] = pixels[static_cast<std::size_t>(size) + 1] =
		    255;
	}

	/// @brief Finds room for a box, on a shelf or on a new one.
	/// @param width Its width, padding included.
	/// @param height Its height, padding included.
	/// @param shelfHeight The height a new shelf gets, if the box is not taller.
	/// @return Its top-left texel, or nothing if the page is full.
	std::optional<std::array<int, 2>> place(int width, int height, int shelfHeight) {
		if(width > size) return std::nullopt;

		// The shortest shelf the box fits on wastes the fewest rows
		Shelf* best = nullptr;
		for(Shelf& shelf : shelves)
			if(shelf.height >= height && shelf.right + width <= size && (!best || shelf.height < best->height))
				best = &shelf;

		if(!best) {
			const int rows = std::max(height, shelfHeight);
			if(freeTop + rows > size) return std::nullopt;
			best = &shelves.emplace_back(Shelf{freeTop, rows, 0});
			freeTop += rows;
		}

		const std::array<int, 2> at{best->right, best->top};
		best->right += width;
		return at;
	}
};

/// @brief Reads a TrueType file.
/// @param path The file.
/// @param pixelHeight The size its glyphs are drawn at.
/// @return The face.
/// @throws std::runtime_error If it can't be read or isn't a usable font.
Face loadFace(const std::filesystem::path& path, float pixelHeight) {
	Face face;
	face.data = readFile(path);
	const int offset = face.data.empty() ? -1 : stbtt_GetFontOffsetForIndex(face.data.data(), 0);
	if(offset < 0 || !stbtt_InitFont(&face.info, face.data.data(), offset))
		throw std::runtime_error("AffrescoUI: not a usable TrueType font: " + path.string());
	face.scale = stbtt_ScaleForPixelHeight(&face.info, pixelHeight);
	return face;
}

} // namespace

/// @brief Everything that grows as glyphs are looked up.
struct Font::Glyphs {
	std::vector<Face> faces;                   ///< The font first, then its fallbacks.
	std::vector<Page> pages;                   ///< The first holds Latin-1.
	std::unordered_map<char32_t, Glyph> added; ///< Rasterised on demand. Its nodes never move.
	std::size_t atlasTexels = 0;               ///< Texels in all pages: see atlasLimit.
	int shelfHeight = 0;                       ///< Rows a new shelf gets: one line of glyphs.

	/// @brief Finds the first face that has a glyph for a character.
	/// @param codepoint The character.
	/// @return The face and its glyph index, or nothing.
	[[nodiscard]] std::optional<std::pair<const Face*, int>> find(char32_t codepoint) const noexcept {
		// Beyond Unicode, the cast to int would turn negative, and stb_truetype indexes some cmap tables with it
		if(codepoint > lastUnicode) return std::nullopt;
		for(const Face& face : faces)
			if(const int index = stbtt_FindGlyphIndex(&face.info, static_cast<int>(codepoint)); index != 0)
				return std::pair{&face, index};
		return std::nullopt;
	}

	/// @brief Finds room for a glyph in a page, adding a page if none has it.
	/// @param width Its width, padding included.
	/// @param height Its height, padding included.
	/// @return Where it goes, or nothing if the atlas is full.
	std::optional<Spot> place(int width, int height) {
		for(std::size_t p = 0; p < pages.size(); ++p)
			if(const auto at = pages[p].place(width, height, shelfHeight))
				return Spot{static_cast<std::uint32_t>(p), (*at)[0], (*at)[1]};

		const int size = std::max(pages.front().size, smallestAddedPage);
		if(width > size || height > size) return std::nullopt; // larger than a whole page
		const std::size_t texels = static_cast<std::size_t>(size) * static_cast<std::size_t>(size);
		if(atlasTexels + texels > atlasLimit) return std::nullopt;

		Page& page = pages.emplace_back(size);
		atlasTexels += texels;
		const auto at = page.place(width, height, shelfHeight);
		if(!at) return std::nullopt; // not reached: an empty page has room for what fits in one
		return Spot{static_cast<std::uint32_t>(pages.size() - 1), (*at)[0], (*at)[1]};
	}

	/// @brief Rasterises a character into the atlas and remembers it.
	/// @param codepoint The character.
	/// @param question The glyph remembered instead when the atlas is full, so it isn't searched again.
	/// @return Its glyph, or nullptr if no face has it.
	const Glyph* add(char32_t codepoint, const Glyph& question) {
		const auto found = find(codepoint);
		if(!found) return nullptr;
		const auto [face, index] = *found;

		// As stb_truetype's packer places glyphs, so they look the same as the ones rasterised at load
		int x0 = 0, y0 = 0, x1 = 0, y1 = 0, advance = 0, bearing = 0;
		stbtt_GetGlyphBitmapBox(&face->info, index, face->scale, face->scale, &x0, &y0, &x1, &y1);
		stbtt_GetGlyphHMetrics(&face->info, index, &advance, &bearing);
		const int width = x1 - x0, height = y1 - y0;

		Glyph glyph{
		    Rect{static_cast<float>(x0), static_cast<float>(y0), static_cast<float>(x1), static_cast<float>(y1)},
		    Rect{}, static_cast<float>(advance) * face->scale, 0};

		if(width > 0 && height > 0) { // spaces have no texels
			const auto spot = place(width + padding, height + padding);
			if(!spot) return &added.emplace(codepoint, question).first->second;

			Page& page = pages[spot->page];
			const int left = spot->x + padding, top = spot->y + padding;
			stbtt_MakeGlyphBitmap(&face->info,
			                      page.pixels.data() +
			                          static_cast<std::size_t>(top) * static_cast<std::size_t>(page.size) +
			                          static_cast<std::size_t>(left),
			                      width, height, page.size, face->scale, face->scale, index);
			page.changes.push_back({left, top, left + width, top + height});

			const auto texel = 1.0f / static_cast<float>(page.size);
			glyph.uv = Rect{static_cast<float>(left) * texel, static_cast<float>(top) * texel,
			                static_cast<float>(left + width) * texel, static_cast<float>(top + height) * texel};
			glyph.page = spot->page;
		}

		return &added.emplace(codepoint, glyph).first->second;
	}
};

Font::Font() : glyphs_(std::make_unique<Glyphs>()) {}

Font::~Font() = default;

std::shared_ptr<const Font> Font::load(const std::filesystem::path& path, float pixelHeight,
                                       std::span<const std::filesystem::path> fallbacks) {
	static std::atomic<std::uint64_t> nextId = 1;

	std::shared_ptr<Font> font(new Font());
	Glyphs& glyphs = *font->glyphs_;
	glyphs.faces.reserve(1 + fallbacks.size());
	glyphs.faces.push_back(loadFace(path, pixelHeight));
	for(const std::filesystem::path& fallback : fallbacks) glyphs.faces.push_back(loadFace(fallback, pixelHeight));
	const Face& main = glyphs.faces.front();

	font->id_ = nextId++;
	font->pixelHeight_ = pixelHeight;

	int ascent = 0, descent = 0, lineGap = 0;
	stbtt_GetFontVMetrics(&main.info, &ascent, &descent, &lineGap);
	font->ascent_ = static_cast<float>(ascent) * main.scale;
	font->lineHeight_ = static_cast<float>(ascent - descent + lineGap) * main.scale;
	glyphs.shelfHeight = static_cast<int>(std::ceil(pixelHeight)) + padding;

	// Smallest square page (power of two) that fits Latin-1
	for(int size = smallestPage; size <= largestPage; size *= 2)
		if(font->bakeLatin1(size)) return font;

	throw std::runtime_error("AffrescoUI: font too large to fit an atlas: " + path.string());
}

bool Font::bakeLatin1(int size) {
	Glyphs& glyphs = *glyphs_;
	const Face& main = glyphs.faces.front();
	Page page(size);
	std::array<stbtt_packedchar, latin1Count> packed{};

	// Glyphs are packed below the reserved rows
	stbtt_pack_context context;
	if(!stbtt_PackBegin(&context, page.pixels.data() + static_cast<std::ptrdiff_t>(whiteRows) * size, size,
	                    size - whiteRows, size, padding, nullptr))
		return false;
	const int packedOk =
	    stbtt_PackFontRange(&context, main.data.data(), 0, pixelHeight_, static_cast<int>(firstCodepoint),
		                    static_cast<int>(latin1Count), packed.data());
	stbtt_PackEnd(&context);
	if(!packedOk) return false;

	const auto texel = 1.0f / static_cast<float>(size);
	int bottom = whiteRows;
	for(std::size_t i = 0; i < latin1Count; ++i) {
		const stbtt_packedchar& p = packed[i];
		latin1_[i] = Glyph{
		    Rect{p.xoff, p.yoff, p.xoff2, p.yoff2},
		    Rect{static_cast<float>(p.x0) * texel, static_cast<float>(p.y0 + whiteRows) * texel,
			     static_cast<float>(p.x1) * texel, static_cast<float>(p.y1 + whiteRows) * texel},
		    p.xadvance,
		};
		bottom = std::max(bottom, p.y1 + whiteRows + padding);
	}

	page.freeTop = bottom; // later glyphs go below Latin-1
	glyphs.atlasTexels = page.pixels.size();
	glyphs.pages.push_back(std::move(page));

	// What the font lacks comes from a fallback now, or is '?', so the table is final and lookups never leave it.
	// '?' itself stays: it is the last resort (the font's missing-glyph box, if it lacks even that).
	const Glyph& question = latin1_[U'?' - firstCodepoint];
	for(std::size_t i = 0; i < latin1Count; ++i) {
		const char32_t codepoint = firstCodepoint + static_cast<char32_t>(i);
		if(codepoint == U'?' || stbtt_FindGlyphIndex(&main.info, static_cast<int>(codepoint)) != 0) continue;
		const Glyph* fallback = glyphs.add(codepoint, question);
		latin1_[i] = fallback ? *fallback : question;
	}
	return true;
}

// Small enough to inline into measure() and wrap(): Latin-1 text never leaves this path
const Font::Glyph& Font::glyph(char32_t codepoint) const noexcept {
	const char32_t index = codepoint - firstCodepoint; // control characters wrap around to large values
	if(index < latin1Count) [[likely]]
		return latin1_[index];
	return glyphBeyondLatin1(codepoint);
}

const Font::Glyph& Font::glyphBeyondLatin1(char32_t codepoint) const noexcept {
	Glyphs& glyphs = *glyphs_;
	const Glyph& question = latin1_[U'?' - firstCodepoint];
	if(codepoint < firstCodepoint) return question; // control characters

	if(const auto it = glyphs.added.find(codepoint); it != glyphs.added.end()) return it->second;

	try {
		const Glyph* added = glyphs.add(codepoint, question);
		return added ? *added : question;
	} catch(...) {
		return question; // out of memory, or the cache failed: draw '?' rather than end the program
	}
}

bool Font::hasGlyph(char32_t codepoint) const noexcept {
	return codepoint >= firstCodepoint && glyphs_->find(codepoint).has_value();
}

float Font::measure(std::u32string_view text) const noexcept {
	// Latin-1 first, without branches so the loop unrolls; text with anything else is measured again, glyph by glyph
	float width = 0.0f;
	bool beyondLatin1 = false;
	for(const char32_t cp : text) {
		const char32_t index = cp - firstCodepoint; // control characters wrap around to large values
		beyondLatin1 |= index >= latin1Count;
		width += latin1_[std::min<std::size_t>(index, latin1Count - 1)].advance;
	}
	if(!beyondLatin1) return width;

	width = 0.0f;
	for(const char32_t cp : text) width += glyph(cp).advance;
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

std::size_t Font::atlasPages() const noexcept { return glyphs_->pages.size(); }

int Font::atlasSize(std::size_t page) const noexcept { return glyphs_->pages[page].size; }

std::span<const std::uint8_t> Font::atlasPixels(std::size_t page) const noexcept { return glyphs_->pages[page].pixels; }

std::uint64_t Font::atlasVersion(std::size_t page) const noexcept { return glyphs_->pages[page].changes.size(); }

Font::AtlasRegion Font::atlasChangesSince(std::size_t page, std::uint64_t version) const noexcept {
	const std::vector<AtlasRegion>& changes = glyphs_->pages[page].changes;
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
	const auto texel = 1.0f / static_cast<float>(glyphs_->pages[page].size);
	return {texel, texel};
}

} // namespace affresco
