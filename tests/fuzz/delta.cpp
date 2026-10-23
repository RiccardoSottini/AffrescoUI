/// @file
/// @brief Fuzzes history deltas (vcs/src/Deltas.hpp): a history on disk keeps each commit as a delta that its id
/// doesn't cover, and loading applies it; and a delta must rebuild its document exactly, keys in order.
///
/// The input is a JSON object `{"from": a, "to": b, "delta": d}`. Applying d to a must give a document or throw
/// json::exception or DocumentError, never crash or assert; and delta(a, b) applied to a must give b, written the
/// same way.

#include "../../modules/vcs/src/revisions/Deltas.hpp" // by its path: clang-tidy reads this file without its include folders
#include "Fuzz.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>

using affresco::json;

namespace {

/// @brief Applies a delta as a history read from disk does with a commit's.
/// @param document The document.
/// @param delta The delta.
/// @return Whether it applied (else it threw what a history reports as damage).
bool applies(json document, const json& delta) {
	try {
		affresco::vcs::detail::applyDelta(document, delta, "fuzz");
		return true;
	} catch(const json::exception&) {
		return false;
	} catch(const affresco::DocumentError&) {
		return false;
	}
}

} // namespace

/// @brief libFuzzer's entry point.
/// @param data The input.
/// @param size Its size.
/// @return 0.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	json input;
	try {
		input = affresco::parseUntrusted(affresco::fuzz::text(data, size));
	} catch(const json::parse_error&) {
		return 0;
	} catch(const affresco::DocumentError&) {
		return 0; // too deep
	}
	if(!input.is_object()) return 0;
	const json from = input.value("from", json()), to = input.value("to", json());
	static_cast<void>(applies(from, input.value("delta", json::array())));

	// What a history writes: it must give back the document its commit's id names
	json rebuilt = from;
	affresco::vcs::detail::applyDelta(rebuilt, affresco::vcs::detail::delta(from, to), "fuzz");
	if(rebuilt.dump() != to.dump()) affresco::fuzz::broken("a delta rebuilds its document exactly");
	return 0;
}
