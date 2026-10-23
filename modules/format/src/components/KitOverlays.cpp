/// @file
/// @brief The built-in overlays of a desktop window (BuiltInComponents.hpp): popover, a card anchored to a node with
/// an arrow pointing at it, that goes on the other side when only that one has room, and toasts, a stack of
/// notifications in the bottom corner.

#include "BuiltInComponents.hpp"

#include <string_view>

namespace affresco::format {

namespace {

/// @brief The overlays, in the format. A popover's arrow is a turned square whose back half, with the border, shows
/// over the card's edge and whose front half hides the border under it; the overlay flips and mirrors with its children,
/// so the arrow keeps facing the anchor above or below it and at its start in either writing direction. The toasts are
/// the list at state[name], each {message, progress, color, actions}; the last "limit" show, the newest at the bottom,
/// and a toast's × or action removes it from the list (an action then runs).
constexpr std::string_view kitOverlays = R"({
"popover": {
	"params": { "open": "state.popover", "anchor": "", "title": "", "text": "", "width": 260, "height": 80,
	            "arrowAt": 16, "placement": "below", "label": "{{title}}", "color": "@kit.surface",
	            "textColor": "@kit.text", "dimColor": "@kit.body" },
	"node": {
		"overlay": { "anchor": "{{anchor}}", "placement": "{{placement}}", "flip": true, "open": "{{open}}",
		             "dismiss": "outside" },
		"width": "{{width}}", "height": "{{height + 7}}", "color": "transparent", "role": "dialog", "label": "{{label}}",
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
},
"toasts": {
	"params": { "name": "toasts", "limit": 4, "width": 360, "gap": 8, "label": "Notifications", "closeLabel": "Close",
	            "color": "@kit.surface", "textColor": "@kit.text", "accent": "@kit.primary" },
	"node": {
		"overlay": { "placement": "window", "open": "{{length(state[name] ?? []) > 0}}" },
		"let": {
			"from": "{{max(0, length(state[name] ?? []) - limit)}}",
			"list": "{{slice(state[name] ?? [], from)}}"
		},
		"anchors": { "right": 16, "bottom": 16 }, "width": "{{width}}",
		"height": "{{sum(map(list, it.progress == null ? 64 : 72)) + gap * max(0, length(list) - 1)}}",
		"color": "transparent", "role": "region", "label": "{{label}}",
		"children": [{
			"type": "column", "anchors": { "left": 0, "right": 0, "top": 0, "bottom": 0 }, "gap": "{{gap}}",
			"children": [{
				"repeat": { "for": "t", "in": "{{list}}", "index": "i" },
				"width": "{{width}}", "height": "{{t.progress == null ? 64 : 72}}", "color": "{{color}}",
				"radius": "@kit.cardRadius", "borderWidth": "@kit.borderWidth", "borderColor": "@kit.border",
				"arrows": "horizontal", "role": "status", "live": "polite", "label": "{{t.message ?? ''}}",
				"children": [
					{ "type": "label", "text": "{{t.message ?? ''}}", "textColor": "{{textColor}}",
					  "anchors": { "left": 14, "right": 36, "top": 10 }, "height": 20 },
					{ "type": "label", "text": "×", "textColor": "{{textColor}}", "anchors": { "right": 8, "top": 6 },
					  "width": 24, "height": 24, "align": "center", "radius": "@kit.radius", "cursor": "hand", "focusable": true,
					  "role": "button", "label": "{{closeLabel}}", "focused": { "color": "@kit.secondary" },
					  "onClick": { "remove": "{{'state.' + name}}", "at": "{{from + i}}" } },
					{ "when": "{{t.progress != null}}", "anchors": { "left": 14, "right": 14, "bottom": 10 }, "height": 3,
					  "color": "@kit.secondary",
					  "children": [{ "anchors": { "left": 0, "top": 0, "bottom": 0 }, "color": "{{t.color ?? accent}}",
					                 "width": "{{max(0, min(1, t.progress ?? 0)) * 100 + '%'}}" }] },
					{ "type": "row", "anchors": { "left": 14, "bottom": "{{t.progress == null ? 8 : 18}}" }, "height": 22,
					  "width": "{{length(t.actions ?? []) * 96}}", "gap": 8,
					  "children": [{
						"repeat": { "for": "a", "in": "{{t.actions ?? []}}" },
						"width": 88, "anchors": { "top": 0, "bottom": 0 }, "focusable": true, "cursor": "hand",
						"role": "button", "label": "{{a.title ?? ''}}", "mnemonic": "{{a.mnemonic ?? ''}}",
						"color": "{{color}}", "hover": { "color": "@kit.secondary" }, "focused": { "color": "@kit.secondary" },
						"onClick": [{ "remove": "{{'state.' + name}}", "at": "{{from + i}}" },
						            { "if": "{{a.run != null}}", "then": [{ "run": "{{a.run}}", "with": "{{a.with}}" }] }],
						"children": [{ "type": "label", "text": "{{a.title ?? ''}}", "textColor": "{{t.color ?? accent}}",
						               "anchors": { "left": 0, "top": 3 }, "fitText": true }]
					  }]
					}
				]
			}]
		}]
	}
}
})";

} // namespace

std::string_view builtInKitOverlays() noexcept { return kitOverlays; }

} // namespace affresco::format
