/// @file
/// @brief Fuzzes core's TextPattern: the editor's find and the search service compile patterns that peers and agents
/// send, then match them against text from anywhere, so a pattern must compile or fail, and match within its limits.
///
/// The input is an options byte (bit 0 a regular expression, bit 1 whole words, bit 2 case-sensitive), then the
/// pattern up to the first zero byte, then the replacement up to the next, then the subject (any bytes, invalid UTF-8
/// included). Every match found from successive starts must lie inside the subject, at or after its start, and
/// expanding it must not fail on a plain pattern.

#include "Fuzz.hpp"

#include <affresco/core/TextPattern.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace {

constexpr std::size_t maxFinds = 256; ///< Matches looked for in one subject, at most.

/// @brief Splits the input at a zero byte.
/// @param input What is left; loses the part and its zero.
/// @return The part before the zero (all of it, without one).
std::string_view until(std::string_view& input) noexcept {
	const std::size_t zero = std::min(input.find('\0'), input.size());
	const std::string_view part = input.substr(0, zero);
	input.remove_prefix(std::min(zero + 1, input.size()));
	return part;
}

} // namespace

extern "C" int LLVMFuzzerInitialize(int* /*argc*/, char*** /*argv*/) {
	affresco::fuzz::silenceLog();
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	std::string_view input = affresco::fuzz::text(data, size);
	if(input.empty()) return 0;
	const auto bits = static_cast<unsigned char>(input.front());
	input.remove_prefix(1);
	const affresco::PatternOptions options{
	    .regex = (bits & 1u) != 0, .wholeWord = (bits & 2u) != 0, .caseSensitive = (bits & 4u) != 0};
	const std::string_view pattern = until(input), replacement = until(input), subject = input;

	affresco::TextPattern compiled;
	const bool failed = compiled.compile(pattern, options).has_value();
	if(failed == compiled.valid()) affresco::fuzz::broken("a pattern compiles or gives an error, not both");
	if(!compiled.valid()) return 0;
	if(pattern.size() > affresco::TextPattern::maxPatternBytes) affresco::fuzz::broken("long patterns are refused");

	std::size_t start = 0;
	for(std::size_t count = 0; count < maxFinds && start <= subject.size(); ++count) {
		const affresco::PatternFind found = compiled.find(subject, start);
		if(!found.match) break;
		const affresco::PatternMatch m = *found.match;
		if(m.offset < start || m.offset + m.length > subject.size())
			affresco::fuzz::broken("a match lies inside the subject, at or after its start");
		if(!compiled.expand(subject, m, replacement) && !options.regex)
			affresco::fuzz::broken("a plain pattern's replacement always expands");
		start = m.offset + std::max<std::size_t>(m.length, 1);
	}
	return 0;
}
