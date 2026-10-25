#pragma once

/// @file
/// @brief Key notations: how a chord is written for people, chosen by the document ("window.keyLabels").

#include <string>
#include <string_view>

namespace affresco::format {

/// @brief Tells whether a notation is one keyLabel() knows.
/// @param notation "platform", "mac", "pc" or "letters" (also "emacs", its older name).
/// @return true if it is.
[[nodiscard]] bool knownKeyNotation(std::string_view notation) noexcept;

/// @brief A normalised chord as people read it, in a notation: "mac" writes "⌃X ⌘S", "pc" "Ctrl+X Super+S",
/// "letters" (or "emacs") "C-x s-s" (meta is "M-": "M-x", "M-<", "C-SPC", "RET", "<f3>"), "platform" (or an unknown
/// name) the platform's own.
/// @param chord The chord, as normalizeChord() writes it.
/// @param notation The notation.
/// @return Its label.
[[nodiscard]] std::string keyLabel(std::string_view chord, std::string_view notation);

} // namespace affresco::format
