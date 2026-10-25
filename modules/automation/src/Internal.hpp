#pragma once

/// @file
/// @brief What the files of the automation module share: errors that name the place, key names, state paths, node
/// properties, and the limits on what a scenarios file may ask for.

#include "affresco/core/Json.hpp"
#include "affresco/render/Input.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::automation::detail {

/* Limits: a scenarios file can come from an agent, so what it asks for is bounded */

inline constexpr std::size_t maxScenarios = 1000;     ///< Scenarios in one file.
inline constexpr std::size_t maxSteps = 10000;        ///< Steps in one scenario.
inline constexpr std::size_t maxTypedBytes = 100000;  ///< Bytes one `type` step types.
inline constexpr double maxWaitMs = 600000.0;         ///< Longest `wait`: ten minutes of the scenario's clock.
inline constexpr int maxDragMoves = 100;              ///< Moves of the pointer in one `drag`.
inline constexpr int maxScale = 4;                    ///< Largest scale of a scenario's pictures.
inline constexpr int maxWindowSide = 16384;           ///< Largest side of a `resize`, in window units.
inline constexpr std::size_t maxTimerRunsPerWait = 1000000; ///< Timer runs one `wait` allows: one that keeps
                                                            ///< setting itself at no delay would never end.

/// @brief Reports a problem in a scenarios file.
/// @param place Where: "scenarios[1].steps[3].click".
/// @param message What is wrong.
/// @throws DocumentError Always: "AffrescoUI scenarios: <place>: <message>" (load() adds the file's name).
[[noreturn]] void fail(const std::string& place, const std::string& message);

/// @brief A key and the modifiers held with it.
struct KeyPress {
	Key key = Key::Unknown; ///< The key.
	Mods mods;              ///< The modifiers.
};

/// @brief Reads a key as scenarios write it: "enter", "a", "f5", "cmd+z", "shift+alt+down".
/// @param text The key.
/// @return The key, or std::nullopt if it isn't one.
[[nodiscard]] std::optional<KeyPress> parseKey(std::string_view text);

/// @brief The key names parseKey() knows, for error messages.
/// @return "enter, escape, ..., a-z, 0-9, f1-f12".
[[nodiscard]] std::string keyNames();

/// @brief Reads a path into the live state, as expressions write it without `state.`: "cards[0].title",
/// "cards.length", "board['my-list']". A last segment `length` on a list or a text that has no such key gives its
/// length.
/// @param text The path.
/// @return Its segments (names, and indexes as their digits), or std::nullopt if it isn't a path.
[[nodiscard]] std::optional<std::vector<std::string>> parseStatePath(std::string_view text);

/// @brief Tells whether `expect` can read a node property of that name.
/// @param name The property.
/// @return true for text, textColor, color, borderColor, borderWidth, shown, visible, focused, hovered, tooltip,
/// checked, value, x, y, width, height, scrollX, scrollY, role, name, description and scrollMarks.
[[nodiscard]] bool isReadableProperty(std::string_view name);

/// @brief The property names isReadableProperty() accepts, for error messages.
/// @return Them, separated by commas.
[[nodiscard]] std::string readableProperties();

/// @brief Tells whether a name can be part of a file's name: scenario and picture names become file names, so they
/// may only have letters, digits, '-', '_' and '.', and don't start with '.'.
/// @param name The name.
/// @return true if it can.
[[nodiscard]] bool isSafeFileName(std::string_view name);

} // namespace affresco::automation::detail
