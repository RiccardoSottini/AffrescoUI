/// @file
/// @brief Unit tests for shaped text (render's Font with HarfBuzz and FreeType): ligatures, kerning, marks, Arabic,
/// fallback by coverage, colour emoji, caches and wrapping, and font files that aren't fonts.
///
/// The fonts are the examples' (AFFRESCOUI_FONTS_DIR: Roboto, JetBrains Mono) and the rendering check's subsets
/// (AFFRESCOUI_TEST_FONTS_DIR: Noto Sans Arabic and SC, Twemoji), which hold only the characters its screens show.

#include <affresco/render/Font.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

using affresco::AtlasKind;
using affresco::Font;

namespace {

/// @brief The examples' fonts.
std::filesystem::path fonts() { return AFFRESCOUI_FONTS_DIR; }
/// @brief The rendering check's font subsets.
std::filesystem::path testFonts() { return AFFRESCOUI_TEST_FONTS_DIR; }

/// @brief Roboto at 16 px, with fallbacks.
/// @param fallbacks Files in the rendering check's fonts.
/// @return The font.
std::shared_ptr<const Font> roboto(std::initializer_list<const char*> fallbacks = {}) {
	std::vector<std::filesystem::path> files;
	for(const char* name : fallbacks) files.push_back(testFonts() / name);
	return Font::load(fonts() / "Roboto-Regular.ttf", 16.0f, files);
}

/// @brief The sum of shaped text's advances.
/// @param shaped The shaped text.
float sumOfAdvances(const Font::ShapedText& shaped) {
	float sum = 0.0f;
	for(const float advance : shaped.advances) sum += advance;
	return sum;
}

} // namespace

TEST_CASE("shaping: a ligature is one glyph, its width goes to its first letter, and its carets are spread") {
	const auto font = roboto();
	const Font::ShapedText shaped = font->shape(U"office"); // Roboto has "fi" (and "ffi" as f + fi)

	CHECK(shaped.glyphs.size() == 5); // o f fi c e
	REQUIRE(shaped.advances.size() == 6);
	REQUIRE(shaped.carets.size() == 7);
	CHECK(shaped.advances[2] > 0.0f);  // f of "fi"
	CHECK(shaped.advances[3] == 0.0f); // i of "fi": its width went to the f
	CHECK(shaped.carets[3] > shaped.carets[2]);
	CHECK(shaped.carets[3] < shaped.carets[4]); // between f and i, inside the ligature
	CHECK(shaped.carets[6] == doctest::Approx(shaped.width));
	CHECK(sumOfAdvances(shaped) == doctest::Approx(shaped.width));
	CHECK(font->measure(U"office") == doctest::Approx(shaped.width));
}

TEST_CASE("shaping: kerning moves letters closer than their own advances") {
	const auto font = roboto();
	CHECK(font->measure(U"To") < font->glyph(U'T').advance + font->glyph(U'o').advance - 0.3f); // 99 font units
	CHECK(font->measure(U"oo") == doctest::Approx(2.0f * font->glyph(U'o').advance));           // no kerning there
}

TEST_CASE("shaping: a combining mark joins its letter's cluster, placed over it, or composes with it") {
	const auto font = roboto();
	const Font::ShapedText shaped = font->shape(U"x́"); // no precomposed x with an acute
	REQUIRE(shaped.glyphs.size() == 2);
	CHECK(shaped.advances[1] == 0.0f);
	CHECK(shaped.width == doctest::Approx(font->glyph(U'x').advance));
	CHECK(shaped.glyphs[1].x + shaped.glyphs[1].glyph->quad.left < shaped.width); // over the x, not after it

	const Font::ShapedText composed = font->shape(U"é"); // the font's é
	REQUIRE(composed.glyphs.size() == 1);
	CHECK(composed.glyphs[0].glyph->uv == font->glyph(U'é').uv); // the same texels as the unshaped é
	CHECK(composed.advances[1] == 0.0f);
}

