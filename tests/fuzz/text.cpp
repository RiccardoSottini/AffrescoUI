/// @file
/// @brief Fuzzes shaping (render's Font, with HarfBuzz and FreeType): a revision or a peer can put any text in a
/// label, and the screen shapes, measures, wraps and draws it.
///
/// The input is UTF-8 text (invalid sequences become U+FFFD, as the labels decode them). Its first byte, when there is
/// one, chooses the width it is wrapped to. The font is Roboto with the rendering check's subsets as fallbacks
/// (Arabic, Devanagari, Chinese, colour emoji), so every path is taken: words, right-to-left runs, fallbacks, marks,
/// colour glyphs. It is drawn too, below a display scale of 2 (fitted to pixel columns, Font::fitGlyph()) and at it.

#include "Fuzz.hpp"

#include "detail/Utf8.hpp"
#include "paint/PaintRecorder.hpp"
#include "paint/RetainedGeometry.hpp"

#include <affresco/render/Font.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
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

/// @brief Checks what drawing one text promises at some display scales: its glyphs' quads are finite and, below a scale
/// of 2, start on a pixel column.
/// @param text The text.
void checkDrawn(std::u32string_view text) {
	for(const float scale : {1.0f, 1.25f, 1.5f, 2.0f}) {
		affresco::detail::PaintRecord record;
		const affresco::Rect clip{0.0f, 0.0f, 4000.0f, 400.0f};
		affresco::detail::PaintRecorder recorder(record, clip, clip, scale);
		recorder.drawText(font(), text, {10.3f, 40.0f}, affresco::colors::black);
		for(const affresco::detail::PaintOp& op : record.ops) {
			if(op.step != affresco::detail::PaintStep::glyphs) continue;
			for(std::uint32_t quad = op.firstQuad; quad < op.firstQuad + op.quads; ++quad) {
				float left = std::numeric_limits<float>::infinity();
				for(std::size_t index = 0; index < affresco::detail::verticesPerQuad; ++index) {
					const affresco::detail::Vertex& vertex =
					    record.vertices[quad * affresco::detail::verticesPerQuad + index];
					if(!std::isfinite(vertex.x) || !std::isfinite(vertex.y)) affresco::fuzz::broken("glyphs drawn finite");
					left = std::min(left, vertex.x);
				}
				const float pixels = left * scale;
				if(scale < 2.0f && std::abs(pixels - std::round(pixels)) > 0.01f + 1e-6f * std::abs(pixels))
					affresco::fuzz::broken("below 2, glyphs start on a pixel column");
			}
		}
	}
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
	checkDrawn(text);
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
