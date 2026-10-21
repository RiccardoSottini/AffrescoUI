/// @file
/// @brief Fuzzes rich text layout (render's TextLayout): a revision or a peer can give a label any runs, and the
/// screen breaks them into lines across fonts, hit tests them and places carets in them.
///
/// The input is UTF-8 text cut into runs at U+001F (the unit separator). The first byte chooses the width the runs are
/// wrapped to and their alignment; each run's first byte (after the separator) its style: bit 0 bold, bit 1 a larger
/// size, bit 2 italic, bit 3 a link. The fonts are Roboto, so every path of breaking, merging and placing is taken.

#include "Fuzz.hpp"

#include <affresco/render/RichText.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace {

/// @brief A font of Roboto, loaded once per size and style.
/// @param variant Bit 0 bold, bit 1 larger, bit 2 italic.
/// @return It.
std::shared_ptr<const affresco::Font> font(unsigned variant) {
	static const std::array<std::shared_ptr<const affresco::Font>, 8> loaded = [] {
		// In the checkout, wherever the target runs from
		const std::filesystem::path file =
		    std::filesystem::path(AFFRESCOUI_FUZZ_CHECKOUT) / "examples/assets/fonts/Roboto-Regular.ttf";
		std::array<std::shared_ptr<const affresco::Font>, 8> fonts;
		for(unsigned index = 0; index < fonts.size(); ++index)
			fonts[index] = affresco::Font::load(file, (index & 2u) != 0 ? 28.0f : 16.0f, {},
			                                    affresco::FontStyle{(index & 1u) != 0, (index & 4u) != 0});
		return fonts;
	}();
	return loaded[variant & 7u];
}

/// @brief Cuts the input into runs.
/// @param text The input as text, without its first byte.
/// @return The runs.
std::vector<affresco::TextRun> runsOf(std::string_view text) {
	std::vector<affresco::TextRun> runs;
	while(!text.empty() && runs.size() < 256) {
		const std::size_t end = text.find('\x1f');
		std::string_view piece = text.substr(0, end);
		affresco::TextRun run;
		if(!piece.empty()) {
			const auto style = static_cast<unsigned char>(piece.front());
			if((style & 7u) != 0) run.font = font(style);
			if((style & 8u) != 0) run.link = "link";
			piece.remove_prefix(1);
		}
		run.text = affresco::codepointsOf(piece);
		runs.push_back(std::move(run));
		text = end == std::string_view::npos ? std::string_view{} : text.substr(end + 1);
	}
	return runs;
}

/// @brief Checks what the lines promise: they cover the text in order, their pieces lie in them and stack up.
/// @param layout The layout.
void checkLines(const affresco::TextLayout& layout) {
	std::size_t next = 0, piece = 0;
	float top = 0.0f;
	for(const affresco::TextLayout::Line& line : layout.lines()) {
		if(line.begin < next || line.begin > line.end || line.end > layout.text().size())
			affresco::fuzz::broken("lines in order, within the text");
		if(line.firstPiece != piece || line.endPiece < line.firstPiece || line.endPiece > layout.pieces().size())
			affresco::fuzz::broken("pieces line by line");
		if(std::abs(line.top - top) > 0.01f * (1.0f + top) || !(line.height >= 0.0f))
			affresco::fuzz::broken("lines stack up");
		for(std::size_t index = line.firstPiece; index < line.endPiece; ++index) {
			const affresco::TextLayout::Piece& part = layout.pieces()[index];
			if(part.begin < line.begin || part.end < part.begin || !std::isfinite(part.x) || !std::isfinite(part.width))
				affresco::fuzz::broken("pieces in their line, placed");
		}
		next = line.end;
		piece = line.endPiece;
		top += line.height;
	}
	if(piece != layout.pieces().size()) affresco::fuzz::broken("every piece on a line");
}

/// @brief Checks hit testing and carets at a few points.
/// @param layout The layout.
void checkHits(const affresco::TextLayout& layout) {
	const std::array points{affresco::Vec2{-5.0f, -5.0f}, affresco::Vec2{3.0f, 3.0f},
	                        affresco::Vec2{layout.width() / 2.0f, layout.height() / 2.0f},
	                        affresco::Vec2{layout.width() + 5.0f, layout.height() + 5.0f}};
	for(const affresco::Vec2 point : points) {
		if(layout.positionAt(point) > layout.text().size()) affresco::fuzz::broken("positions within the text");
		if(const auto hit = layout.pieceAt(point); hit && *hit >= layout.pieces().size())
			affresco::fuzz::broken("hits are pieces");
	}
	for(const affresco::TextLayout::Line& line : layout.lines())
		if(!std::isfinite(layout.caretX(line, line.begin)) || !std::isfinite(layout.caretX(line, line.end)))
			affresco::fuzz::broken("carets are finite");
}

} // namespace

extern "C" int LLVMFuzzerInitialize(int* /*argc*/, char*** /*argv*/) {
	affresco::fuzz::silenceLog();
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	const std::string_view text = affresco::fuzz::text(data, size);
	const unsigned first = size > 0 ? data[0] : 0u;
	const float width = 8.0f + static_cast<float>(first >> 2) * 8.0f;
	const auto align = static_cast<affresco::Align>(first % 3u);
	const std::vector<affresco::TextRun> runs = runsOf(size > 0 ? text.substr(1) : text);

	affresco::TextLayout layout;
	layout.layOut(runs, *font(0), width, width, align);
	checkLines(layout);
	checkHits(layout);
	std::size_t characters = 0;
	for(const affresco::TextRun& run : runs) characters += run.text.size();
	if(layout.text().size() != characters) affresco::fuzz::broken("every code point laid out");
	return 0;
}
