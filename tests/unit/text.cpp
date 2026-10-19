/// @file
/// @brief Unit tests for text beyond Latin-1: UTF-8 decoding (render's src/detail/Utf8.hpp), glyphs rasterised on
/// demand into atlas pages, fallback fonts, and the "fallback" key of a screen's "fonts".
///
/// The fonts are the examples' (AFFRESCOUI_FONTS_DIR): Roboto has Latin, Greek and Cyrillic but no arrows or check
/// marks, which JetBrains Mono has, so it is the fallback here. Neither has U+2605 (a black star).

#include "detail/Utf8.hpp"

#include <affresco/format/Rules.hpp>
#include <affresco/format/Runtime.hpp>
#include <affresco/render/Font.hpp>
#include <affresco/vcs/Repository.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

using affresco::Font;
using affresco::json;

namespace {

/// @brief The examples' fonts.
/// @return Their folder.
std::filesystem::path fonts() { return AFFRESCOUI_FONTS_DIR; }

/// @brief Loads Roboto, with or without JetBrains Mono as its fallback.
/// @param pixelHeight The size.
/// @param withFallback Whether to add the fallback.
/// @return The font.
std::shared_ptr<const Font> roboto(float pixelHeight, bool withFallback) {
	const std::array<std::filesystem::path, 1> fallback{fonts() / "JetBrainsMono-Regular.ttf"};
	return Font::load(fonts() / "Roboto-Regular.ttf", pixelHeight,
	                  withFallback ? std::span<const std::filesystem::path>(fallback)
	                               : std::span<const std::filesystem::path>());
}

/// @brief Whether a region of an atlas page has any coverage.
/// @param font The font.
/// @param page The page.
/// @param region The region.
bool covered(const Font& font, std::size_t page, const Font::AtlasRegion& region) {
	const std::span<const std::uint8_t> pixels = font.atlasPixels(page);
	const auto size = static_cast<std::size_t>(font.atlasSize(page));
	for(int y = region.top; y < region.bottom; ++y)
		for(int x = region.left; x < region.right; ++x)
			if(pixels[static_cast<std::size_t>(y) * size + static_cast<std::size_t>(x)] != 0) return true;
	return false;
}

/// @brief Whether a character draws as '?': the same texels, placed the same way.
/// @param font The font.
/// @param codepoint The character.
bool drawsAsQuestionMark(const Font& font, char32_t codepoint) {
	const Font::Glyph& glyph = font.glyph(codepoint);
	const Font::Glyph& question = font.glyph(U'?');
	return glyph.page == question.page && glyph.uv == question.uv && glyph.quad == question.quad &&
	       glyph.advance == question.advance;
}

/// @brief A screen whose default font names fallbacks.
/// @param fallback The "fallback" value.
/// @return The document.
json screenWithFallback(const json& fallback) {
	return {{"format", "affrescoui-screen"},
	        {"version", 1},
	        {"fonts",
	         {{"default", {{"file", "Roboto-Regular.ttf"}, {"size", 14}, {"fallback", fallback}}},
	          {"mono", {{"file", "JetBrainsMono-Regular.ttf"}, {"size", 13}}}}},
	        {"root", {{"type", "panel"}}}};
}

} // namespace

