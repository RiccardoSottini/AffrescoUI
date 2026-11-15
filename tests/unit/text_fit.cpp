/// @file
/// @brief Glyphs fitted to pixel columns below a display scale of 2 (render's Font::fitGlyph()): four bitmaps per
/// glyph, moved right by quarters of a pixel, rasterised on demand within the atlas's limit.
///
/// The fonts are the examples' Roboto (AFFRESCOUI_FONTS_DIR) and the rendering check's colour emoji subset
/// (AFFRESCOUI_TEST_FONTS_DIR).

#include "paint/PaintRecorder.hpp"

#include <affresco/render/Font.hpp>

#include <doctest/doctest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

using affresco::AtlasKind;
using affresco::Font;

// The quarter a glyph's bitmap is moved by fits in what was padding after its colour flag
static_assert(sizeof(Font::Glyph) <= 52, "a glyph keeps its size: every font holds thousands");

namespace {

/// @brief Loads Roboto.
/// @param pixelHeight Its size.
/// @return The font.
std::shared_ptr<const Font> roboto(float pixelHeight) {
	return Font::load(std::filesystem::path(AFFRESCOUI_FONTS_DIR) / "Roboto-Regular.ttf", pixelHeight);
}

/// @brief Where a glyph's ink is centred across, from its pen: its bitmap's left plus the coverage-weighted centre of
/// its columns.
/// @param raster The font or raster that holds the bitmap.
/// @param glyph The glyph.
/// @return The centre, in texels (the raster's pixels).
double inkCentre(const Font& raster, const Font::Glyph& glyph) {
	const int size = raster.atlasSize(glyph.page);
	const std::span<const std::uint8_t> pixels = raster.atlasPixels(glyph.page);
	const auto left = std::lround(glyph.uv.left * static_cast<float>(size));
	const auto top = std::lround(glyph.uv.top * static_cast<float>(size));
	const auto right = std::lround(glyph.uv.right * static_cast<float>(size));
	const auto bottom = std::lround(glyph.uv.bottom * static_cast<float>(size));
	double weight = 0.0, moment = 0.0;
	for(long row = top; row < bottom; ++row) {
		for(long column = left; column < right; ++column) {
			const double coverage = pixels[static_cast<std::size_t>(row * size + column)];
			weight += coverage;
			moment += coverage * (static_cast<double>(column - left) + 0.5);
		}
	}
	const double bitmapLeft = static_cast<double>(glyph.quad.left) * static_cast<double>(raster.rasterScale());
	return weight > 0.0 ? bitmapLeft + moment / weight : bitmapLeft;
}

/// @brief What a font's atlas has taken so far: each page's version, and their count.
/// @param font The font or raster.
/// @return One version per coverage page, then one per colour page.
std::vector<std::uint64_t> atlasVersions(const Font& font) {
	std::vector<std::uint64_t> versions;
	for(const AtlasKind kind : {AtlasKind::coverage, AtlasKind::color}) {
		for(std::size_t page = 0; page < font.atlasPages(kind); ++page) versions.push_back(font.atlasVersion(page, kind));
		versions.push_back(0); // a separator, so a page moving from one kind to the other shows
	}
	return versions;
}

/// @brief Records a text as a panel's paint() would.
/// @param font The font.
/// @param text The text.
/// @param scale The frame's pixels per unit.
void record(const Font& font, std::u32string_view text, float scale) {
	affresco::detail::PaintRecord record;
	const affresco::Rect clip{0.0f, 0.0f, 2000.0f, 200.0f};
	affresco::detail::PaintRecorder recorder(record, clip, clip, scale);
	recorder.drawText(font, text, {10.3f, 40.0f}, affresco::colors::black);
}

} // namespace

// A centre taken at pixel centres is off by up to about a tenth of a pixel for one narrow stem (the ink inside a column
// is not centred in it), and that error changes sign from glyph to glyph: so each glyph is held to an eighth of a pixel,
// and the 20 together, on average, to 0.03.
TEST_CASE("text fit: a glyph's bitmap at shift k has its ink k quarters of a pixel right of shift 0") {
	const auto font = roboto(15.0f);
	constexpr std::u32string_view letters = U"PinedWavgylmoxAHQRéΩ"; // 20 glyphs, stems and bowls
	for(const float scale : {1.0f, 1.25f}) {
		CAPTURE(scale);
		const Font& raster = font->rasterAt(scale);
		std::array<double, 4> error{};
		for(const char32_t letter : letters) {
			CAPTURE(static_cast<unsigned>(letter));
			const Font::Glyph& glyph = font->glyph(letter);
			const Font::FittedGlyph unmoved = raster.fitGlyph(glyph, 0.0f);
			REQUIRE(unmoved.glyph->shift == 0);
			CHECK(unmoved.pen == 0.0f);
			const double origin = inkCentre(raster, *unmoved.glyph);
			for(unsigned shift = 1; shift < 4; ++shift) {
				CAPTURE(shift);
				const Font::FittedGlyph moved = raster.fitGlyph(glyph, static_cast<float>(shift) / 4.0f / scale);
				REQUIRE(moved.glyph->shift == shift);
				CHECK(moved.pen == 0.0f); // on column 0, its bitmap moved
				CHECK(moved.glyph->advance == glyph.advance);
				const double off = inkCentre(raster, *moved.glyph) - origin - static_cast<double>(shift) / 4.0;
				CHECK(std::abs(off) <= 0.125);
				error[shift] += off;
			}
			// A fraction nearer the next pixel than three quarters: the next column, unmoved
			const Font::FittedGlyph next = raster.fitGlyph(glyph, 0.9f / scale);
			CHECK(next.glyph == unmoved.glyph);
			CHECK(next.pen * scale == doctest::Approx(1.0f));
		}
		for(unsigned shift = 1; shift < 4; ++shift) {
			CAPTURE(shift);
			CHECK(std::abs(error[shift]) / static_cast<double>(letters.size()) <= 0.03);
		}
	}
}

