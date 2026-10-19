#pragma once

/// @file
/// @brief Font: a TrueType font, with fallbacks, rasterised into glyph atlas pages on demand; measuring and word wrap.

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

/// @brief A TrueType font rasterised at one pixel height into glyph atlas pages, with fallback fonts.
///
/// Latin-1 (U+0020..U+00FF) is rasterised when the font loads, into the first page. Any other character is
/// rasterised the first time it is looked up, from the first of the font and its fallbacks that has it, and added to
/// a page with room (a new page when they are all full). Glyphs from the fallbacks share the font's pages, so text
/// that mixes them still draws in one batch. The pages of one font hold at most 64 Mi texels; characters past that
/// draw as '?', so text from a peer can't grow the atlas without bound.
///
/// Pure CPU data: each window uploads the pages to its own GPU context when it draws with them, then only what was
/// added since. Each page also holds a white block, so solid rectangles can share the text's texture. Not thread-safe:
/// a font is used on the main thread, like the panels that draw with it.
class Font {
public:
	/// @brief Where a glyph is in the atlas and how to place it.
	struct Glyph {
		Rect quad;              ///< Relative to the pen position on the baseline (y pointing down).
		Rect uv;                ///< Texture coordinates in its atlas page.
		float advance;          ///< Horizontal pen movement.
		std::uint32_t page = 0; ///< The atlas page that holds it.
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
	/// @param path A TrueType file.
	/// @param pixelHeight Size in pixels.
	/// @param fallbacks TrueType files asked, in order, for characters the font lacks. Their glyphs are drawn at
	/// @p pixelHeight, on the font's baseline.
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

	/// @brief Looks a glyph up, rasterising it the first time.
	///
	/// The reference stays valid as long as the font. A lookup may add the glyph to an atlas page, so a renderer
	/// uploads what was added (atlasChangesSince()) before it draws.
	/// @param codepoint A Unicode code point.
	/// @return Its glyph from the font or the first fallback that has it; else, and for control characters (below
	/// U+0020) or when the atlas is full, the glyph of '?'.
	[[nodiscard]] const Glyph& glyph(char32_t codepoint) const noexcept;

	/// @brief Whether the font or one of its fallbacks has a glyph for a character.
	/// @param codepoint A Unicode code point.
	[[nodiscard]] bool hasGlyph(char32_t codepoint) const noexcept;

	/// @brief Measures one line of text.
	/// @param text The text.
	/// @return Its width in pixels.
	[[nodiscard]] float measure(std::u32string_view text) const noexcept;

	/// @brief Splits text into lines: at '\n', else at the last space that fits, else inside a word.
	/// @param text The text.
	/// @param maxWidth Widest line, in pixels.
	/// @return Views into @p text, one per line.
	[[nodiscard]] std::vector<std::u32string_view> wrap(std::u32string_view text, float maxWidth) const;

	/* Atlas: pages of one byte of coverage per texel */

	/// @brief Unique per loaded font, e.g. to cache its GPU textures.
	[[nodiscard]] std::uint64_t id() const noexcept { return id_; }
	/// @brief How many atlas pages there are. Never fewer than one; pages are only ever added.
	[[nodiscard]] std::size_t atlasPages() const noexcept;
	/// @brief A page's width and height, in texels.
	/// @param page The page, below atlasPages().
	[[nodiscard]] int atlasSize(std::size_t page = 0) const noexcept;
	/// @brief A page's coverage, top row first.
	/// @param page The page, below atlasPages().
	[[nodiscard]] std::span<const std::uint8_t> atlasPixels(std::size_t page = 0) const noexcept;
	/// @brief How many times glyphs were added to a page since it was made: compare it to know what to upload.
	/// @param page The page, below atlasPages().
	[[nodiscard]] std::uint64_t atlasVersion(std::size_t page) const noexcept;
	/// @brief What changed in a page since a version.
	/// @param page The page, below atlasPages().
	/// @param version An earlier atlasVersion() of the page.
	/// @return The smallest region holding every glyph added since; empty if none was.
	[[nodiscard]] AtlasRegion atlasChangesSince(std::size_t page, std::uint64_t version) const noexcept;
	/// @brief Texture coordinates of a fully covered texel of a page, for solid rectangles.
	/// @param page The page, below atlasPages().
	[[nodiscard]] Vec2 whiteTexel(std::size_t page = 0) const noexcept;

private:
	struct Glyphs;

	Font();

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
	/// glyph or '?'). Here rather than in glyphs_, so the lookup most text makes is one read.
	std::array<Glyph, latin1Count> latin1_{};
	/// @brief The font files, the atlas pages and the glyphs rasterised so far. Lookups add to it from const
	/// functions: rasterising on demand changes how the font is stored, never what it draws.
	std::unique_ptr<Glyphs> glyphs_;
};

} // namespace affresco
