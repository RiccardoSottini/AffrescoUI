#pragma once

#include "sgl/render/Geometry.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace sgl {

// A TrueType font rasterised at one pixel height into a glyph atlas (Latin-1: U+0020..U+00FF).
// Pure CPU data: each window uploads the atlas to its own GPU context the first time it draws with it.
class Font {
public:
	struct Glyph {
		Rect quad;     // relative to the pen position on the baseline (y pointing down)
		Rect uv;       // atlas texture coordinates
		float advance; // horizontal pen movement
	};

	// Throws std::runtime_error if the file can't be read or isn't a usable font
	[[nodiscard]] static std::shared_ptr<const Font> load(const std::filesystem::path& path, float pixelHeight);

	[[nodiscard]] float pixelHeight() const noexcept { return pixelHeight_; }
	[[nodiscard]] float ascent() const noexcept { return ascent_; }         // baseline to the top of the tallest glyph
	[[nodiscard]] float lineHeight() const noexcept { return lineHeight_; } // baseline to baseline

	[[nodiscard]] const Glyph& glyph(char32_t codepoint) const noexcept; // '?' for codepoints outside the atlas
	[[nodiscard]] float measure(std::u32string_view text) const noexcept;

	// Splits text into lines no wider than `maxWidth`: at '\n', at spaces, or inside words longer than a line
	[[nodiscard]] std::vector<std::u32string_view> wrap(std::u32string_view text, float maxWidth) const;

	/* Atlas: one byte of coverage per texel */
	[[nodiscard]] std::uint64_t id() const noexcept { return id_; } // unique per loaded font
	[[nodiscard]] int atlasSize() const noexcept { return atlasSize_; }
	[[nodiscard]] std::span<const std::uint8_t> atlasPixels() const noexcept { return atlas_; }
	[[nodiscard]] Vec2 whiteTexel() const noexcept; // a fully covered texel: lets solid rectangles use the atlas too

private:
	Font() = default;

	static constexpr char32_t firstCodepoint = 0x20;
	static constexpr char32_t lastCodepoint = 0xFF;

	std::uint64_t id_ = 0;
	float pixelHeight_ = 0.0f;
	float ascent_ = 0.0f;
	float lineHeight_ = 0.0f;
	int atlasSize_ = 0;
	std::vector<std::uint8_t> atlas_;
	std::vector<Glyph> glyphs_; // indexed by codepoint - firstCodepoint
};

} // namespace sgl
