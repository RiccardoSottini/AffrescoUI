/// @file
/// @brief Telling formats apart, and converting whichever one a file is in (Adapters.hpp).

#include "affresco/format/Adapters.hpp"

#include <algorithm>
#include <array>

namespace affresco::format::adapters {

namespace {

/// @brief Whether a value is one A2UI message: an object whose only key is a message's name.
/// @param value The value.
/// @return Whether it is.
[[nodiscard]] bool a2uiMessage(const json& value) noexcept {
	constexpr std::array<std::string_view, 4> kinds{"surfaceUpdate", "dataModelUpdate", "beginRendering",
	                                                "deleteSurface"};
	if(!value.is_object() || value.size() != 1) return false;
	const std::string& key = value.begin().key();
	return std::ranges::find(kinds, key) != kinds.end();
}

} // namespace

SourceFormat detect(const json& document) noexcept {
	if(document.is_object() && document.contains("type") && document["type"] == "AdaptiveCard")
		return SourceFormat::AdaptiveCards;
	if(a2uiMessage(document)) return SourceFormat::A2ui;
	if(document.is_array() && !document.empty() && std::ranges::all_of(document, a2uiMessage)) return SourceFormat::A2ui;
	return SourceFormat::Screen;
}

std::optional<Converted> convertForeign(std::string_view text, const Options& options) {
	if(text.size() > maxInputBytes) return std::nullopt;
	try {
		const json document = parseUntrusted(text);
		switch(detect(document)) {
		case SourceFormat::A2ui: return fromA2ui(text, options);
		case SourceFormat::AdaptiveCards: return fromAdaptiveCard(document, options);
		case SourceFormat::Screen: break;
		}
		return std::nullopt;
	} catch(const json::parse_error&) { // perhaps JSON Lines: A2UI, one message per line
	}
	const std::string_view first = text.substr(0, std::min(text.find('\n'), text.size()));
	try {
		if(detect(parseUntrusted(first)) != SourceFormat::A2ui) return std::nullopt;
	} catch(const json::parse_error&) {
		return std::nullopt;
	}
	return fromA2ui(text, options);
}

} // namespace affresco::format::adapters
