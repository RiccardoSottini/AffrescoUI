/// @file
/// @brief Unit tests for the fast path of plain text (render's text/PlainText): a word that needs no shaping is laid
/// out from advances and kerning pairs, and its glyphs, advances, carets and widths equal HarfBuzz's to the last bit.
///
/// Each comparison loads a font twice: one with the fast path, one with it turned off, so everything goes through
/// HarfBuzz. The text is every string of the templates, and generated words: every pair of Latin-1 characters, and
/// random words of them. The fonts are the examples' (AFFRESCOUI_FONTS_DIR) and the rendering check's subsets
/// (AFFRESCOUI_TEST_FONTS_DIR).

#include <affresco/core/Json.hpp>
#include <affresco/render/Font.hpp>

#include "detail/Utf8.hpp"
#include "text/FontInspector.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using affresco::Font;
using affresco::detail::FontInspector;

namespace {

/// @brief The examples' fonts.
std::filesystem::path fonts() { return AFFRESCOUI_FONTS_DIR; }
/// @brief The rendering check's font subsets.
std::filesystem::path testFonts() { return AFFRESCOUI_TEST_FONTS_DIR; }

/// @brief A font loaded twice: with the fast path, and with every word shaped by HarfBuzz.
struct FontPair {
	std::shared_ptr<const Font> fast;   ///< With the fast path.
	std::shared_ptr<const Font> shaped; ///< Without it.

	/// @brief Loads both.
	/// @param path The font file.
	/// @param size Its pixel height.
	FontPair(const std::filesystem::path& path, float size)
	    : fast(Font::load(path, size)), shaped(Font::load(path, size)) {
		FontInspector::setPlainTextEnabled(*shaped, false);
	}
};

/// @brief Collects every string of a JSON value, split at line breaks.
/// @param value The value.
/// @param lines Gets them.
void collectStrings(const affresco::json& value, std::vector<std::u32string>& lines) {
	if(value.is_string()) {
		std::stringstream text(value.get<std::string>());
		for(std::string line; std::getline(text, line);) lines.push_back(affresco::detail::fromUtf8(line));
	} else if(value.is_structured()) {
		for(const auto& item : value) collectStrings(item, lines);
	}
}

/// @brief Every string of the templates, line by line.
std::vector<std::u32string> templateText() {
	std::vector<std::u32string> lines;
	for(const auto& entry : std::filesystem::directory_iterator(AFFRESCOUI_SCREENS_DIRECTORY)) {
		if(entry.path().extension() != ".json") continue;
		std::ifstream file(entry.path());
		collectStrings(affresco::json::parse(file), lines);
	}
	return lines;
}

/// @brief Words of Latin-1: every pair of characters from '!' to U+00FF, then random words of 1 to 12 of them, in
/// lines of 1 to 6 words.
std::vector<std::u32string> generatedText() {
	std::vector<std::u32string> lines;
	std::u32string line;
	for(char32_t first = 0x21; first <= 0xFF; ++first) {
		for(char32_t second = 0x21; second <= 0xFF; ++second) line += {first, second, U' '};
		lines.push_back(std::move(line));
		line.clear();
	}
	std::uint32_t random = 26; // a linear congruential generator: the same words everywhere
	const auto next = [&random](std::uint32_t range) {
		random = random * 1664525u + 1013904223u;
		return (random >> 8) % range;
	};
	for(int i = 0; i < 3000; ++i) {
		for(std::uint32_t word = 0, words = 1 + next(6); word < words; ++word) {
			if(word) line += U' ';
			for(std::uint32_t length = 1 + next(12); length > 0; --length)
				line +=
				    next(4) == 0 ? static_cast<char32_t>(0xA0 + next(0x60)) : static_cast<char32_t>(0x21 + next(0x5E));
		}
		lines.push_back(std::move(line));
		line.clear();
	}
	return lines;
}

/// @brief Checks that a line comes out the same from both fonts: shaped, measured and wrapped.
/// @param pair The fonts.
/// @param line The line.
void checkSame(const FontPair& pair, std::u32string_view line) {
	const Font::ShapedText fast = pair.fast->shape(line); // copies: the next call replaces what shape() returns
	const Font::ShapedText& shaped = pair.shaped->shape(line);
	CAPTURE(affresco::detail::toUtf8(line));
	REQUIRE(fast.glyphs.size() == shaped.glyphs.size());
	for(std::size_t i = 0; i < fast.glyphs.size(); ++i) {
		// Each font has its own atlas, so the glyphs are compared by what they draw, not by where they are stored
		CHECK(fast.glyphs[i].glyph->quad == shaped.glyphs[i].glyph->quad);
		CHECK(fast.glyphs[i].glyph->advance == shaped.glyphs[i].glyph->advance);
		CHECK(fast.glyphs[i].x == shaped.glyphs[i].x);
		CHECK(fast.glyphs[i].y == shaped.glyphs[i].y);
	}
	CHECK(fast.advances == shaped.advances);
	CHECK(fast.carets == shaped.carets);
	CHECK(fast.width == shaped.width);
	CHECK(pair.fast->measure(line) == pair.shaped->measure(line));
	CHECK(pair.fast->wrap(line, 120.0f) == pair.shaped->wrap(line, 120.0f));
}

/// @brief Checks a font against itself without the fast path, over the templates' text and the generated words.
/// @param path The font file.
/// @param size Its pixel height.
void checkFont(const std::filesystem::path& path, float size) {
	const FontPair pair(path, size);
	for(const std::u32string& line : templateText()) checkSame(pair, line);
	for(const std::u32string& line : generatedText()) checkSame(pair, line);
}

} // namespace

