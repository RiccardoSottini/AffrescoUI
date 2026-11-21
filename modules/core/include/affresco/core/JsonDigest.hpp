#pragma once

/// @file
/// @brief A digest of a JSON value that doesn't depend on the order of its members, so two modules that never link
/// each other (the format and version control) tell whether a value changed the same way.

#include "affresco/core/Json.hpp"
#include "affresco/core/Sha256.hpp"

#include <string>

namespace affresco {

/// @brief The SHA-256 of a value's compact text, every object's members sorted by name.
/// @param value The value (a checked document part: its depth is bounded where it was read).
/// @return "sha256:<64 hex digits>".
[[nodiscard]] inline std::string jsonDigest(const json& value) {
	// nlohmann::json keeps its members in a std::map: converting sorts them at every depth
	const nlohmann::json sorted = value;
	return "sha256:" + sha256::hex(sorted.dump());
}

} // namespace affresco
