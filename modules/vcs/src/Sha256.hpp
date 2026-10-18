#pragma once

/// @file
/// @brief SHA-256, for content-addressed commits.

#include <string>
#include <string_view>

namespace affresco::vcs::detail {

/// @brief Hashes bytes with SHA-256 (FIPS 180-4).
/// @param data The bytes.
/// @return 64 lowercase hex digits.
[[nodiscard]] std::string sha256(std::string_view data);

} // namespace affresco::vcs::detail