TEST_CASE("text fit: a colour glyph is never moved, and at 2 a glyph is drawn as laid out") {
	const std::array fallback{std::filesystem::path(AFFRESCOUI_TEST_FONTS_DIR) / "TwemojiMozilla-Subset.ttf"};
	const auto font = Font::load(std::filesystem::path(AFFRESCOUI_FONTS_DIR) / "Roboto-Regular.ttf", 16.0f, fallback);
	const Font::Glyph& rocket = font->glyph(U'\U0001F680');
	REQUIRE(rocket.color);
	for(const float scale : {1.0f, 1.25f, 1.5f}) {
		CAPTURE(scale);
		const Font& raster = font->rasterAt(scale);
		const Font::FittedGlyph fitted = raster.fitGlyph(rocket, 10.3f);
		CHECK(fitted.glyph == &raster.rasterGlyph(rocket));
		CHECK(fitted.glyph->shift == 0);
		CHECK(fitted.pen == std::round(10.3f * scale) / scale); // the nearest column
	}

	const Font& dense = font->rasterAt(2.0f);
	const Font::FittedGlyph fitted = dense.fitGlyph(font->glyph(U'W'), 10.3f);
	CHECK(fitted.glyph == &dense.rasterGlyph(font->glyph(U'W')));
	CHECK(fitted.pen == 10.3f);
}

TEST_CASE("text fit: drawing a text again adds nothing, and whole pixels make no moved bitmap") {
	constexpr std::u32string_view text = U"Pinned to the sidebar: evenly spaced, Ω and é";
	for(const float scale : {1.0f, 1.25f, 1.5f}) {
		CAPTURE(scale);
		const auto font = roboto(15.0f);
		const Font& raster = font->rasterAt(scale);
		record(*font, text, scale);
		const std::vector<std::uint64_t> versions = atlasVersions(raster);
		const std::size_t bytes = font->memoryBytes();
		record(*font, text, scale);
		CHECK(atlasVersions(raster) == versions);
		CHECK(font->memoryBytes() == bytes);

		// Every pen on a whole pixel: shift 0 only, which is already there
		const auto fresh = roboto(15.0f);
		const Font& wholeRaster = fresh->rasterAt(scale);
		const Font::ShapedText& shaped = fresh->shape(text);
		for(const Font::ShapedGlyph& glyph : shaped.glyphs) static_cast<void>(wholeRaster.fitGlyph(*glyph.glyph, 0.0f));
		const std::size_t unmoved = fresh->memoryBytes();
		for(const Font::ShapedGlyph& glyph : fresh->shape(text).glyphs) {
			const float pen = std::round(glyph.x * scale) / scale; // a whole pixel
			CHECK(wholeRaster.fitGlyph(*glyph.glyph, pen).glyph->shift == 0);
		}
		CHECK(fresh->memoryBytes() == unmoved);
	}
}

TEST_CASE("text fit: with the atlas full, a glyph draws its plain bitmap on the nearest column, not '?'") {
	const auto font = roboto(300.0f); // its Latin-1 page is the largest: the limit after a few pages
	const Font::Glyph& question = font->glyph(U'?');

	// Every glyph from U+0100 on, at every quarter, until the pages are at their limit
	std::size_t refused = 0;
	for(char32_t c = 0x100; c <= 0x2FFF; ++c) {
		if(!font->hasGlyph(c)) continue;
		const Font::Glyph& glyph = font->glyph(c);
		for(unsigned shift = 0; shift < 4; ++shift)
			if(font->fitGlyph(glyph, static_cast<float>(shift) / 4.0f).glyph->shift != shift) ++refused;
	}
	REQUIRE(refused > 0);

	std::size_t bytes = 0;
	for(std::size_t page = 0; page < font->atlasPages(); ++page)
		bytes += static_cast<std::size_t>(font->atlasSize(page)) * static_cast<std::size_t>(font->atlasSize(page));
	for(std::size_t page = 0; page < font->atlasPages(AtlasKind::color); ++page)
		bytes += 4 * static_cast<std::size_t>(font->atlasSize(page, AtlasKind::color)) *
		         static_cast<std::size_t>(font->atlasSize(page, AtlasKind::color));
	CHECK(bytes <= std::size_t{64} << 20);

	// Latin-1 letters never drawn: their plain bitmaps, from the first page, on the nearest column
	for(const char32_t letter : std::u32string_view(U"AKMwz")) {
		CAPTURE(static_cast<unsigned>(letter));
		const Font::Glyph& glyph = font->glyph(letter);
		const Font::FittedGlyph fitted = font->fitGlyph(glyph, 20.4f);
		CHECK(fitted.glyph->page == glyph.page); // the plain bitmap (at 1, a copy of it kept with the drawn ones)
		CHECK(fitted.glyph->uv.left == glyph.uv.left);
		CHECK(fitted.glyph->uv.top == glyph.uv.top);
		CHECK(fitted.glyph->shift == 0);
		CHECK(fitted.glyph->uv.left != question.uv.left);
		CHECK(fitted.pen == 20.0f);
		CHECK(font->fitGlyph(glyph, 20.6f).pen == 21.0f);
	}
}
