/// @file
/// @brief Fuzzes shaping (render's Font, with HarfBuzz and FreeType): a revision or a peer can put any text in a
/// label, and the screen shapes, measures, wraps and draws it.
///
/// The input is UTF-8 text (invalid sequences become U+FFFD, as the labels decode them). Its first byte, when there is
/// one, chooses the width it is wrapped to. The font is Roboto with the rendering check's subsets as fallbacks
/// (Arabic, Devanagari, Chinese, colour emoji), so every path is taken: words, right-to-left runs, fallbacks, marks,
/// colour glyphs.

#include "Fuzz.hpp"

#include "detail/Utf8.hpp"

#include <affresco/render/Font.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace {

/// @brief The font, loaded once.
/// @return It.
const affresco::Font& font() {
	static const std::shared_ptr<const affresco::Font> loaded = [] {
		// In the checkout, wherever the target runs from
		const std::filesystem::path root = AFFRESCOUI_FUZZ_CHECKOUT;
		const std::filesystem::path fonts = root / "examples/shared/fonts", subsets = root / "tests/render/fonts";
		const std::array fallbacks{subsets / "NotoSansArabic-Subset.ttf", subsets / "NotoSansDevanagari-Subset.ttf",
		                           subsets / "NotoSansSC-Subset.ttf", subsets / "TwemojiMozilla-Subset.ttf"};
		return affresco::Font::load(fonts / "Roboto-Regular.ttf", 16.0f, fallbacks);
	}();
	return *loaded;
}

/// @brief Checks what shaping promises about one text.
/// @param text The text.
void checkShaped(std::u32string_view text) {
	const affresco::Font::ShapedText& shaped = font().shape(text);
	if(shaped.advances.size() != text.size()) affresco::fuzz::broken("one advance per code point");
	if(shaped.carets.size() != text.size() + 1) affresco::fuzz::broken("one caret per code point, and one more");
	float sum = 0.0f;
	for(const float advance : shaped.advances) {
		if(!std::isfinite(advance)) affresco::fuzz::broken("advances are finite");
		sum += advance;
	}
	if(std::abs(sum - shaped.width) > 0.01f * (1.0f + shaped.width))
		affresco::fuzz::broken("advances sum to the width");
	for(const float caret : shaped.carets)
		if(!std::isfinite(caret)) affresco::fuzz::broken("carets are finite");
	for(const affresco::Font::ShapedGlyph& glyph : shaped.glyphs)
		if(!glyph.glyph || !std::isfinite(glyph.x) || !std::isfinite(glyph.y)) affresco::fuzz::broken("glyphs placed");
}

} // namespace

extern "C" int LLVMFuzzerInitialize(int* /*argc*/, char*** /*argv*/) {
	affresco::fuzz::silenceLog();
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	const float width = size > 0 ? 8.0f + static_cast<float>(data[0]) * 4.0f : 240.0f;
	const std::u32string text = affresco::detail::fromUtf8(affresco::fuzz::text(data, size));

	checkShaped(text);
	if(std::abs(font().measure(text) - font().shape(text).width) > 0.01f * (1.0f + font().shape(text).width))
		affresco::fuzz::broken("measuring gives the shaped width");

	// Lines are views into the text, in order, and only spaces and line breaks are left out between them
	std::size_t next = 0;
	for(const std::u32string_view line : font().wrap(text, width)) {
		const auto start = static_cast<std::size_t>(line.data() - text.data());
		if(start < next || start > next + 1 || start + line.size() > text.size())
			affresco::fuzz::broken("lines in order");
		if(start == next + 1 && text[next] != U' ' && text[next] != U'\n')
			affresco::fuzz::broken("only a break is cut");
		next = start + line.size();
		checkShaped(line);
	}
	return 0;
}
