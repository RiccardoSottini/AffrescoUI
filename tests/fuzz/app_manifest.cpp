/// @file
/// @brief Fuzzes parseApp (format/App.hpp): an app's manifest is untrusted input (SCREEN_FORMAT.md, "Apps: app.json").
///
/// Any text is refused with json::parse_error or DocumentError, or is a manifest whose id is its own appId(), whose
/// every path is inside the app's folder, and whose capabilities each have a key and a reason. The same JSON read as a
/// screen gives the capabilities it uses, never an error, and the ones the manifest doesn't declare are among them.

#include "Fuzz.hpp"

#include "affresco/format/App.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

using affresco::json;

/// @brief libFuzzer's entry point: reads the input as a manifest, and as a screen against it.
/// @param data The input.
/// @param size Its size.
/// @return 0.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	if(size > affresco::format::maxAppManifestBytes) return 0;
	json value;
	try {
		value = affresco::parseUntrusted(affresco::fuzz::text(data, size));
	} catch(const json::parse_error&) {
		return 0;
	}
	affresco::format::AppManifest app;
	try {
		app = affresco::format::parseApp(value);
	} catch(const affresco::DocumentError&) {
		app = affresco::format::manifestOf("fuzz", "Fuzz", "index.screen.json", affresco::fuzz::kanbanTemplate());
	}
	if(affresco::format::appId(app.id) != app.id) std::abort();
	for(const std::string& path : {app.entry, app.models})
		if(!path.empty() && !affresco::format::insideApp(path)) std::abort();
	for(const auto& [collection, path] : app.seed)
		if(!affresco::format::insideApp(path)) std::abort();
	for(const affresco::format::AppCapability& capability : app.capabilities)
		if(capability.key().empty() || capability.name.empty()) std::abort();
	if(!value.is_object()) return 0;
	std::vector<std::string> used;
	for(const affresco::format::AppCapability& capability : affresco::format::usedCapabilities(value))
		used.push_back(capability.key());
	for(const std::string& missing : affresco::format::undeclaredCapabilities(app, value))
		if(std::ranges::find(used, missing) == used.end()) std::abort();
	return 0;
}
