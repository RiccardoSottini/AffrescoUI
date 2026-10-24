/// @file
/// @brief Text on HiDPI displays (render's Font::rasterAt()): glyphs rasterised at the frame's pixels per unit, while
/// shaping, measuring, wrapping and where glyphs go stay in window units, the same at any scale.
///
/// The font is the examples' Roboto (AFFRESCOUI_FONTS_DIR).

#include "Headless.hpp"
#include "paint/GeometryCache.hpp"
#include "paint/PaintRecorder.hpp"
#include "paint/RetainedGeometry.hpp"

#include <affresco/render/Font.hpp>
#include <affresco/render/Label.hpp>
#include <affresco/render/Window.hpp>

#include <doctest/doctest.h>

#include <cmath>
#include <cstddef>
#include <filesystem>
#include <limits>
#include <memory>
#include <string_view>
#include <vector>

using affresco::Font;
using affresco::Rect;
using affresco::detail::PaintRecord;
using affresco::detail::PaintRecorder;
using affresco::detail::PaintStep;

namespace {

/// @brief Loads Roboto.
/// @param pixelHeight Its size.
/// @return The font.
std::shared_ptr<const Font> roboto(float pixelHeight) {
	return Font::load(std::filesystem::path(AFFRESCOUI_FONTS_DIR) / "Roboto-Regular.ttf", pixelHeight);
}

/// @brief What a font lays a text out as: its width, its advances and carets, where its glyphs go, and its lines.
struct Layout {
	float width = 0.0f;                   ///< measure().
	std::vector<float> advances;          ///< shape(): per code point.
	std::vector<float> carets;            ///< shape(): per code point and one more.
	std::vector<float> pens;              ///< shape(): each glyph's pen position.
	std::vector<std::size_t> lineLengths; ///< wrap() at 120 pixels: each line's length.

	/// @brief Equal when every number is, to the last bit.
	bool operator==(const Layout&) const = default;
};

/// @brief Lays a text out.
/// @param font The font.
/// @param text The text.
/// @return Its layout.
Layout layOut(const Font& font, std::u32string_view text) {
	Layout layout;
	layout.width = font.measure(text);
	const Font::ShapedText& shaped = font.shape(text);
	layout.advances = shaped.advances;
	layout.carets = shaped.carets;
	for(const Font::ShapedGlyph& glyph : shaped.glyphs) layout.pens.push_back(glyph.x);
	for(const std::u32string_view line : font.wrap(text, 120.0f)) layout.lineLengths.push_back(line.size());
	return layout;
}

/// @brief Records a text as a panel's paint() would, for frames of some pixels per unit.
/// @param font The font.
/// @param text The text.
/// @param scale The frames' pixels per unit.
/// @return The record.
PaintRecord record(const Font& font, std::u32string_view text, float scale) {
	PaintRecord record;
	const Rect clip{0.0f, 0.0f, 1000.0f, 200.0f};
	PaintRecorder recorder(record, clip, clip, scale);
	recorder.drawText(font, text, {10.0f, 40.0f}, affresco::colors::black);
	return record;
}

/// @brief The font a record's text is drawn from: its first glyph step's.
/// @param record The record.
/// @return The font, or nullptr if no glyph was drawn.
const Font* drawnFrom(const PaintRecord& record) {
	for(const affresco::detail::PaintOp& op : record.ops)
		if(op.step == PaintStep::glyphs) return op.font;
	return nullptr;
}

/// @brief How many texels wide and tall a glyph's bitmap is in its font's atlas.
/// @param font The font (or raster) that holds it.
/// @param glyph The glyph.
/// @return Width and height, in texels.
affresco::Vec2 texels(const Font& font, const Font::Glyph& glyph) {
	const auto size = static_cast<float>(font.atlasSize(glyph.page));
	return {std::round(glyph.uv.width() * size), std::round(glyph.uv.height() * size)};
}

} // namespace