TEST_CASE("shaping: Arabic letters join, and the run goes right to left") {
	const auto font = roboto({"NotoSansArabic-Subset.ttf"});
	const Font::ShapedText alone = font->shape(U"س");
	const Font::ShapedText word = font->shape(U"سلام"); // peace: four letters, joined

	REQUIRE_FALSE(alone.glyphs.empty());
	REQUIRE(word.glyphs.size() >= 3);
	// Right to left: the first letter is drawn last (rightmost), in its initial form, not the one standing alone
	const Font::Glyph* first = word.glyphs.back().glyph;
	CHECK(first != alone.glyphs.front().glyph);
	CHECK(word.carets[0] > word.carets[3]); // carets go from right to left
	CHECK(word.carets[0] == doctest::Approx(word.width));
	CHECK(word.carets[4] == doctest::Approx(0.0f)); // after the last letter: the left end
	for(const Font::ShapedGlyph& glyph : word.glyphs) CHECK(glyph.glyph != &font->glyph(U'?'));

	// Words of a right-to-left run keep their order: the first word is on the right
	const Font::ShapedText phrase = font->shape(U"سلام سلام");
	CHECK(phrase.carets[0] > phrase.carets[5]);
}

TEST_CASE("shaping: each character comes from the first font that has it, at the font's em size") {
	const auto font = roboto({"NotoSansArabic-Subset.ttf", "NotoSansSC-Subset.ttf"});
	CHECK(font->hasGlyph(U'你'));
	CHECK_FALSE(roboto()->hasGlyph(U'你'));

	const Font::ShapedText mixed = font->shape(U"Hi 你好");
	REQUIRE(mixed.glyphs.size() == 4);
	for(const Font::ShapedGlyph& glyph : mixed.glyphs) CHECK(glyph.glyph != &font->glyph(U'?'));
	// A Chinese character is one em wide, Roboto's em: its 16 px span 2,701 of its 2,048 units per em, so 12.1 px
	CHECK(mixed.advances[3] == doctest::Approx(16.0f * 2048.0f / 2701.0f).epsilon(0.01));

	// No font has it: '?'
	const Font::ShapedText missing = font->shape(U"★");
	REQUIRE(missing.glyphs.size() == 1);
	CHECK(missing.glyphs[0].glyph == &font->glyph(U'?'));
	CHECK(missing.width == doctest::Approx(font->glyph(U'?').advance));
}

TEST_CASE("shaping: colour emoji go to a colour page, and a sequence is one glyph") {
	const auto font = roboto({"TwemojiMozilla-Subset.ttf"});
	CHECK(font->atlasPages(AtlasKind::color) == 0);

	const Font::ShapedText palette = font->shape(U"a🎨");
	REQUIRE(palette.glyphs.size() == 2);
	CHECK_FALSE(palette.glyphs[0].glyph->color);
	CHECK(palette.glyphs[1].glyph->color);
	REQUIRE(font->atlasPages(AtlasKind::color) == 1);
	const auto size = static_cast<std::size_t>(font->atlasSize(0, AtlasKind::color));
	CHECK(font->atlasPixels(0, AtlasKind::color).size() == size * size * 4);
	CHECK(font->atlasVersion(0, AtlasKind::color) == 1);
	CHECK_FALSE(font->atlasChangesSince(0, 0, AtlasKind::color).empty());

	// Its texels have colour: red, green and blue differ somewhere
	const std::span<const std::uint8_t> texels = font->atlasPixels(0, AtlasKind::color);
	bool colorful = false;
	for(std::size_t i = 0; i + 3 < texels.size() && !colorful; i += 4)
		colorful = texels[i + 3] > 0 && (texels[i] != texels[i + 1] || texels[i + 1] != texels[i + 2]);
	CHECK(colorful);

	// A family (three emoji joined by U+200D), a flag (two regional indicators), a skin tone: one glyph each
	for(const std::u32string_view sequence :
	    {std::u32string_view(U"👨‍👩‍👧"), std::u32string_view(U"🇮🇹"), std::u32string_view(U"👍🏽")}) {
		const Font::ShapedText shaped = font->shape(sequence);
		CHECK(shaped.glyphs.size() == 1);
		CHECK(shaped.glyphs[0].glyph->color);
		CHECK(shaped.advances[0] == doctest::Approx(shaped.width));
	}
}

