/// @file
/// @brief The built-in bars of a desktop window (BuiltInComponents.hpp): toolbar, a row of buttons and toggles whose
/// items that don't fit move into a "more" menu, and statusBar, a row of texts at the start and at the end.

#include "BuiltInComponents.hpp"

#include <string_view>

namespace affresco::format {

namespace {

/// @brief The bars, in the format. A toolbar's item is {title, icon, keyLabel, tooltip, checked, enabled, width, run,
/// with} or {separator: true}; its width is the item's "width", "size" for an icon, or else estimated from its title.
/// The arrows move between the items, and the "more" menu opens at state[open].more. A status bar's item is {text,
/// icon, tooltip, color, run, with, side}, "side": "end" putting it at the end; an item with a "run" is a button.
constexpr std::string_view kitBars = R"({
"toolbar": {
	"params": { "items": [], "open": "state.toolbar", "width": "{{window.width}}", "height": 36, "size": 28,
	            "moreWidth": 32, "menuWidth": 220, "label": "Toolbar", "moreLabel": "More", "color": "@kit.bar",
	            "textColor": "@kit.text", "dimColor": "@kit.dim", "highlight": "@kit.barHover",
	            "checkedColor": "@kit.selection" },
	"node": {
		"width": "{{width}}", "height": "{{height}}", "color": "{{color}}", "role": "toolbar", "label": "{{label}}",
		"let": {
			"w": "{{map(items, (it.separator == true ? 9 : it.width ?? (it.icon != null ? size : length(it.title ?? '') * 7 + 20)) + 2)}}",
			"room": "{{sum(w) <= width - 8 ? length(items) : count(w, sum(slice(w, 0, index + 1)) <= width - 10 - moreWidth)}}",
			"fit": "{{room > 0 && room < length(items) && items[room - 1].separator == true ? room - 1 : room}}",
			"rest": "{{filter(slice(items, fit), it.separator != true)}}"
		},
		"children": [{
			"type": "row", "anchors": { "left": 4, "right": 4, "top": "{{(height - size) / 2}}" }, "height": "{{size}}",
			"gap": 2, "arrows": "horizontal",
			"children": [
				{ "repeat": { "for": "item", "in": "{{slice(items, 0, fit)}}", "index": "i" },
				  "width": "{{w[i] - 2}}", "anchors": { "top": 0, "bottom": 0 }, "radius": "@kit.radius",
				  "role": "{{item.separator == true ? 'separator' : 'button'}}",
				  "label": "{{item.title ?? item.tooltip ?? ''}}", "selected": "{{item.checked == true}}",
				  "focusable": "{{item.separator != true}}",
				  "disabled": "{{item.separator == true || item.enabled == false}}",
				  "tooltip": "{{item.separator == true ? '' : item.tooltip ?? (item.title ?? '') + (item.keyLabel != null ? ' (' + item.keyLabel + ')' : '')}}",
				  "color": "{{item.checked == true ? checkedColor : color}}",
				  "hover": { "color": "{{item.separator == true || item.enabled == false ? color : item.checked == true ? checkedColor : highlight}}" },
				  "focused": { "borderWidth": "@kit.borderWidth", "borderColor": "@kit.focus" },
				  "pressed": { "color": "{{checkedColor}}" },
				  "onClick": [{ "if": "{{item.run != null}}", "then": [{ "run": "{{item.run}}", "with": "{{item.with}}" }] }],
				  "children": [
					{ "when": "{{item.separator == true}}", "anchors": { "left": 4, "top": 4, "bottom": 4 }, "width": 1,
					  "color": "@kit.border" },
					{ "when": "{{item.separator != true}}", "type": "label", "icon": "{{item.icon ?? ''}}",
					  "text": "{{item.icon != null ? '' : item.title ?? ''}}", "align": "center",
					  "textColor": "{{item.enabled == false ? dimColor : textColor}}",
					  "anchors": { "left": 0, "right": 0, "top": 0, "bottom": 0 } }
				  ] },
				{ "when": "{{fit < length(items)}}", "width": "{{moreWidth}}", "anchors": { "top": 0, "bottom": 0 },
				  "radius": "@kit.radius", "role": "button", "label": "{{moreLabel}}", "tooltip": "{{moreLabel}}",
				  "focusable": true, "color": "{{color}}", "hover": { "color": "{{highlight}}" },
				  "focused": { "borderWidth": "@kit.borderWidth", "borderColor": "@kit.focus" },
				  "onClick": { "toggle": "{{open + '.more'}}" },
				  "children": [
					{ "type": "label", "icon": "ellipsis", "align": "center", "textColor": "{{textColor}}",
					  "anchors": { "left": 0, "right": 0, "top": 0, "bottom": 0 } },
					{ "use": "menu", "with": { "items": "{{rest}}", "open": "{{open + '.more'}}", "width": "{{menuWidth}}",
					  "placement": "below-end" } }
				  ] }
			]
		}]
	}
},
"statusBar": {
	"params": { "items": [], "height": 24, "label": "Status", "color": "@kit.bar", "textColor": "@kit.text",
	            "highlight": "@kit.barHover" },
	"node": {
		"type": "row", "height": "{{height}}", "color": "{{color}}", "role": "status", "label": "{{label}}",
		"alignItems": "center", "gap": 2, "arrows": "horizontal",
		"children": [
			{ "width": 4, "height": 1, "color": "{{color}}" },
			{ "repeat": { "for": "s", "in": "{{concat(filter(items, it.side != 'end'), [{spacer: true}], filter(items, it.side == 'end'))}}" },
			  "grow": "{{s.spacer == true ? 1 : 0}}", "type": "label", "fitText": "{{s.spacer != true}}", "padding": 3,
			  "text": "{{s.text ?? ''}}", "icon": "{{s.icon ?? ''}}", "textColor": "{{s.color ?? textColor}}",
			  "radius": "@kit.radius", "tooltip": "{{s.tooltip ?? ''}}",
			  "role": "{{s.run != null ? 'button' : 'text'}}", "label": "{{s.tooltip ?? s.text ?? ''}}",
			  "focusable": "{{s.run != null}}", "cursor": "{{s.run != null ? 'hand' : 'arrow'}}",
			  "hover": { "color": "{{s.run != null ? highlight : color}}" },
			  "focused": { "borderWidth": "@kit.borderWidth", "borderColor": "@kit.focus" },
			  "onClick": [{ "if": "{{s.run != null}}", "then": [{ "run": "{{s.run}}", "with": "{{s.with}}" }] }] },
			{ "width": 4, "height": 1, "color": "{{color}}" }
		]
	}
}
})";

} // namespace

std::string_view builtInKitBars() noexcept { return kitBars; }

} // namespace affresco::format