TEST_CASE("text scale: a frame's pixels per unit round to a quarter from 1 to 4, and 1 is the font itself") {
	CHECK(Font::rasterScaleFor(1.0f) == 1.0f);
	CHECK(Font::rasterScaleFor(1.1f) == 1.0f);
	CHECK(Font::rasterScaleFor(0.25f) == 1.0f); // thumbnails draw from the font's own glyphs, as before
	CHECK(Font::rasterScaleFor(1.25f) == 1.25f);
	CHECK(Font::rasterScaleFor(1.49f) == 1.5f);
	CHECK(Font::rasterScaleFor(2.0f) == 2.0f);
	CHECK(Font::rasterScaleFor(2.0004f) == 2.0f); // a framebuffer rounded to whole pixels
	CHECK(Font::rasterScaleFor(3.0f) == 3.0f);
	CHECK(Font::rasterScaleFor(9.0f) == 4.0f);
	CHECK(Font::rasterScaleFor(std::numeric_limits<float>::infinity()) == 4.0f);
	CHECK(Font::rasterScaleFor(std::numeric_limits<float>::quiet_NaN()) == 1.0f);

	const auto font = roboto(15.0f);
	CHECK(&font->rasterAt(1.0f) == font.get());
	CHECK(&font->rasterAt(1.1f) == font.get());
	const Font& raster = font->rasterAt(2.0f);
	CHECK(&raster != font.get());
	CHECK(&font->rasterAt(2.0004f) == &raster); // one raster per scale, kept
	CHECK(&raster.rasterAt(3.0f) == &raster);   // a raster has none of its own
	CHECK(raster.rasterScale() == 2.0f);
	CHECK(font->rasterScale() == 1.0f);
	CHECK(raster.id() != font->id()); // its pages are textures of their own
	CHECK(raster.ascent() == font->ascent());
	CHECK(raster.lineHeight() == font->lineHeight());
	CHECK(raster.pixelHeight() == font->pixelHeight());
}

TEST_CASE("text scale: at a scale of 2, shaping, measuring and wrapping give the numbers they give at 1") {
	const std::u32string_view text = U"Fresco painting, one giornata at a time: Ωμέγα, ffi, 12.5%";
	const auto font = roboto(15.0f);
	const Layout before = layOut(*font, text);

	const PaintRecord atOne = record(*font, text, 1.0f);
	const PaintRecord atTwo = record(*font, text, 2.0f);
	CHECK(layOut(*font, text) == before);
	CHECK(layOut(*roboto(15.0f), text) == before); // a font never drawn at 2: the same numbers

	// The same glyphs in the same places, each within a pixel: only the bitmaps are denser
	CHECK(drawnFrom(atOne) == font.get());
	CHECK(drawnFrom(atTwo) == &font->rasterAt(2.0f));
	REQUIRE(atOne.vertices.size() == atTwo.vertices.size());
	REQUIRE(!atOne.vertices.empty());
	for(std::size_t i = 0; i < atOne.vertices.size(); ++i) {
		CHECK(std::abs(atOne.vertices[i].x - atTwo.vertices[i].x) <= 1.0f);
		CHECK(std::abs(atOne.vertices[i].y - atTwo.vertices[i].y) <= 1.0f);
	}
}

