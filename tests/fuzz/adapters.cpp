/// @file
/// @brief Fuzzes the A2UI and Adaptive Cards adapters (format/include/affresco/format/Adapters.hpp): a document in
/// another format is untrusted input (SECURITY.md, "Other formats").
///
/// Any text is refused with json::parse_error or DocumentError, or is not another format's (std::nullopt), or
/// converts into a screen that the screen format accepts as it is; and the conversion stays within its limits.

#include "Fuzz.hpp"

#include <affresco/format/Adapters.hpp>
#include <affresco/format/Rules.hpp>
#include <affresco/vcs/Repository.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>

using affresco::json;
namespace adapters = affresco::format::adapters;

/// @brief Silences the log.
/// @return 0.
extern "C" int LLVMFuzzerInitialize(int* /*argc*/, char*** /*argv*/) {
	affresco::fuzz::silenceLog();
	return 0;
}

/// @brief libFuzzer's entry point: converts the input, as the screen loader does, and loads the screen it gives.
/// @param data The input.
/// @param size Its size.
/// @return 0.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	std::optional<adapters::Converted> converted;
	try {
		converted = adapters::convertForeign(affresco::fuzz::text(data, size));
	} catch(const json::parse_error&) {
		return 0;
	} catch(const affresco::DocumentError&) {
		return 0;
	}
	if(!converted) return 0;
	if(converted->warnings.size() > adapters::maxElements) std::abort();
	static const affresco::format::Rules rules;
	const affresco::vcs::Repository repository(rules, converted->screen); // a screen the format refuses throws here
	static_cast<void>(repository);
	return 0;
}
