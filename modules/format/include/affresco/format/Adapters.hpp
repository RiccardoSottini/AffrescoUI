#pragma once

/// @file
/// @brief Adapters from other UI formats (SCREEN_FORMAT.md, "Other formats"): pure functions that turn an A2UI message
/// stream or an Adaptive Card into a screen document, with a visible placeholder and a warning for what has no
/// equivalent. Each is pinned to the version of the spec it was written against.

#include "affresco/core/Json.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::format::adapters {

inline constexpr std::string_view a2uiVersion = "0.8";          ///< The A2UI spec the A2UI adapter follows.
inline constexpr std::string_view adaptiveCardsVersion = "1.6"; ///< The Adaptive Cards schema its adapter follows.
inline constexpr std::size_t maxInputBytes = 1024 * 1024;       ///< Of a document or a message stream.
inline constexpr int maxDepth = 32;                             ///< Elements inside elements.
inline constexpr std::size_t maxElements = 2000;                ///< Elements in one conversion.

/// @brief What a document is written in.
enum class SourceFormat {
	Screen,       ///< Ours (or anything else: the screen format reports what is wrong with it).
	A2ui,         ///< An A2UI message, or a list of them.
	AdaptiveCards ///< An Adaptive Card, `{"type": "AdaptiveCard", ...}`.
};

/// @brief How a conversion is written.
struct Options {
	std::string fontFile;     ///< The text font, relative to the screen file; empty: the screen has no fonts.
	std::string title;        ///< The window's title; empty: the source's own, or the format's name.
	float width = 480;        ///< The window's width, which text is wrapped to.
	float height = 640;       ///< Its height.
};

/// @brief A screen made from another format.
struct Converted {
	json screen;                       ///< The screen document ("affrescoui-screen", version 1).
	std::vector<std::string> warnings; ///< What became a placeholder or was left out, each naming its place.
	std::vector<json> revisions;       ///< A2UI's updates after it began rendering, as revisions, in order.
};

/// @brief Tells what a parsed document is written in, from its shape alone.
/// @param document The document.
/// @return Its format.
[[nodiscard]] SourceFormat detect(const json& document) noexcept;

/// @brief Converts A2UI: one message, a JSON list of messages, or JSON Lines (one message per line).
/// @param text The messages, at most maxInputBytes.
/// @param options How the screen is written.
/// @return The screen, its warnings and its revisions.
/// @throws DocumentError Naming the place, if the text is too large, not A2UI, or past a limit.
[[nodiscard]] Converted fromA2ui(std::string_view text, const Options& options = {});

/// @brief Converts an Adaptive Card.
/// @param card The card, `{"type": "AdaptiveCard", "body": [...], "actions": [...]}`.
/// @param options How the screen is written.
/// @return The screen and its warnings.
/// @throws DocumentError Naming the place, if it isn't a card or is past a limit.
[[nodiscard]] Converted fromAdaptiveCard(const json& card, const Options& options = {});

/// @brief Converts a document if it is written in another format: what Screen does with the files it loads.
/// @param text The file's text.
/// @param options How the screen is written.
/// @return The conversion, or std::nullopt if the text is a screen (or not JSON, or larger than maxInputBytes).
/// @throws DocumentError If it is another format's, and can't be converted.
[[nodiscard]] std::optional<Converted> convertForeign(std::string_view text, const Options& options = {});

} // namespace affresco::format::adapters
