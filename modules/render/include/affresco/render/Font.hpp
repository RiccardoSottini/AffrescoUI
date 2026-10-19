#pragma once

/// @file
/// @brief Font: a font with fallbacks, shaped by HarfBuzz and rasterised by FreeType into glyph atlas pages on demand;
/// shaping, measuring and word wrap.

#include "affresco/render/Geometry.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace affresco {

class FontCache;

namespace detail {
class FontFiles;
}

/// @brief Which pages of a font's atlas: coverage (one byte per texel, tinted by the text's colour) or colour (four
/// bytes per texel, premultiplied RGBA, e.g. colour emoji).
enum class AtlasKind : std::uint8_t {
	coverage, ///< One byte of coverage per texel. Each page also holds a white block, for solid rectangles.
	color,    ///< Premultiplied red, green, blue and alpha per texel.
};

/// @brief A font file and its fallbacks at one pixel height: text shaped into glyphs and rasterised into atlas pages.
///
/// **Shaping.** Text is shaped with HarfBuzz: kerning, ligatures, combining marks, and the joining and reordering of
/// complex scripts (Arabic, Devanagari and the others HarfBuzz knows). Each character is drawn from the first of the
/// font and its fallbacks that has it (its coverage), and a run of one script and one font file is shaped at a time.
/// A run of a right-to-left script is laid out right to left, and the runs of a line follow each other left to right:
/// there is no bidirectional reordering yet. Shaped text is cached by its text: a line from its words (split at
/// spaces, so a new line of known words is quick), and lines as a whole.
///
/// **Atlas.** Glyphs are rasterised with FreeType the first time they are drawn, into a page with room (a new page when
/// they are all full). Latin-1 (U+0020..U+00FF) is rasterised when the font loads, into the first page. Colour glyphs
/// (fonts with a COLR table, such as colour emoji) go to colour pages. Glyphs from the fallbacks share the font's
/// pages, so text that mixes them still draws in one batch. The pages of one font hold at most 64 MiB together;
/// characters past that draw as '?', so text from a peer can't grow the atlas without bound.
///
/// Pure CPU data: each window uploads the pages to its own GPU context when it draws with them, then only what was
/// added since. Not thread-safe: a font is used on the main thread, like the panels that draw with it.
class Font : public std::enable_shared_from_this<Font> {
public:
	/// @brief Where a glyph is in the atlas and how to place it.
	struct Glyph {
		Rect quad;              ///< Relative to the pen position on the baseline (y pointing down).
		Rect uv;                ///< Texture coordinates in its atlas page.
		float advance;          ///< Horizontal pen movement, before shaping (kerning and the like).
		std::uint32_t page = 0; ///< The atlas page that holds it.
		bool color = false;     ///< Whether the page is a colour page (AtlasKind::color): its texels are the colours.
	};

	/// @brief A glyph of shaped text, placed.
	struct ShapedGlyph {
		const Glyph* glyph; ///< The glyph; valid as long as the font.
		float x;            ///< Where its pen position is, from the text's start along the baseline.
		float y;            ///< How far it is moved down from the baseline (e.g. a mark above a letter: negative).
	};

	/// @brief Text shaped into glyphs, with what measuring, wrapping and carets need.
	struct ShapedText {
		/// The glyphs that draw something, in drawing order: left to right. Spaces aren't in it.
		std::vector<ShapedGlyph> glyphs;
		/// Per code point, its share of the width: a cluster of several code points (a ligature, a letter and its
		/// marks) gives all of its width to its first one, so the sum of a line's is its width.
		std::vector<float> advances;
		/// Per code point and one more: where a caret before it goes, from the text's start (the last one is after
		/// the text). A ligature's carets are spread over its width. In a right-to-left run they go from right to left.
		std::vector<float> carets;
		float width = 0.0f; ///< How far the pen moves over the whole text.
	};

	/// @brief A rectangle of texels in an atlas page; right and bottom are exclusive.
	struct AtlasRegion {
		int left = 0;   ///< First column.
		int top = 0;    ///< First row.
		int right = 0;  ///< One past the last column.
		int bottom = 0; ///< One past the last row.

