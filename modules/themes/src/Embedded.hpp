/// @file
/// @brief The themes' JSON files, built into the library (Embed.cmake writes their bytes at build time).

#pragma once

#include <span>
#include <string_view>

namespace affresco::themes {

/// @brief One embedded file.
struct EmbeddedFile {
	std::string_view path;                ///< Its path in modules/themes, e.g. "components/core.json".
	std::span<const unsigned char> bytes; ///< Its contents.
};

/// @brief Every embedded file.
/// @return Them, in the order CMakeLists.txt lists them.
[[nodiscard]] std::span<const EmbeddedFile> embeddedFiles() noexcept;

} // namespace affresco::themes
