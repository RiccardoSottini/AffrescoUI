/// @file
/// @brief Fuzzes readPackage and packageSummary (format/PackageFiles.hpp, format/Package.hpp): a package is a file a
/// person shares, untrusted input (VERSION_CONTROL.md, "Sharing a version: packages").
///
/// Any bytes are refused with json::parse_error or DocumentError, or are a package that writes back as itself, reads
/// back from what it writes, gives the base its tree names, and has a summary.

#include "Fuzz.hpp"

#include "affresco/format/Package.hpp"
#include "affresco/format/PackageFiles.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>

using affresco::json;

/// @brief libFuzzer's entry point: reads the input as a package, writes it back and sums it up.
/// @param data The input.
/// @param size Its size.
/// @return 0.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	affresco::format::Package package;
	try {
		package = affresco::format::readPackage(affresco::fuzz::text(data, size));
	} catch(const json::parse_error&) {
		return 0;
	} catch(const affresco::DocumentError&) {
		return 0;
	}
	const json written = affresco::format::packageJson(package);
	const affresco::format::Package again = affresco::format::readPackage(written.dump());
	if(affresco::format::packageJson(again) != written) std::abort();
	const json base = affresco::format::baseOf(package);
	if(affresco::format::documentTree(base) != package.base.tree) std::abort();
	const json summary = affresco::format::packageSummary(package, base, json::object());
	if(summary["base"]["exact"] != true) std::abort();
	if(summary["kind"] != (package.kind == affresco::format::PackageKind::App ? "app" : "changes")) std::abort();
	return 0;
}
