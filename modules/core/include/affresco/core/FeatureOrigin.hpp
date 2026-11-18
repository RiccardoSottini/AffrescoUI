#pragma once

/// @file
/// @brief Where a feature's record came from, so two modules that never link each other (the format and version
/// control) tell an installed feature from one the app's own file shipped the same way.

#include "affresco/core/Json.hpp"

#include <algorithm>
#include <array>
#include <string_view>

namespace affresco {

/// The keys of a feature's record that say which release it is and where it came from: an install from a catalogue
/// or a package writes them, and an upgrade takes the new release's.
inline constexpr std::array<std::string_view, 5> featureOriginKeys = {"release", "source", "digest", "publisher",
                                                                      "installed"};

/// @brief Tells whether a feature's record says it was installed: it holds one of featureOriginKeys, or it isn't an
/// object, which no file ships, so it is never taken for the app's own.
/// @param record The record.
/// @return Whether it does.
[[nodiscard]] inline bool holdsFeatureOrigin(const json& record) {
	if(!record.is_object()) return true;
	return std::ranges::any_of(featureOriginKeys, [&](std::string_view key) { return record.contains(key); });
}

/// @brief Tells whether a document's feature was installed (holdsFeatureOrigin()): false when it has no record, as for
/// the app's own files under `features/<name>/`.
/// @param document The document.
/// @param name The feature.
/// @return Whether it was.
[[nodiscard]] inline bool installedFeature(const json& document, std::string_view name) {
	if(!document.is_object()) return true;
	const auto records = document.find("features");
	if(records == document.end()) return false;
	if(!records->is_object()) return true;
	const auto record = records->find(name);
	return record != records->end() && holdsFeatureOrigin(*record);
}

} // namespace affresco
