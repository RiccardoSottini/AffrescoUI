/// @file
/// @brief Fuzzes checkManifest (format/src/features/FeatureManifest.hpp): a feature's manifest is untrusted input (RFC
/// 0004).
///
/// Any text is refused with json::parse_error or DocumentError, or is a manifest whose record composes into the JSON
/// template, and into the template holding a program (where a feature's own program builds), giving a screen or
/// problems, never another error.

#include "Fuzz.hpp"

#include "Composition.hpp"
#include "FeatureManifest.hpp"

#include <cstddef>
#include <cstdint>

using affresco::json;

/// @brief libFuzzer's entry point: checks the input as a manifest and, if it passes, composes it.
/// @param data The input.
/// @param size Its size.
/// @return 0.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	if(size > affresco::format::maxManifestBytes) return 0;
	json manifest;
	try {
		manifest = affresco::parseUntrusted(affresco::fuzz::text(data, size));
		affresco::format::checkManifest(manifest, "feature");
	} catch(const json::parse_error&) {
		return 0;
	} catch(const affresco::DocumentError&) {
		return 0;
	}
	json document = affresco::fuzz::jsonTemplate();
	document["features"][manifest["name"].get<std::string>()] =
	    affresco::format::recordFor(manifest, "fuzz", "", "", 10);
	for(const bool withProgram : {false, true}) {
		// A static app refuses a feature's program; an app with one builds it
		if(withProgram)
			document["program"] = {{"app.lisp", {{"after-save", "(defhook after-save \"After a save.\")"}}}};
		try {
			static_cast<void>(affresco::format::compose(document));
			// NOLINTNEXTLINE(bugprone-empty-catch): a document the format refuses is an answer, not a finding
		} catch(const affresco::DocumentError&) {
		}
	}
	return 0;
}