TEST_CASE("text: UTF-8 decodes well-formed sequences and replaces each invalid one with one U+FFFD") {
	using affresco::detail::fromUtf8;

	CHECK(fromUtf8("") == U"");
	CHECK(fromUtf8("caf\xC3\xA9") == U"café");                          // 2 bytes
	CHECK(fromUtf8("\xCE\xA9\xD0\x96\xE2\x82\xAC") == U"ΩЖ€");          // Greek, Cyrillic, euro
	CHECK(fromUtf8("\xF0\x9F\x98\x80!") == U"\U0001F600!");             // 4 bytes
	CHECK(fromUtf8("\xF4\x8F\xBF\xBF") == U"\U0010FFFF");               // the last code point
	CHECK(fromUtf8("\xC0\xAF") == U"\uFFFD\uFFFD");                     // overlong '/'
	CHECK(fromUtf8("\xE0\x80\xAF") == U"\uFFFD\uFFFD\uFFFD");           // overlong, 3 bytes
	CHECK(fromUtf8("\xED\xA0\x80") == U"\uFFFD\uFFFD\uFFFD");           // a surrogate
	CHECK(fromUtf8("\xF4\x90\x80\x80") == U"\uFFFD\uFFFD\uFFFD\uFFFD"); // above U+10FFFF
	CHECK(fromUtf8("\xE2\x82") == U"\uFFFD");                           // truncated: one replacement
	CHECK(fromUtf8("\xE2\x82"
	               "A") == U"\uFFFDA"); // the next character survives
	CHECK(fromUtf8("\x80\xBF"
	               "a") == U"\uFFFD\uFFFDa");                                     // stray continuation bytes
	CHECK(fromUtf8("\xF8\x88\x80\x80\x80") == U"\uFFFD\uFFFD\uFFFD\uFFFD\uFFFD"); // 5-byte forms don't exist
}

TEST_CASE("text: UTF-8 encoding round-trips every kind of code point") {
	using affresco::detail::fromUtf8;
	using affresco::detail::toUtf8;

	const std::u32string text = U"AéΩЖ€✓\U0001F600\U0010FFFF";
	CHECK(fromUtf8(toUtf8(text)) == text);
	CHECK(toUtf8(std::u32string(1, char32_t{0xD800})) == "\xEF\xBF\xBD"); // a lone surrogate can't be encoded: U+FFFD
}

TEST_CASE("text: Latin-1 is rasterised when the font loads, anything else on first use") {
	const auto font = roboto(15.0f, false);
	REQUIRE(font->atlasPages() == 1);

	for(char32_t c = U' '; c <= U'\u00FF'; ++c) (void)font->glyph(c);
	CHECK(font->atlasVersion(0) == 0); // nothing added: Latin-1 was already there

	const Font::Glyph& omega = font->glyph(U'Ω');
	CHECK(font->atlasVersion(0) == 1);
	CHECK(&font->glyph(U'Ω') == &omega); // the second lookup finds the same glyph
	CHECK(font->atlasVersion(0) == 1);
	CHECK(omega.advance > 0.0f);
	CHECK(omega.quad.right > omega.quad.left);

	const Font::AtlasRegion added = font->atlasChangesSince(0, 0);
	CHECK(!added.empty());
	CHECK(covered(*font, 0, added));
	CHECK(font->atlasChangesSince(0, 1).empty());

	// Measuring counts code points, whichever way they were rasterised
	const std::u32string_view word = U"Ωμéж";
	float sum = 0.0f;
	for(const char32_t c : word) sum += font->glyph(c).advance;
	CHECK(font->measure(word) == doctest::Approx(sum));
}

TEST_CASE("text: a character the font lacks comes from its first fallback that has it, else '?'") {
	const auto plain = roboto(15.0f, false);
	const auto withFallback = roboto(15.0f, true);
	const Font::Glyph& question = plain->glyph(U'?');

	CHECK_FALSE(plain->hasGlyph(U'✓'));
	CHECK(&plain->glyph(U'✓') == &question); // no fallback: '?'
	CHECK(plain->atlasVersion(0) == 0);

	REQUIRE(withFallback->hasGlyph(U'✓'));
	const Font::Glyph& check = withFallback->glyph(U'✓');
	CHECK(&check != &withFallback->glyph(U'?'));
	CHECK(check.quad.right > check.quad.left);
	CHECK(withFallback->atlasVersion(check.page) == 1); // in the font's own pages: the same batch as its text

	// The font's own glyphs come first: Roboto has 'A' and Omega
	CHECK(withFallback->glyph(U'A').advance == plain->glyph(U'A').advance);
	CHECK(withFallback->glyph(U'Ω').advance == plain->glyph(U'Ω').advance);

	CHECK_FALSE(withFallback->hasGlyph(U'★')); // in neither font
	CHECK(&withFallback->glyph(U'★') == &withFallback->glyph(U'?'));
	CHECK(&withFallback->glyph(U'\t') == &withFallback->glyph(U'?')); // control characters

	// Beyond U+10FFFF: '?', never handed to the font's tables
	for(const char32_t beyond : {char32_t{0x110000}, char32_t{0x80000000}, char32_t{0xFFFFFFFF}}) {
		CHECK_FALSE(withFallback->hasGlyph(beyond));
		CHECK(&withFallback->glyph(beyond) == &withFallback->glyph(U'?'));
	}
}

