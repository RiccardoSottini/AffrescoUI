/// @file
/// @brief The built-in overlays of a desktop window (BuiltInComponents.hpp): popover, a card anchored to a node with
/// an arrow pointing at it, that goes on the other side when only that one has room.

#include "BuiltInComponents.hpp"

#include <string_view>

namespace affresco::format {

namespace {

/// @brief The overlays, in the format. A popover's arrow is a turned square whose back half, with the border, shows
/// over the card's edge and whose front half hides the border under it; the overlay flips and mirrors with its children,
/// so the arrow keeps facing the anchor above or below it and at its start in either writing direction.
constexpr std::string_view kitOverlays = R"({
"popover": {
	"params": { "open": "state.popover", "anchor": "", "title": "", "text": "", "width": 260, "height": 80,
	            "arrowAt": 16, "placement": "below", "label": "{{title}}", "color": "@kit.surface",
	            "textColor": "@kit.text", "dimColor": "@kit.body" },
	"node": {
		"overlay": { "anchor": "{{anchor}}", "placement": "{{placement}}", "flip": true, "open": "{{open}}",
		             "dismiss": "outside" },
		"width": "{{width}}", "height": "{{height + 7}}", "role": "dialog", "label": "{{label}}",
		"children": [
			{ "x": "{{arrowAt}}", "y": 2, "width": 10, "height": 10, "rotate": 45, "color": "{{color}}",
			  "borderWidth": "@kit.borderWidth", "borderColor": "@kit.border" },
			{ "y": 7, "width": "{{width}}", "height": "{{height}}", "color": "{{color}}", "radius": "@kit.cardRadius",
			  "borderWidth": "@kit.borderWidth", "borderColor": "@kit.border",
			  "children": [
				{ "type": "label", "text": "{{title}}", "textColor": "{{textColor}}", "anchors": { "left": 12, "top": 10 },
				  "fitText": true },
				{ "type": "label", "text": "{{text}}", "textColor": "{{dimColor}}", "wrap": true,
				  "anchors": { "left": 12, "right": 12, "top": 32, "bottom": 8 } },
				{ "slot": true }
			  ] },
			{ "x": "{{arrowAt}}", "y": 3, "width": 10, "height": 10, "rotate": 45, "color": "{{color}}" }
		]
	}
}
})";

} // namespace

std::string_view builtInKitOverlays() noexcept { return kitOverlays; }

} // namespace affresco::format
