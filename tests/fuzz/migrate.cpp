/// @file
/// @brief Fuzzes migrate and migratePatch (format/Migrate.hpp): a version 1 file is untrusted input, read by the
/// loader and by `affrescoui migrate`.
///
/// Any text is refused with json::parse_error, or migrates without another error; a second migration changes nothing,
/// and a document it changed declares the current version.

#include "Fuzz.hpp"

#include <affresco/format/Migrate.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdlib>

using affresco::json;
using namespace affresco::format;

/// @brief libFuzzer's entry point: migrates the input as a document and as a patch, then again.
/// @param data The input.
/// @param size Its size.
/// @return 0.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	json document;
	try {
		document = affresco::parseUntrusted(affresco::fuzz::text(data, size));
	} catch(const json::parse_error&) {
		return 0;
	}
	json patch = document;
	json migrated = document;
	if(!migrate(migrated).empty() && documentVersion(document) == 1 && documentVersion(migrated) != formatVersion)
		std::abort(); // changed but still says version 1
	json again = migrated;
	if(!migrate(again).empty() || again != migrated) std::abort(); // not idempotent
	if(patch.is_array()) {
		static_cast<void>(migratePatch(patch));
		json twice = patch;
		if(!migratePatch(twice).empty() || twice != patch) std::abort();
	}
	return 0;
}
