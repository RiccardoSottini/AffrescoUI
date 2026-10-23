/// @file
/// @brief Fuzzes the search service's globs (services/src/search/SearchGlob.hpp): a screen's "include" and "exclude"
/// lists and the `.gitignore` files of a searched folder come from peers and from anywhere on disk, so a list must
/// compile or fail, and matching must stay within its limits.
///
/// The input is a glob list up to the first zero byte, then a `.gitignore` text up to the next, then paths, one per
/// line. A list compiles or gives an error and keeps at most GlobList::maxGlobs globs; the rules kept are at most
/// IgnoreRules::maxRules; a path matches the same way twice, and whatever a matching folder holds matches too.

#include "Fuzz.hpp"

#include "search/SearchGlob.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace {

constexpr std::size_t maxPaths = 64; ///< Paths matched in one input, at most.

/// @brief Splits the input at a zero byte.
/// @param input What is left; loses the part and its zero.
/// @return The part before the zero (all of it, without one).
std::string_view until(std::string_view& input) noexcept {
	const std::size_t zero = std::min(input.find('\0'), input.size());
	const std::string_view part = input.substr(0, zero);
	input.remove_prefix(std::min(zero + 1, input.size()));
	return part;
}

/// @brief Checks one path against the list and the rules.
/// @param globs The list.
/// @param rules The rules.
/// @param path The path.
void check(const affresco::services::GlobList& globs, const affresco::services::IgnoreRules& rules,
           std::string_view path) {
	std::vector<std::string_view> segments = affresco::services::splitPath(path);
	if(segments.empty()) return;
	const bool folder = path.ends_with('/');
	const bool matched = globs.matches(segments, folder);
	if(matched != globs.matches(segments, folder)) affresco::fuzz::broken("a path matches the same way twice");
	if(rules.ignored(segments, folder) != rules.ignored(segments, folder))
		affresco::fuzz::broken("a path is ignored the same way twice");
	if(!globs.matches(segments, true)) return;
	segments.push_back("inside");
	if(!globs.matches(segments, false)) affresco::fuzz::broken("what a matching folder holds matches too");
}

} // namespace

extern "C" int LLVMFuzzerInitialize(int* /*argc*/, char*** /*argv*/) {
	affresco::fuzz::silenceLog();
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	std::string_view input = affresco::fuzz::text(data, size);
	const std::string_view list = until(input), ignore = until(input);

	affresco::services::GlobList globs;
	if(globs.compile(list)) globs = {};
	affresco::services::IgnoreRules rules;
	rules.add("", ignore.substr(0, affresco::services::IgnoreRules::maxFileBytes));
	rules.add("sub", ignore.substr(0, affresco::services::IgnoreRules::maxFileBytes));
	if(rules.size() > affresco::services::IgnoreRules::maxRules) affresco::fuzz::broken("the rules kept are capped");

	for(std::size_t count = 0; count < maxPaths && !input.empty(); ++count) {
		const std::size_t end = std::min(input.find('\n'), input.size());
		check(globs, rules, input.substr(0, end));
		input.remove_prefix(std::min(end + 1, input.size()));
	}
	return 0;
}
