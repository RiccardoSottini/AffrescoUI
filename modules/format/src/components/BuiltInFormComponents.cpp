/// @file
/// @brief The built-in form components (BuiltInComponents.hpp): comboBox, a text field with a list of matching
/// choices, and passwordMeter, a bar and a word for passwordStrength().

#include "BuiltInComponents.hpp"

#include <string_view>

namespace affresco::format {

namespace {

/// @brief The form components, in the format. A comboBox keeps its state under state[name]: value (the text),
/// open (whether the list shows) and index (the highlighted choice). Down opens the list and moves down, Up moves up,
/// Enter picks the highlighted choice, Escape or a click outside closes it.
constexpr std::string_view formComponents = R"({
"comboBox": {
	"params": { "name": "comboBox", "options": [], "rows": 6, "rowHeight": 26, "width": 240, "height": 30,
	            "placeholder": "", "label": "", "color": "#ffffff", "textColor": "#1f2328", "dimColor": "#6e7781",
	            "highlight": "#ddf4ff", "borderColor": "#d0d7de" },
	"node": {
		"width": "{{width}}", "height": "{{height}}", "role": "combobox", "label": "{{label}}",
		"let": {
			"base": "{{'state.' + name}}",
			"cb": "{{state[name] ?? {}}}",
			"q": "{{lower(trim(cb.value ?? ''))}}",
			"matches": "{{filter(options, length(q) == 0 || contains(lower(text(it)), q))}}",
			"total": "{{length(matches)}}",
			"sel": "{{clamp(cb.index ?? 0, 0, max(0, total - 1))}}",
			"shown": "{{cb.open == true && total > 0}}"
		},
		"keys": {
			"down": [{ "if": "{{cb.open == true}}",
			           "then": [{ "set": "{{base + '.index'}}", "to": "{{min(sel + 1, max(0, total - 1))}}" }],
			           "else": [{ "set": "{{base + '.open'}}", "to": true }, { "set": "{{base + '.index'}}", "to": 0 }] }],
			"up": [{ "set": "{{base + '.index'}}", "to": "{{max(sel - 1, 0)}}" }],
			"escape": [{ "set": "{{base + '.open'}}", "to": false }]
		},
		"children": [
			{ "type": "textarea", "bind": "{{base + '.value'}}", "placeholder": "{{placeholder}}", "label": "{{label}}",
			  "anchors": { "left": 0, "right": 0, "top": 0, "bottom": 0 }, "padding": 6, "radius": 4,
			  "borderWidth": 1, "borderColor": "{{borderColor}}", "color": "{{color}}", "textColor": "{{textColor}}",
			  "placeholderColor": "{{dimColor}}",
			  "onChange": [{ "set": "{{base + '.open'}}", "to": true }, { "set": "{{base + '.index'}}", "to": 0 }],
			  "onSubmit": [{ "if": "{{shown}}", "then": [
				{ "set": "{{base + '.value'}}", "to": "{{text(matches[sel])}}" },
				{ "set": "{{base + '.open'}}", "to": false }] }] },
			{ "type": "column", "role": "listbox", "label": "{{label}}",
			  "overlay": { "placement": "below", "open": "{{base + '.open'}}", "dismiss": "outside" },
			  "width": "{{width}}", "height": "{{min(total, rows) * rowHeight}}", "color": "{{color}}", "radius": 4,
			  "borderWidth": 1, "borderColor": "{{borderColor}}",
			  "children": [{
				"repeat": { "for": "o", "in": "{{slice(matches, max(0, sel - rows + 1), max(0, sel - rows + 1) + rows)}}",
				            "index": "i" },
				"anchors": { "left": 0, "right": 0 }, "height": "{{rowHeight}}", "role": "option", "cursor": "hand",
				"label": "{{text(o)}}", "selected": "{{max(0, sel - rows + 1) + i == sel}}",
				"color": "{{max(0, sel - rows + 1) + i == sel ? highlight : color}}", "hover": { "color": "{{highlight}}" },
				"onClick": [{ "set": "{{base + '.value'}}", "to": "{{text(o)}}" }, { "set": "{{base + '.open'}}", "to": false }],
				"children": [{ "type": "label", "text": "{{text(o)}}", "textColor": "{{textColor}}",
				               "anchors": { "left": 8, "top": "{{(rowHeight - 18) / 2}}" }, "fitText": true }]
			  }] }
		]
	}
},
"passwordMeter": {
	"params": { "password": "", "width": 240, "labels": {}, "track": "#d0d7de", "textColor": "#57606a",
	            "colors": ["#cf222e", "#cf222e", "#bf8700", "#1a7f37", "#1a7f37"] },
	"node": {
		"width": "{{width}}", "height": 34, "role": "status", "live": "polite",
		"let": { "strength": "{{passwordStrength(password)}}" },
		"label": "{{labels[strength.label] ?? strength.label}}",
		"children": [
			{ "anchors": { "left": 0, "right": 0, "top": 0 }, "height": 6, "radius": 3, "color": "{{track}}" },
			{ "anchors": { "left": 0, "top": 0 }, "height": 6, "radius": 3,
			  "width": "{{width * strength.score / 4}}", "color": "{{colors[strength.score]}}" },
			{ "type": "label", "anchors": { "left": 0, "top": 12 }, "fitText": true, "textColor": "{{textColor}}",
			  "text": "{{length(password) == 0 ? '' : labels[strength.label] ?? strength.label}}" }
		]
	}
}
})";

} // namespace

std::string_view builtInFormComponents() noexcept { return formComponents; }

} // namespace affresco::format