TEST_CASE("text scale: a glyph at a scale of 2 is as large in pixels, from a bitmap about twice as large") {
	const auto font = roboto(15.0f);
	const Font& raster = font->rasterAt(2.0f);

	for(const char32_t c : {U'W', U'g', U'Ω', U'é', U'?'}) {
		CAPTURE(static_cast<unsigned>(c));
		const Font::Glyph& glyph = font->glyph(c);
		const Font::Glyph& dense = raster.rasterGlyph(glyph);
		CHECK(&raster.rasterGlyph(glyph) == &dense); // rasterised once
		CHECK(dense.advance == glyph.advance);
		CHECK(std::abs(dense.quad.left - glyph.quad.left) <= 1.0f);
		CHECK(std::abs(dense.quad.top - glyph.quad.top) <= 1.0f);
		CHECK(std::abs(dense.quad.right - glyph.quad.right) <= 1.0f);
		CHECK(std::abs(dense.quad.bottom - glyph.quad.bottom) <= 1.0f);

		const affresco::Vec2 one = texels(*font, glyph), two = texels(raster, dense);
		CHECK(two.x == doctest::Approx(2.0f * one.x).epsilon(0.25));
		CHECK(two.y == doctest::Approx(2.0f * one.y).epsilon(0.25));
		// Its quad is its bitmap's rectangle in pixels
		CHECK(dense.quad.width() == doctest::Approx(two.x / 2.0f));
		CHECK(dense.quad.height() == doctest::Approx(two.y / 2.0f));
	}

	// The raster counts in its font's memory; the font's own atlas didn't grow
	CHECK(font->memoryBytes() > raster.memoryBytes());
	CHECK(font->atlasPages() == 1);
	CHECK(font->atlasVersion(0) == 1); // Omega, at 1: rasterised for its lookup above
}

TEST_CASE("text scale: a raster's atlas stops growing at the same limit, and glyphs past it draw as '?'") {
	const auto font = roboto(150.0f); // 300 pixels at a scale of 2: the limit after a few hundred glyphs
	const Font& raster = font->rasterAt(2.0f);
	REQUIRE(&raster != font.get());
	const Font::Glyph& question = raster.rasterGlyph(font->glyph(U'?'));

	std::size_t questions = 0;
	for(char32_t c = 0x100; c <= 0x2FFF; ++c) {
		if(!font->hasGlyph(c)) continue;
		const Font::Glyph& dense = raster.rasterGlyph(font->glyph(c));
		if(dense.page == question.page && dense.uv == question.uv && dense.quad == question.quad) ++questions;
	}
	CHECK(questions > 0);

	std::size_t bytes = 0;
	for(std::size_t page = 0; page < raster.atlasPages(); ++page)
		bytes += static_cast<std::size_t>(raster.atlasSize(page)) * static_cast<std::size_t>(raster.atlasSize(page));
	CHECK(bytes <= std::size_t{64} << 20);
}

TEST_CASE("text scale: a frame at another scale records its text again, from that scale's glyphs") {
	const affresco::detail::ManualClock clock;
	const auto window = affresco::detail::createHeadlessWindow(400, 120, clock);
	const auto font = roboto(15.0f);
	affresco::Label& label = window->root().add<affresco::Label>("Moved to a Retina display");
	label.setFont(font);
	label.setSize(affresco::px(380), affresco::px(40));

	affresco::detail::GeometryCache cache;
	static_cast<void>(cache.refresh(affresco::detail::takeFrame(*window), 1.0f));
	CHECK_FALSE(cache.refresh(affresco::detail::takeFrame(*window), 1.0f).damage); // nothing changed

	// The display's scale changed: the text is rasterised at 2, and everything is drawn again
	const std::size_t before = font->memoryBytes();
	CHECK(cache.refresh(affresco::detail::takeFrame(*window), 2.0f).damage);
	const Font& raster = font->rasterAt(2.0f);
	CHECK(raster.atlasVersion(0) > 1); // '?', then the label's glyphs
	CHECK(font->memoryBytes() > before);
	CHECK_FALSE(cache.refresh(affresco::detail::takeFrame(*window), 2.0f).damage);

	// Back on the first display: its glyphs are the font's own again, the raster kept for the next move
	const std::uint64_t rasterised = raster.atlasVersion(0);
	CHECK(cache.refresh(affresco::detail::takeFrame(*window), 1.0f).damage);
	CHECK(cache.refresh(affresco::detail::takeFrame(*window), 2.0f).damage);
	CHECK(raster.atlasVersion(0) == rasterised); // nothing rasterised twice
	CHECK(&font->rasterAt(2.0f) == &raster);
}