TEST_CASE("plain text: Roboto's plain words equal HarfBuzz's, at two sizes") {
	checkFont(fonts() / "Roboto-Regular.ttf", 15.0f);
	checkFont(fonts() / "Roboto-Regular.ttf", 23.5f);
}

TEST_CASE("plain text: JetBrains Mono's plain words equal HarfBuzz's, around its code ligatures") {
	checkFont(fonts() / "JetBrainsMono-Regular.ttf", 14.0f);
}

TEST_CASE("plain text: a subset font with few Latin characters equals HarfBuzz's") {
	checkFont(testFonts() / "NotoSansArabic-Subset.ttf", 16.0f);
}

TEST_CASE("plain text: which words take the fast path") {
	const auto roboto = Font::load(fonts() / "Roboto-Regular.ttf", 16.0f);
	CHECK(FontInspector::plainCharacters(*roboto) > 150);
	CHECK(FontInspector::isPlain(*roboto, U"Onboarding"));
	CHECK(FontInspector::isPlain(*roboto, U"To")); // kerned: a pair HarfBuzz only moves
	CHECK(FontInspector::isPlain(*roboto, U"café,"));
	CHECK(FontInspector::isPlain(*roboto, U"12:30"));             // no script: shaped with the default one
	CHECK_FALSE(FontInspector::isPlain(*roboto, U"office"));      // "fi" is a ligature
	CHECK_FALSE(FontInspector::isPlain(*roboto, U"x́"));           // a mark
	CHECK_FALSE(FontInspector::isPlain(*roboto, U"Ω"));           // beyond Latin-1
	CHECK_FALSE(FontInspector::isPlain(*roboto, U"soft­hyphen")); // HarfBuzz hides it

	const auto mono = Font::load(fonts() / "JetBrainsMono-Regular.ttf", 14.0f);
	CHECK_FALSE(FontInspector::isPlain(*mono, U"->")); // a code ligature
	CHECK(FontInspector::isPlain(*mono, U"value"));
}

TEST_CASE("plain text: most words of the templates take the fast path, so the comparisons above test it") {
	const auto roboto = Font::load(fonts() / "Roboto-Regular.ttf", 15.0f);
	std::size_t words = 0, plain = 0;
	for(const std::u32string& line : templateText()) {
		for(std::size_t start = 0; start < line.size();) {
			const std::size_t end = std::min(line.find(U' ', start), line.size());
			if(end > start) {
				++words;
				if(FontInspector::isPlain(*roboto, std::u32string_view(line).substr(start, end - start))) ++plain;
			}
			start = end + 1;
		}
	}
	MESSAGE(plain, " of ", words, " words are plain");
	CHECK(plain * 10 >= words * 9); // 98% when this was written
}

TEST_CASE("plain text: turning the fast path off makes every word shaped, and memory counts its tables") {
	const auto font = Font::load(fonts() / "Roboto-Regular.ttf", 16.0f);
	const std::size_t before = font->memoryBytes();
	(void)font->measure(U"Kerning: To, AV, Wa."); // shapes a few pairs: their table is made
	CHECK(font->memoryBytes() > before);

	FontInspector::setPlainTextEnabled(*font, false);
	CHECK_FALSE(FontInspector::isPlain(*font, U"Onboarding"));
	FontInspector::setPlainTextEnabled(*font, true);
	CHECK(FontInspector::isPlain(*font, U"Onboarding"));
}