		/// @brief Whether it covers no texel.
		[[nodiscard]] bool empty() const noexcept { return right <= left || bottom <= top; }
	};

	/// @brief Loads a font and rasterises Latin-1.
	/// @param path A TrueType or OpenType file (a collection's first font).
	/// @param pixelHeight Size in pixels: from the top of the tallest glyph to the bottom of the deepest.
	/// @param fallbacks Font files asked, in order, for characters the font lacks. Their glyphs are drawn with the
	/// font's em size (so their text looks as large as the font's), on the font's baseline.
	/// @return The font. Its first page is the smallest square (256 to 4096 texels) that fits Latin-1.
	/// @throws std::runtime_error If a file can't be read, isn't a usable font, or Latin-1 doesn't fit.
	[[nodiscard]] static std::shared_ptr<const Font> load(const std::filesystem::path& path, float pixelHeight,
	                                                      std::span<const std::filesystem::path> fallbacks = {});

	/// @brief Frees the font's data and pages.
	~Font();
	Font(const Font&) = delete;            ///< Shared through std::shared_ptr instead.
	Font& operator=(const Font&) = delete; ///< Shared through std::shared_ptr instead.

	/// @brief The size it was rasterised at, in pixels.
	[[nodiscard]] float pixelHeight() const noexcept { return pixelHeight_; }
	/// @brief Distance from the baseline to the top of the tallest glyph.
	[[nodiscard]] float ascent() const noexcept { return ascent_; }
	/// @brief Distance from one baseline to the next.
	[[nodiscard]] float lineHeight() const noexcept { return lineHeight_; }

	/// @brief Looks the glyph of one character up, unshaped, rasterising it the first time.
	///
	/// The reference stays valid as long as the font. A lookup may add the glyph to an atlas page, so a renderer
	/// uploads what was added (atlasChangesSince()) before it draws.
	/// @param codepoint A Unicode code point.
	/// @return Its glyph from the font or the first fallback that has it; else, and for control characters (below
	/// U+0020) or when the atlas is full, the glyph of '?'.
	[[nodiscard]] const Glyph& glyph(char32_t codepoint) const noexcept;

	/// @brief Whether the font or one of its fallbacks has a glyph for a character.
	///
	/// It says what the files hold, not what glyph() draws: once the atlas is full, a character it has may still draw
	/// as '?'.
	/// @param codepoint A Unicode code point.
	[[nodiscard]] bool hasGlyph(char32_t codepoint) const noexcept;

	/// @brief Shapes one line of text, rasterising the glyphs it meets for the first time.
	///
	/// A character no file has draws as '?', as do control characters (a '\n' is not a line break here).
	/// @param text The text.
	/// @return The shaped text. The reference stays valid until the font shapes another text (this function,
	/// measure(), wrap()); copy what must last longer.
	[[nodiscard]] const ShapedText& shape(std::u32string_view text) const;

	/// @brief Measures one line of text, shaped.
	/// @param text The text.
	/// @return Its width in pixels; 0 if it couldn't be shaped (out of memory).
	[[nodiscard]] float measure(std::u32string_view text) const noexcept;

	/// @brief Splits text into lines: at '\n', else at the last space that fits, else inside a word (never inside a
	/// cluster: a ligature, or a letter and its marks). Widths are those of the shaped text.
	/// @param text The text.
	/// @param maxWidth Widest line, in pixels.
	/// @return Views into @p text, one per line.
	[[nodiscard]] std::vector<std::u32string_view> wrap(std::u32string_view text, float maxWidth) const;

	/// @brief How much memory the font's atlas pages, glyphs and shaped text take, without its font files.
	[[nodiscard]] std::size_t memoryBytes() const noexcept;

	/* Atlas */

