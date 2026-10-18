#pragma once

/// @file
/// @brief Font: a TrueType font rasterised into a glyph atlas, with measuring and word wrap.

#include "affresco/render/Geometry.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace affresco {

/// @brief A TrueType font rasterised at one pixel height into a glyph atlas (Latin-1: U+0020..U+00FF).
///
/// Pure CPU data: each window uploads the atlas to its own GPU context the first time it draws with it. The atlas
/// also holds a white block, so solid rectangles can share the text's texture.
class Font {
public:
	/// @brief Where a glyph is in the atlas and how to place it.
	struct Glyph {
		Rect quad;     ///< Relative to the pen position on the baseline (y pointing down).
		Rect uv;       ///< Atlas texture coordinates.
		float advance; ///< Horizontal pen movement.
	};

	/// @brief Loads and rasterises a font.
	/// @param path A TrueType file.
	/// @param pixelHeight Size in pixels.
	/// @return The font, in the smallest square atlas (256 to 4096 texels) that fits.
	/// @throws std::runtime_error If the file can't be read, isn't a usable font, or doesn't fit.
	[[nodiscard]] static std::shared_ptr<const Font> load(const std::filesystem::path& path, float pixelHeight);

	/// @brief The size it was rasterised at, in pixels.
	[[nodiscard]] float pixelHeight() const noexcept { return pixelHeight_; }
	/// @brief Distance from the baseline to the top of the tallest glyph.
	[[nodiscard]] float ascent() const noexcept { return ascent_; }
	/// @brief Distance from one baseline to the next.
	[[nodiscard]] float lineHeight() const noexcept { return lineHeight_; }

	/// @brief Looks a glyph up.
	/// @param codepoint A Unicode code point.
	/// @return Its glyph, or '?' for code points outside the atlas.
	[[nodiscard]] const Glyph& glyph(char32_t codepoint) const noexcept;

	/// @brief Measures one line of text.
	/// @param text The text.
	/// @return Its width in pixels.
	[[nodiscard]] float measure(std::u32string_view text) const noexcept;

	/// @brief Splits text into lines: at '\n', else at the last space that fits, else inside a word.
	/// @param text The text.
	/// @param maxWidth Widest line, in pixels.
	/// @return Views into @p text, one per line.
	[[nodiscard]] std::vector<std::u32string_view> wrap(std::u32string_view text, float maxWidth) const;

	/* Atlas: one byte of coverage per texel */

	/// @brief Unique per loaded font, e.g. to cache its GPU texture.
	[[nodiscard]] std::uint64_t id() const noexcept { return id_; }
	/// @brief Atlas width and height, in texels.
	[[nodiscard]] int atlasSize() const noexcept { return atlasSize_; }
	/// @brief Atlas coverage, top row first.
	[[nodiscard]] std::span<const std::uint8_t> atlasPixels() const noexcept { return atlas_; }
	/// @brief Texture coordinates of a fully covered texel, for solid rectangles.
	[[nodiscard]] Vec2 whiteTexel() const noexcept;

private:
	Font() = default;

	static constexpr char32_t firstCodepoint = 0x20; ///< First code point in the atlas.
	static constexpr char32_t lastCodepoint = 0xFF;  ///< Last code point in the atlas.

	std::uint64_t id_ = 0;            ///< See id().
	float pixelHeight_ = 0.0f;        ///< See pixelHeight().
	float ascent_ = 0.0f;             ///< See ascent().
	float lineHeight_ = 0.0f;         ///< See lineHeight().
	int atlasSize_ = 0;               ///< See atlasSize().
	std::vector<std::uint8_t> atlas_; ///< See atlasPixels().
	std::vector<Glyph> glyphs_;       ///< Indexed by codepoint - firstCodepoint.
};

} // namespace affresco
