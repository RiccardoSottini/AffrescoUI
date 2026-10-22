#pragma once

/// @file
/// @brief format::systemLocale(): the locale the computer's user chose, for Runtime::setLocale().

#include <string>

namespace affresco::format {

/// @brief The locale the computer's user chose: the system's preference on macOS and Windows, LC_ALL, LC_MESSAGES or
/// LANG elsewhere.
/// @return A BCP 47 tag, "fr-CA"; "" if none is set.
[[nodiscard]] std::string systemLocale();

} // namespace affresco::format