	/// @brief Unique per loaded font, e.g. to cache its GPU textures.
	[[nodiscard]] std::uint64_t id() const noexcept { return id_; }
	/// @brief How many atlas pages there are of a kind. Never fewer than one coverage page; pages are only ever added.
	/// @param kind Coverage or colour pages.
	[[nodiscard]] std::size_t atlasPages(AtlasKind kind = AtlasKind::coverage) const noexcept;
	/// @brief A page's width and height, in texels.
	/// @param page The page, below atlasPages().
	/// @param kind Its kind.
	[[nodiscard]] int atlasSize(std::size_t page = 0, AtlasKind kind = AtlasKind::coverage) const noexcept;
	/// @brief A page's texels, top row first: one byte each for coverage, four (premultiplied RGBA) for colour.
	/// @param page The page, below atlasPages().
	/// @param kind Its kind.
	[[nodiscard]] std::span<const std::uint8_t> atlasPixels(std::size_t page = 0,
	                                                        AtlasKind kind = AtlasKind::coverage) const noexcept;
	/// @brief How many times glyphs were added to a page since it was made: compare it to know what to upload.
	/// @param page The page, below atlasPages().
	/// @param kind Its kind.
	[[nodiscard]] std::uint64_t atlasVersion(std::size_t page, AtlasKind kind = AtlasKind::coverage) const noexcept;
	/// @brief What changed in a page since a version.
	/// @param page The page, below atlasPages().
	/// @param version An earlier atlasVersion() of the page.
	/// @param kind Its kind.
	/// @return The smallest region holding every glyph added since; empty if none was.
	[[nodiscard]] AtlasRegion atlasChangesSince(std::size_t page, std::uint64_t version,
	                                            AtlasKind kind = AtlasKind::coverage) const noexcept;
	/// @brief Texture coordinates of a fully covered texel of a coverage page, for solid rectangles.
	/// @param page The page, below atlasPages().
	[[nodiscard]] Vec2 whiteTexel(std::size_t page = 0) const noexcept;

private:
	friend class FontCache;
	struct Data; ///< The files, the atlas, the glyphs and the shaping caches; defined in src/text/FontData.hpp.

	/// @brief An empty font, with no face or page yet: load() fills it.
	Font();

	/// @brief Loads a font, reading its files through a set that shares the bytes of files already in use.
	/// @param path As load().
	/// @param pixelHeight As load().
	/// @param fallbacks As load().
	/// @param optional More fallbacks, after @p fallbacks; one that can't be read or used is left out.
	/// @param files Reads the files.
	/// @return The font.
	/// @throws std::runtime_error As load().
	[[nodiscard]] static std::shared_ptr<const Font> load(const std::filesystem::path& path, float pixelHeight,
	                                                      std::span<const std::filesystem::path> fallbacks,
	                                                      std::span<const std::filesystem::path> optional,
	                                                      detail::FontFiles& files);

	/// @brief Rasterises Latin-1 into the first page, if it fits, taking what the font lacks from its fallbacks.
	/// @param size The page's width and height, in texels.
	/// @return Whether it fit.
	/// @pre The faces are loaded and there is no page yet.
	[[nodiscard]] bool bakeLatin1(int size);
	/// @brief glyph() for what wasn't rasterised when the font loaded: looked up, or rasterised now.
	/// @param codepoint A Unicode code point.
	/// @return As glyph().
	[[nodiscard]] const Glyph& glyphBeyondLatin1(char32_t codepoint) const noexcept;

	static constexpr char32_t firstCodepoint = 0x20; ///< First code point rasterised when the font loads.
	static constexpr char32_t lastCodepoint = 0xFF;  ///< Last code point rasterised when the font loads.
	static constexpr std::size_t latin1Count = lastCodepoint - firstCodepoint + 1; ///< Glyphs rasterised at load.

	std::uint64_t id_ = 0;     ///< See id().
	float pixelHeight_ = 0.0f; ///< See pixelHeight().
	float ascent_ = 0.0f;      ///< See ascent().
	float lineHeight_ = 0.0f;  ///< See lineHeight().
	/// @brief Latin-1, by codepoint - firstCodepoint, final once loaded (characters the font lacks hold a fallback's
	/// glyph or '?'). Here rather than in data_, so the lookup glyph() most often makes is one read.
	std::array<Glyph, latin1Count> latin1_{};
	/// @brief The font files, the atlas pages, the glyphs rasterised so far and the shaped text. Lookups add to it from
	/// const functions: rasterising and shaping on demand change how the font is stored, never what it draws.
	std::unique_ptr<Data> data_;
};

} // namespace affresco
