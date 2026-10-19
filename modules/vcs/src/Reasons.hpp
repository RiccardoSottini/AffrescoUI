#pragma once

/// @file
/// @brief The reason an error gives, without the prefix that says where it came from, for a message that wraps it in
/// another place: "proposal #3: no longer applies (<reason>)", "AffrescoUI history: <file>: <reason>".

#include "affresco/core/Json.hpp"

#include <string>
#include <string_view>

namespace affresco::vcs::detail {

/// @brief A document error's reason.
/// @param error The error: "AffrescoUI screen: revision.patch[0]: …".
/// @return Its message without "AffrescoUI screen: ".
[[nodiscard]] inline std::string reasonOf(const DocumentError& error) {
	std::string why = error.what();
	if(const std::string_view prefix = "AffrescoUI screen: "; why.starts_with(prefix)) why.erase(0, prefix.size());
	return why;
}

/// @brief A JSON library error's reason.
/// @param error The error: "[json.exception.out_of_range.403] key 'author' not found".
/// @return Its message without the bracketed code: "key 'author' not found".
[[nodiscard]] inline std::string reasonOf(const json::exception& error) {
	std::string why = error.what();
	if(const std::size_t bracket = why.find("] "); why.starts_with("[json.exception") && bracket != std::string::npos)
		why.erase(0, bracket + 2);
	return why;
}

} // namespace affresco::vcs::detail