TEST_CASE("text: glyphs that don't fit the first page go to a new one") {
	const auto font = roboto(48.0f, false);
	const int firstSize = font->atlasSize(0);

	std::size_t added = 0;
	for(char32_t c = 0x100; c <= 0x52F; ++c) // Latin Extended, Greek, Cyrillic
		if(font->hasGlyph(c)) {
			(void)font->glyph(c);
			++added;
		}
	REQUIRE(added > 500);

	REQUIRE(font->atlasPages() >= 2);
	CHECK(font->atlasSize(0) == firstSize); // pages never resize: glyphs keep their coordinates
	CHECK(font->atlasSize(1) >= 1024);
	CHECK(font->atlasVersion(1) > 0);
	CHECK(covered(*font, 1, font->atlasChangesSince(1, 0)));

	const auto onLaterPage = [&](char32_t c) { return font->glyph(c).page > 0; };
	bool any = false;
	for(char32_t c = 0x100; c <= 0x52F && !any; ++c) any = font->hasGlyph(c) && onLaterPage(c);
	CHECK(any);

	// Each page keeps a white block for solid rectangles
	const affresco::Vec2 white = font->whiteTexel(1);
	const auto size = static_cast<std::size_t>(font->atlasSize(1));
	CHECK(white.x == doctest::Approx(1.0f / static_cast<float>(size)));
	CHECK(font->atlasPixels(1)[size + 1] == 255);
}

TEST_CASE("text: the atlas stops growing at its limit, and characters past it draw as '?'") {
	const auto font = roboto(300.0f, false); // glyphs this large reach the limit after several hundred
	REQUIRE(font->atlasSize(0) == 4096);

	std::size_t questions = 0;
	for(char32_t c = 0x100; c <= 0xFFFF; ++c)
		if(font->hasGlyph(c) && drawsAsQuestionMark(*font, c)) ++questions;
	CHECK(questions > 0);

	std::size_t texels = 0;
	for(std::size_t page = 0; page < font->atlasPages(); ++page)
		texels += static_cast<std::size_t>(font->atlasSize(page)) * static_cast<std::size_t>(font->atlasSize(page));
	CHECK(texels <= std::size_t{64} << 20);
}

TEST_CASE("text: a screen's font names its fallbacks, and wrong names are errors that name the place") {
	const affresco::format::Rules rules;

	affresco::vcs::Repository good(rules, screenWithFallback(json::array({"mono"})));
	const affresco::format::Runtime runtime(good, fonts());
	CHECK(runtime.font("default")->hasGlyph(U'✓'));
	CHECK_FALSE(runtime.font("mono")->hasGlyph(U'★'));

	affresco::vcs::Repository unknown(rules, screenWithFallback(json::array({"symbols"})));
	CHECK_THROWS_WITH_AS(affresco::format::Runtime(unknown, fonts()),
	                     doctest::Contains("fonts.default.fallback[0]: unknown font 'symbols'"),
	                     affresco::DocumentError);

	affresco::vcs::Repository notList(rules, screenWithFallback("mono"));
	CHECK_THROWS_WITH_AS(affresco::format::Runtime(notList, fonts()), doctest::Contains("fonts.default.fallback"),
	                     affresco::DocumentError);

	affresco::vcs::Repository tooMany(rules, screenWithFallback(json::array({"mono", "mono", "mono", "mono", "mono",
	                                                                         "mono", "mono", "mono", "mono"})));
	CHECK_THROWS_WITH_AS(affresco::format::Runtime(tooMany, fonts()), doctest::Contains("at most 8 fallback fonts"),
	                     affresco::DocumentError);
}