TEST_CASE("shaping: shaped text is cached by its text, and the caches stay bounded") {
	const auto font = roboto();
	const Font::ShapedText& first = font->shape(U"Backlog");
	CHECK(&font->shape(U"Backlog") == &first);
	const float width = first.width; // the reference is only valid until other text is shaped

	// Many more texts than the caches keep: shaping still gives the same result, and memory stays bounded
	const std::size_t before = font->memoryBytes();
	for(char32_t i = 0; i < 20000; ++i) {
		std::u32string text = U"line ";
		text += static_cast<char32_t>(U'a' + i % 26);
		text.append(i % 7, U'x');
		text += static_cast<char32_t>(0x4E00 + i); // never the same twice
		(void)font->shape(text);
	}
	CHECK(font->shape(U"Backlog").width == doctest::Approx(width));
	CHECK(font->memoryBytes() < before + (std::size_t{32} << 20));
}

TEST_CASE("shaping: wrapping breaks at spaces by shaped widths, and never inside a cluster") {
	const auto font = roboto();
	const std::u32string_view text = U"office office";
	const std::vector<std::u32string_view> lines = font->wrap(text, font->measure(U"office ") + 1.0f);
	REQUIRE(lines.size() == 2);
	CHECK(lines[0] == U"office"); // the space it broke at is dropped
	CHECK(lines[1] == U"office");

	// A word wider than the line breaks inside it, but not between the f and the i of the ligature (offset 3)
	const auto widest = static_cast<int>(font->measure(U"office"));
	for(int width = 4; width < widest; ++width)
		for(const std::u32string_view line : font->wrap(text.substr(0, 6), static_cast<float>(width)))
			CHECK(line.data() + line.size() != text.data() + 3);

	// '\n' still breaks lines, and an empty paragraph is an empty line
	const std::vector<std::u32string_view> paragraphs = font->wrap(U"a\n\nb", 1000.0f);
	REQUIRE(paragraphs.size() == 3);
	CHECK(paragraphs[1].empty());
}

TEST_CASE("shaping: measuring by words gives what shaping the whole line gives") {
	const auto font = roboto({"NotoSansSC-Subset.ttf"});
	for(const std::u32string_view text :
	    {std::u32string_view(U"Keyboard shortcuts for moving cards"), std::u32string_view(U"  two  spaces "),
	     std::u32string_view(U"你好， 世界。 office"), std::u32string_view(U"")}) {
		CHECK(font->measure(text) == doctest::Approx(font->shape(text).width));
		CHECK(sumOfAdvances(font->shape(text)) == doctest::Approx(font->shape(text).width));
	}
}

TEST_CASE("fonts: a path that isn't a regular font file is refused, naming it") {
	CHECK_THROWS_WITH_AS((void)Font::load(fonts(), 16.0f), doctest::Contains("could not open font file"),
	                     std::runtime_error);
	CHECK_THROWS_WITH_AS((void)Font::load(fonts() / "README.md", 16.0f), doctest::Contains("not a usable font"),
	                     std::runtime_error);
	CHECK_THROWS_AS((void)Font::load(fonts() / "missing.ttf", 16.0f), std::runtime_error);
}

// Each character comes from the other face, so every one is a run of its own, and none has a script of its own: the
// direction of each used to be looked for in all the runs after it
TEST_CASE("shaping: text that alternates faces costs in proportion to its length" * doctest::timeout(30)) {
	const std::array<std::filesystem::path, 1> mono{fonts() / "JetBrainsMono-Regular.ttf"};
	const auto alone = Font::load(fonts() / "Roboto-Regular.ttf", 16.0f);
	const auto font = Font::load(fonts() / "Roboto-Regular.ttf", 16.0f, mono);
	constexpr char32_t robotoOnly = U'\u2000', monoOnly = U'\u20BF'; // an en quad, and the bitcoin sign
	REQUIRE(alone->hasGlyph(robotoOnly));
	REQUIRE_FALSE(alone->hasGlyph(monoOnly));
	REQUIRE(font->hasGlyph(monoOnly));

	std::u32string text;
	for(int i = 0; i < 200000; ++i) text += {robotoOnly, monoOnly};
	const float pair = font->measure(U"\u2000") + font->measure(U"\u20BF");
	CHECK(font->measure(text) == doctest::Approx(200000.0f * pair).epsilon(0.01)); // float sums lose a little
}
