/// @file
/// @brief Fuzzes the reader of font tables behind the fast path for plain text (render's text/LookupSpans): a revision
/// can name any font file, and its GSUB and GPOS tables are read with offsets the file gives.
///
/// The input's first byte says which table it is (an odd byte: GPOS), and the rest is the table. Every lookup the
/// table may list is read, against a fixed set of candidate glyphs.

#include "Fuzz.hpp"

#include "font/LookupSpans.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {

/// @brief Candidate glyphs: every third one up to 600, about what Latin-1 maps to in a font.
/// @return Them, sorted.
const std::vector<unsigned>& candidates() {
	static const std::vector<unsigned> glyphs = [] {
		std::vector<unsigned> list;
		for(unsigned glyph = 1; glyph < 600; glyph += 3) list.push_back(glyph);
		return list;
	}();
	return glyphs;
}

} // namespace

extern "C" int LLVMFuzzerInitialize(int* /*argc*/, char*** /*argv*/) {
	affresco::fuzz::silenceLog();
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	if(size == 0) return 0;
	const bool positioning = (data[0] & 1) != 0;
	affresco::detail::LookupSpans spans(std::span(data + 1, size - 1), positioning, candidates());
	for(unsigned lookup = 0; lookup < 64; ++lookup) {
		const auto span = spans.span(lookup);
		if(span && *span > affresco::detail::LookupSpans::unbounded) affresco::fuzz::broken("spans are bounded");
		if(span != spans.span(lookup)) affresco::fuzz::broken("a lookup's span is the same when read again");
	}
	return 0;
}
