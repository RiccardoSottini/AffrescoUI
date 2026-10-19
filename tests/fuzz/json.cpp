/// @file
/// @brief Fuzzes parseUntrusted (core/Json.hpp), which reads every message, revision and change set from outside.
///
/// Any text either parses or throws json::parse_error or DocumentError. What parses nests at most maxNesting levels,
/// so the recursive work the app does on a value (copying, comparing, writing it out) can't overflow the stack.

#include "Fuzz.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

using affresco::json;

namespace {

/// @brief Measures how deep a value nests, without recursion (the point is to check the limit, not to rely on it).
/// @param value The value.
/// @return How many arrays and objects the deepest item is inside, counting itself: 0 for a scalar, 1 for `[1]`.
int depthOf(const json& value) {
	int deepest = 0;
	std::vector<std::pair<const json*, int>> pending{{&value, 0}}; // a value, and how many containers it is inside
	while(!pending.empty()) {
		const auto [current, outside] = pending.back();
		pending.pop_back();
		if(!current->is_structured()) continue;
		deepest = std::max(deepest, outside + 1);
		for(const json& item : *current) pending.emplace_back(&item, outside + 1);
	}
	return deepest;
}

} // namespace

/// @brief libFuzzer's entry point: parses the input with and without comments.
/// @param data The input.
/// @param size Its size.
/// @return 0.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	for(const bool comments : {false, true}) {
		try {
			const json value = affresco::parseUntrusted(affresco::fuzz::text(data, size), comments);
			if(depthOf(value) > affresco::maxNesting) affresco::fuzz::broken("parseUntrusted keeps to maxNesting");
			const json copy = value;
			if(copy != value) affresco::fuzz::broken("a parsed value equals its copy");
			static_cast<void>(copy.dump());
		} catch(const json::parse_error&) {
		} catch(const affresco::DocumentError&) {
		}
	}
	return 0;
}
