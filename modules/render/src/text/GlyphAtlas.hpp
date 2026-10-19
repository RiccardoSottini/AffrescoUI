#pragma once

/// @file
/// @brief detail::GlyphAtlas: a font's atlas pages, coverage and colour, filled in shelves, within a byte limit.

#include "affresco/render/Font.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace affresco::detail {

/// @brief The atlas pages of one font: square pages of coverage (one byte per texel) and of colour (four), filled in
/// shelves, rows of glyphs each as tall as its tallest.
///
/// Pages are only ever added, never resized or moved, so a glyph keeps its texture coordinates. All the pages of one
/// atlas hold at most limitBytes together.
class GlyphAtlas {
public:
	/// @brief The most bytes the pages of one atlas may hold together: text naming many characters (e.g. from a
	/// peer) can't grow it without bound. Glyphs that don't fit draw as '?'.
	static constexpr std::size_t limitBytes = std::size_t{64} << 20;
	static constexpr int whiteRows = 2; ///< Rows reserved at the top of each coverage page for its 2x2 white block.
	static constexpr int padding = 1;   ///< Empty texels left of and above each glyph, so filtering doesn't bleed.
	static constexpr int largestPage = 4096; ///< No page is larger.

	/// @brief Where a glyph's texels go.
	struct Spot {
		std::uint32_t page; ///< The page.
		int x, y;           ///< Its top-left texel, padding included.
	};

	/// @brief One square page.
	struct Page {
		/// @brief A row of glyphs.
		struct Shelf {
			int top;    ///< Its first row.
			int height; ///< Its rows.
			int right;  ///< The first free column.
		};

		int size = 0;                           ///< Width and height, in texels.
		int bytesPerTexel = 1;                  ///< 1 for coverage, 4 for colour.
		std::vector<std::uint8_t> pixels;       ///< Texels, top row first.
		std::vector<Font::AtlasRegion> changes; ///< One per glyph added after the page was made.
		std::vector<Shelf> shelves;             ///< Rows that glyphs are added to.
		int freeTop = 0;                        ///< The first row below every shelf.

		/// @brief Makes an empty page; a coverage page gets its white block.
		/// @param pageSize Width and height, in texels.
		/// @param kind Coverage or colour.
		Page(int pageSize, AtlasKind kind);

		/// @brief Finds room for a box, on a shelf or on a new one.
		/// @param width Its width, padding included.
		/// @param height Its height, padding included.
		/// @param shelfHeight The height a new shelf gets, if the box is not taller.
		/// @return Its top-left texel, or nothing if the page is full.
		[[nodiscard]] std::optional<std::array<int, 2>> place(int width, int height, int shelfHeight);
	};

	/// @brief An atlas with no page yet, until one with a size is assigned to it.
	GlyphAtlas() = default;
	/// @brief An atlas whose first coverage page has a size.
	/// @param firstPageSize Its width and height, in texels (at most largestPage).
	/// @param shelfHeight Rows a new shelf gets: one line of glyphs, padding included.
	GlyphAtlas(int firstPageSize, int shelfHeight);

	/// @brief Finds room for a glyph, adding a page if none has it.
	/// @param kind The pages it goes to.
	/// @param width Its width, padding included.
	/// @param height Its height, padding included.
	/// @param mayAddPage Whether a page may be added for it.
	/// @return Where it goes, or nothing if the atlas is full.
	[[nodiscard]] std::optional<Spot> place(AtlasKind kind, int width, int height, bool mayAddPage = true);
	/// @brief The pages of a kind.
	/// @param kind Coverage or colour.
	[[nodiscard]] std::vector<Page>& pages(AtlasKind kind) noexcept {
		return kind == AtlasKind::color ? colorPages_ : coveragePages_;
	}
	/// @brief The pages of a kind.
	/// @param kind Coverage or colour.
	[[nodiscard]] const std::vector<Page>& pages(AtlasKind kind) const noexcept {
		return kind == AtlasKind::color ? colorPages_ : coveragePages_;
	}
	/// @brief Forgets what was added so far, so it counts as the pages' first content (Latin-1, at load).
	void forgetChanges() noexcept;
	/// @brief The bytes its pages hold.
	[[nodiscard]] std::size_t bytes() const noexcept { return bytes_; }

private:
	std::vector<Page> coveragePages_; ///< The first holds Latin-1.
	std::vector<Page> colorPages_;    ///< Made when the first colour glyph is.
	std::size_t bytes_ = 0;           ///< See bytes().
	int shelfHeight_ = 0;             ///< See the constructor.
};

} // namespace affresco::detail
