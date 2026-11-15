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
		for(std::size_t page = 0; page < font.atlasPages(kind); ++page)
			versions.push_back(font.atlasVersion(page, kind));
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
// is not centred in it), and that error changes sign from glyph to glyph: so each glyph is held to an eighth of a
// pixel, and the 20 together, on average, to 0.03.
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
	const Font::Glyph* plain = nullptr; // one refused: drawn from its plain bitmap
	for(char32_t c = 0x100; c <= 0x2FFF; ++c) {
		if(!font->hasGlyph(c)) continue;
		const Font::Glyph& glyph = font->glyph(c);
		for(unsigned shift = 0; shift < 4; ++shift) {
			if(font->fitGlyph(glyph, static_cast<float>(shift) / 4.0f).glyph->shift == shift) continue;
			++refused;
			if(shift == 2) plain = &glyph;
		}
	}
	REQUIRE(refused > 0);
	REQUIRE(plain != nullptr);

	std::size_t bytes = 0;
	for(std::size_t page = 0; page < font->atlasPages(); ++page)
		bytes += static_cast<std::size_t>(font->atlasSize(page)) * static_cast<std::size_t>(font->atlasSize(page));
	for(std::size_t page = 0; page < font->atlasPages(AtlasKind::color); ++page)
		bytes += 4 * static_cast<std::size_t>(font->atlasSize(page, AtlasKind::color)) *
		         static_cast<std::size_t>(font->atlasSize(page, AtlasKind::color));
	CHECK(bytes <= std::size_t{64} << 20);

	// A glyph refused: its plain bitmap on the nearest column, not '?'
	const Font::FittedGlyph fitted = font->fitGlyph(*plain, 20.4f);
	CHECK(fitted.glyph->page == plain->page); // the plain bitmap (at 1, a copy of it kept with the drawn ones)
	CHECK(fitted.glyph->uv.left == plain->uv.left);
	CHECK(fitted.glyph->uv.top == plain->uv.top);
	CHECK(fitted.glyph->shift == 0);
	CHECK(fitted.glyph->uv.left != question.uv.left);
	CHECK(fitted.pen == 20.0f);
	CHECK(font->fitGlyph(*plain, 20.6f).pen == 21.0f);

	// Latin-1 letters never drawn still have their places: every quarter, in the first drawn page
	for(const char32_t letter : std::u32string_view(U"AKMwz")) {
		CAPTURE(static_cast<unsigned>(letter));
		const Font::Glyph& glyph = font->glyph(letter);
		const Font::FittedGlyph moved = font->fitGlyph(glyph, 20.5f);
		CHECK(moved.glyph->shift == 2);
		CHECK(moved.glyph->page != glyph.page);
		CHECK(moved.pen == 20.0f);
	}
}

namespace {

/// @brief How a font's raster fits each glyph of a text: its bitmap's place around the pen, its shift and its pen.
/// @param font The font.
/// @param text The text.
/// @param scale The frame's pixels per unit.
/// @return Per glyph: the quad's left, top, right and bottom, the shift, then the pen.
std::vector<float> fits(const Font& font, std::u32string_view text, float scale) {
	const Font& raster = font.rasterAt(scale);
	std::vector<float> values;
	for(const Font::ShapedGlyph& shaped : font.shape(text).glyphs) {
		const Font::FittedGlyph fitted = raster.fitGlyph(*shaped.glyph, 10.3f + shaped.x);
		const affresco::Rect& quad = fitted.glyph->quad;
		values.insert(values.end(), {quad.left, quad.top, quad.right, quad.bottom,
		                             static_cast<float>(fitted.glyph->shift), fitted.pen});
	}
	return values;
}

} // namespace

TEST_CASE("text fit: a glyph is drawn the same whatever was drawn before it, past a full page") {
	constexpr std::u32string_view first = U"Pinned to the sidebar";
	constexpr std::u32string_view later = U"Évenly spaced, quick brown ×";
	for(const float scale : {1.0f, 1.25f}) {
		CAPTURE(scale);
		const auto alone = roboto(15.0f);
		const std::vector<float> laterAlone = fits(*alone, later, scale);

		const auto font = roboto(15.0f);
		const Font& raster = font->rasterAt(scale);
		const std::vector<float> firstTime = fits(*font, first, scale);
		const std::size_t pages = raster.atlasPages();
		// Enough other glyphs, at every quarter, to fill the page they went to and more
		for(char32_t c = 0x100; c <= 0x2FFF && raster.atlasPages() == pages; ++c) {
			if(!font->hasGlyph(c)) continue;
			const Font::Glyph& glyph = font->glyph(c);
			for(unsigned shift = 0; shift < 4; ++shift)
				static_cast<void>(raster.fitGlyph(glyph, static_cast<float>(shift) / (4.0f * scale)));
		}
		REQUIRE(raster.atlasPages() > pages); // the page drawn glyphs went to is full
		CHECK(fits(*font, first, scale) == firstTime);
		CHECK(fits(*font, later, scale) == laterAlone);
	}
}

// A font drawing itself makes its drawn page the first time it draws. If its atlas is already at its limit then (it
// measured enough large glyphs first), that page is never made, and a glyph with no texels must not name it
TEST_CASE("text fit: with the atlas full before a font first draws, every glyph it draws names a page it has") {
	const auto font = roboto(300.0f);
	const Font::Glyph& question = font->glyph(U'?');
	bool full = false;
	for(char32_t c = 0x100; c <= 0x2FFF && !full; ++c)
		if(font->hasGlyph(c) && font->glyph(c).uv.left == question.uv.left && font->glyph(c).uv.top == question.uv.top)
			full = true; // measured as '?': no room left
	REQUIRE(full);

	const std::size_t pages = font->atlasPages();
	for(const char32_t letter : std::u32string_view(U" A")) {
		CAPTURE(static_cast<unsigned>(letter));
		for(unsigned shift = 0; shift < 4; ++shift) {
			CAPTURE(shift);
			const Font::FittedGlyph fitted =
			    font->fitGlyph(font->glyph(letter), 20.0f + static_cast<float>(shift) / 4.0f);
			CHECK(fitted.glyph->page < font->atlasPages());
		}
	}
	CHECK(font->atlasPages() == pages);
	record(*font, U"A B", 1.0f); // drawn: its steps name pages a window can upload
}
