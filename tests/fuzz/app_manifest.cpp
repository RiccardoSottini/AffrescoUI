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

/// @brief Reads a value as a manifest; a refused one is replaced by the manifest the host makes for a lone screen.
/// @param value The value.
/// @return The manifest.
affresco::format::AppManifest readManifest(const json& value) {
	try {
		return affresco::format::parseApp(value);
	} catch(const affresco::DocumentError&) {
		return affresco::format::manifestOf("fuzz", "Fuzz", "index.screen.json", affresco::fuzz::kanbanTemplate());
	}
}

/// @brief Stops if a manifest breaks a promise: an id that is its own appId(), every path inside the app's folder,
/// every capability with a key and a name.
/// @param app The manifest.
void checkManifest(const affresco::format::AppManifest& app) {
	if(affresco::format::appId(app.id) != app.id) std::abort();
	for(const std::string& path : {app.entry, app.models})
		if(!path.empty() && !affresco::format::insideApp(path)) std::abort();
	for(const auto& [collection, path] : app.seed)
		if(!affresco::format::insideApp(path)) std::abort();
	for(const affresco::format::AppCapability& capability : app.capabilities)
		if(capability.key().empty() || capability.name.empty()) std::abort();
}

/// @brief Stops if a capability the manifest doesn't declare isn't among those the screen uses.
/// @param app The manifest.
/// @param screen The same JSON, read as a screen.
void checkUndeclared(const affresco::format::AppManifest& app, const json& screen) {
	std::vector<std::string> used;
	for(const affresco::format::AppCapability& capability : affresco::format::usedCapabilities(screen))
		used.push_back(capability.key());
	for(const std::string& missing : affresco::format::undeclaredCapabilities(app, screen))
		if(std::ranges::find(used, missing) == used.end()) std::abort();
}

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
	} catch(const affresco::DocumentError&) {
		return 0; // too deep, or a number too long
	}
	const affresco::format::AppManifest app = readManifest(value);
	checkManifest(app);
	if(value.is_object()) checkUndeclared(app, value);
	return 0;
}
