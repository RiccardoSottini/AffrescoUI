/// @file
/// @brief The built-in panels of a desktop window (BuiltInComponents.hpp): inspector, a property sheet of collapsible
/// groups of rows editing an object in the state (numbers scrubbed by dragging their name, sliders, colours, choices,
/// switches, texts and vectors), and ribbon, tabs of groups of large and small buttons whose groups that don't fit
/// fold into one button each.

#include "BuiltInComponents.hpp"

#include <string_view>

namespace affresco::format {

namespace {

/// @brief The panels, in the format. An inspector's row is {key, label, type, unit, min, max, step, decimals,
/// options, axes, enabled, tooltip}, its type "number" (the default), "slider", "color", "select", "toggle",
/// "checkbox", "text" or "vector" (a list of numbers, one field per axis); it edits values[key] at bind + "." + key
/// and runs onChange with {key, value}. Its own state at state[name] is {closed (group ids), scrub, open}. A typed
/// number is clamped to min and max, and a text that isn't one leaves the value as it was.
constexpr std::string_view kitPanels = R"({
"inspector": {
	"params": { "name": "inspector", "bind": null, "values": null, "groups": [], "onChange": "", "width": 280,
	            "height": 320, "labelWidth": 100, "rowHeight": 28, "headerHeight": 26, "pickerWidth": 240,
	            "label": "Properties", "color": "@kit.surface", "headerColor": "@kit.bar", "textColor": "@kit.text",
	            "dimColor": "@kit.dim", "fieldColor": "@kit.field", "borderColor": "@kit.border",
	            "focusColor": "@kit.focus", "highlight": "@kit.barHover", "menuColor": "@kit.surface",
	            "axisColors": ["#e5484d", "#30a46c", "#3e63dd", "#8e4ec6"] },
	"node": {
		"let": {
			"base": "{{'state.' + name}}",
			"ui": "{{state[name] ?? {}}}",
			"path": "{{bind ?? 'state.' + name + '.values'}}",
			"vals": "{{values ?? (state[name] ?? {}).values ?? {}}}"
		},
		"type": "column", "width": "{{width}}", "height": "{{height}}", "color": "{{color}}", "scroll": "vertical",
		"role": "form", "label": "{{label}}",
		"children": [{
			"repeat": { "for": "g", "in": "{{groups}}" },
			"let": { "shut": "{{(ui.closed ?? {})[g.id] == true}}" },
			"type": "column", "width": "100%", "color": "{{color}}",
			"height": "{{headerHeight + (shut ? 0 : length(g.rows ?? []) * rowHeight + 6)}}",
			"children": [
				{ "width": "100%", "height": "{{headerHeight}}", "color": "{{headerColor}}", "role": "button",
				  "label": "{{g.title ?? g.id}}", "focusable": true, "cursor": "hand", "isOpen": "{{!shut}}",
				  "hover": { "color": "{{highlight}}" },
				  "focused": { "borderWidth": "@kit.borderWidth", "borderColor": "{{focusColor}}" },
				  "onClick": { "set": "{{base + '.closed.' + g.id}}", "to": "{{!shut}}" },
				  "children": [
					{ "type": "label", "icon": "{{shut ? 'chevron-right' : 'chevron-down'}}", "mirror": true,
					  "align": "center", "textColor": "{{dimColor}}", "anchors": { "left": 4, "top": 0, "bottom": 0 },
					  "width": 18 },
					{ "type": "label", "text": "{{g.title ?? g.id}}", "align": { "horizontal": "start", "vertical": "center" }, "textColor": "{{textColor}}",
					  "anchors": { "left": 24, "right": 8, "top": 0, "bottom": 0 } }
				  ] },
				{ "when": "{{!shut}}", "type": "column", "width": "100%", "height": "{{length(g.rows ?? []) * rowHeight + 6}}",
				  "color": "{{color}}",
				  "children": [{
					"repeat": { "for": "r", "in": "{{g.rows ?? []}}" },
					"let": {
						"kind": "{{r.type ?? 'number'}}",
						"v": "{{vals[r.key]}}",
						"at": "{{path + '.' + r.key}}",
						"step": "{{r.step ?? 1}}",
						"lo": "{{r.min ?? -1000000000}}",
						"hi": "{{r.max ?? 1000000000}}",
						"off": "{{r.enabled == false}}",
						"caption": "{{r.label ?? r.key}}"
					},
					"type": "row", "width": "100%", "height": "{{rowHeight}}", "color": "{{color}}", "role": "group",
					"label": "{{caption}}", "tooltip": "{{r.tooltip ?? ''}}",
					"children": [{
						"let": { "d": "{{r.decimals ?? (step >= 1 ? 0 : step >= 0.1 ? 1 : step >= 0.01 ? 2 : 3)}}" },
						"type": "row", "grow": 1, "anchors": { "top": 0, "bottom": 0 }, "color": "{{color}}",
						"children": [
						{ "color": "{{color}}", "width": 12, "height": 1 },
						{ "type": "label", "width": "{{labelWidth}}", "anchors": { "top": 0, "bottom": 0 }, "text": "{{caption}}", "align": { "horizontal": "start", "vertical": "center" }, 
						  "textColor": "{{off ? dimColor : textColor}}",
						  "cursor": "{{kind == 'number' && !off ? 'resize-horizontal' : 'arrow'}}",
						  "onPress": [{ "if": "{{kind == 'number' && !off}}", "then": [
							{ "set": "{{base + '.scrub'}}", "to": "{{ {key: r.key, from: number(v) ?? 0, last: number(v) ?? 0} }}" }] }],
						  "onDrag": [{ "if": "{{kind == 'number' && !off && (state[name] ?? {}).scrub.key == r.key && clamp(number(fixed(state[name].scrub.from + round(event.dx / 2) * step, d)), lo, hi) != state[name].scrub.last}}", "then": [
							{ "set": "{{at}}", "to": "{{clamp(number(fixed(state[name].scrub.from + round(event.dx / 2) * step, d)), lo, hi)}}" },
							{ "set": "{{base + '.scrub.last'}}", "to": "{{clamp(number(fixed(state[name].scrub.from + round(event.dx / 2) * step, d)), lo, hi)}}" },
							{ "if": "{{onChange != ''}}", "then": [{ "run": "{{onChange}}", "with": "{{ {key: r.key, value: clamp(number(fixed(state[name].scrub.from + round(event.dx / 2) * step, d)), lo, hi)} }}" }] }] }] },
						{ "grow": 1, "anchors": { "top": 3, "bottom": 3 }, "color": "{{color}}",
						  "children": [
							{ "when": "{{kind == 'number'}}", "type": "textarea", "label": "{{caption}}",
							  "text": "{{fixed(number(v) ?? 0, d)}}", "disabled": "{{off}}",
							  "anchors": { "left": 0, "right": "{{r.unit != null ? 30 : 0}}", "top": 0, "bottom": 0 },
							  "padding": 4, "color": "{{fieldColor}}", "radius": "@kit.radius",
							  "borderWidth": "@kit.borderWidth", "borderColor": "{{borderColor}}",
							  "focused": { "borderColor": "{{focusColor}}" }, "textColor": "{{textColor}}",
							  "onSubmit": [
								{ "if": "{{number(event.text) != null && number(event.text) == number(event.text)}}", "then": [
									{ "set": "{{at}}", "to": "{{clamp(number(event.text), lo, hi)}}" },
									{ "if": "{{onChange != ''}}", "then": [{ "run": "{{onChange}}", "with": "{{ {key: r.key, value: clamp(number(event.text), lo, hi)} }}" }] }] }],
							  "keys": {
								"up": [{ "set": "{{at}}", "to": "{{clamp(number(fixed((number(v) ?? 0) + step, d)), lo, hi)}}" },
								       { "if": "{{onChange != ''}}", "then": [{ "run": "{{onChange}}", "with": "{{ {key: r.key, value: clamp(number(fixed((number(v) ?? 0) + step, d)), lo, hi)} }}" }] }],
								"down": [{ "set": "{{at}}", "to": "{{clamp(number(fixed((number(v) ?? 0) - step, d)), lo, hi)}}" },
								         { "if": "{{onChange != ''}}", "then": [{ "run": "{{onChange}}", "with": "{{ {key: r.key, value: clamp(number(fixed((number(v) ?? 0) - step, d)), lo, hi)} }}" }] }]
							  } },
							{ "when": "{{kind == 'number' && r.unit != null}}", "type": "label", "text": "{{r.unit ?? ''}}",
							  "textColor": "{{dimColor}}", "align": "center", "width": 28,
							  "anchors": { "right": 0, "top": 0, "bottom": 0 } },
							{ "when": "{{kind == 'slider'}}", "type": "slider", "bind": "{{at}}", "min": "{{r.min ?? 0}}",
							  "max": "{{r.max ?? 100}}", "step": "{{step}}", "label": "{{caption}}", "disabled": "{{off}}",
							  "anchors": { "left": 0, "right": 48, "top": 2, "bottom": 2 },
							  "onChange": [{ "if": "{{onChange != ''}}", "then": [{ "run": "{{onChange}}", "with": "{{ {key: r.key, value: event.value} }}" }] }] },
							{ "when": "{{kind == 'slider'}}", "type": "label", "text": "{{fixed(number(v) ?? 0, d) + (r.unit ?? '')}}",
							  "textColor": "{{textColor}}", "align": { "horizontal": "end", "vertical": "center" }, "width": 44,
							  "anchors": { "right": 0, "top": 0, "bottom": 0 } },
							{ "when": "{{kind == 'color'}}", "width": 34, "anchors": { "left": 0, "top": 0, "bottom": 0 },
							  "color": "{{v ?? '#000000'}}", "radius": "@kit.radius", "borderWidth": "@kit.borderWidth",
							  "borderColor": "{{borderColor}}", "role": "button", "label": "{{caption}}", "focusable": "{{!off}}",
							  "disabled": "{{off}}", "cursor": "hand", "isOpen": "{{ui.open == r.key}}",
							  "focused": { "borderColor": "{{focusColor}}" },
							  "onClick": { "set": "{{base + '.open'}}", "to": "{{ui.open == r.key ? null : r.key}}" },
							  "children": [{ "type": "colorPicker", "bind": "{{at}}", "label": "{{caption}}", "alpha": false,
							                 "overlay": { "placement": "below", "flip": true, "open": "{{ui.open == r.key}}" },
							                 "width": "{{pickerWidth}}", "height": 300, "color": "{{menuColor}}",
							                 "radius": "@kit.cardRadius", "borderWidth": "@kit.borderWidth",
							                 "borderColor": "{{borderColor}}",
							                 "keys": { "escape": { "set": "{{base + '.open'}}", "to": null } },
							                 "onChange": [{ "if": "{{onChange != ''}}", "then": [{ "run": "{{onChange}}", "with": "{{ {key: r.key, value: event.value} }}" }] }] }] },
							{ "when": "{{kind == 'color'}}", "type": "label", "text": "{{v ?? ''}}", "align": { "horizontal": "start", "vertical": "center" }, "textColor": "{{textColor}}",
							  "anchors": { "left": 42, "right": 0, "top": 0, "bottom": 0 } },
							{ "when": "{{kind == 'select'}}",
							  "let": { "opts": "{{map(r.options ?? [], type(it) == 'object' ? it : {id: it, label: text(it)})}}" },
							  "anchors": { "left": 0, "right": 0, "top": 0, "bottom": 0 }, "color": "{{fieldColor}}",
							  "radius": "@kit.radius", "borderWidth": "@kit.borderWidth", "borderColor": "{{borderColor}}",
							  "role": "combobox", "label": "{{caption}}", "focusable": "{{!off}}", "disabled": "{{off}}",
							  "cursor": "hand", "isOpen": "{{ui.open == r.key}}", "focused": { "borderColor": "{{focusColor}}" },
							  "onClick": { "set": "{{base + '.open'}}", "to": "{{ui.open == r.key ? null : r.key}}" },
							  "children": [
								{ "type": "label", "text": "{{(find(opts, it.id == v) ?? {}).label ?? text(v ?? '')}}", "align": { "horizontal": "start", "vertical": "center" }, 
								  "textColor": "{{off ? dimColor : textColor}}", "anchors": { "left": 6, "right": 22, "top": 0, "bottom": 0 } },
								{ "type": "label", "icon": "chevron-down", "align": "center", "textColor": "{{dimColor}}",
								  "anchors": { "right": 2, "top": 0, "bottom": 0 }, "width": 18 },
								{ "type": "column", "role": "listbox", "label": "{{caption}}", "arrows": "vertical",
								  "overlay": { "placement": "below", "flip": true, "open": "{{ui.open == r.key}}" },
								  "width": "{{width - labelWidth - 24}}", "height": "{{length(opts) * 26 + 8}}", "color": "{{menuColor}}",
								  "radius": "@kit.cardRadius", "borderWidth": "@kit.borderWidth", "borderColor": "{{borderColor}}",
								  "keys": { "escape": { "set": "{{base + '.open'}}", "to": null } },
								  "children": [{ "color": "transparent", "height": 4, "width": 1 }, {
									"repeat": { "for": "o", "in": "{{opts}}" },
									"width": "100%", "height": 26, "role": "option", "label": "{{o.label ?? o.id}}",
									"selected": "{{o.id == v}}", "focusable": true, "cursor": "hand",
									"color": "{{o.id == v ? '@kit.selection' : menuColor}}",
									"hover": { "color": "@kit.selection" }, "focused": { "color": "@kit.selection" },
									"onClick": [{ "set": "{{at}}", "to": "{{o.id}}" }, { "set": "{{base + '.open'}}", "to": null },
									            { "if": "{{onChange != ''}}", "then": [{ "run": "{{onChange}}", "with": "{{ {key: r.key, value: o.id} }}" }] }],
									"children": [{ "type": "label", "text": "{{o.label ?? o.id}}", "align": { "horizontal": "start", "vertical": "center" }, "textColor": "{{textColor}}",
									               "anchors": { "left": 10, "right": 6, "top": 0, "bottom": 0 } }] }] }
							  ] },
							{ "when": "{{kind == 'toggle'}}", "type": "toggle", "bind": "{{at}}", "label": "{{caption}}",
							  "disabled": "{{off}}", "width": 34, "anchors": { "left": 0, "top": 2, "bottom": 2 },
							  "onChange": [{ "if": "{{onChange != ''}}", "then": [{ "run": "{{onChange}}", "with": "{{ {key: r.key, value: event.value} }}" }] }] },
							{ "when": "{{kind == 'checkbox'}}", "type": "checkbox", "bind": "{{at}}", "label": "{{caption}}",
							  "disabled": "{{off}}", "width": 18, "height": 18, "anchors": { "left": 0 }, "y": 2,
							  "onChange": [{ "if": "{{onChange != ''}}", "then": [{ "run": "{{onChange}}", "with": "{{ {key: r.key, value: event.value} }}" }] }] },
							{ "when": "{{kind == 'text'}}", "type": "textarea", "bind": "{{at}}", "label": "{{caption}}",
							  "disabled": "{{off}}", "anchors": { "left": 0, "right": 0, "top": 0, "bottom": 0 }, "padding": 4,
							  "color": "{{fieldColor}}", "radius": "@kit.radius", "borderWidth": "@kit.borderWidth",
							  "borderColor": "{{borderColor}}", "focused": { "borderColor": "{{focusColor}}" },
							  "textColor": "{{textColor}}",
							  "onSubmit": [{ "if": "{{onChange != ''}}", "then": [{ "run": "{{onChange}}", "with": "{{ {key: r.key, value: event.text} }}" }] }] },
							{ "when": "{{kind == 'vector'}}", "type": "row", "gap": 4, "role": "group", "label": "{{caption}}",
							  "anchors": { "left": 0, "right": 0, "top": 0, "bottom": 0 },
							  "children": [{
								"repeat": { "for": "axis", "in": "{{r.axes ?? ['x', 'y', 'z']}}", "index": "ai" },
								"let": { "n": "{{number((v ?? [])[ai]) ?? 0}}" },
								"grow": 1, "anchors": { "top": 0, "bottom": 0 }, "color": "{{fieldColor}}",
								"radius": "@kit.radius", "borderWidth": "@kit.borderWidth", "borderColor": "{{borderColor}}",
								"children": [
									{ "type": "label", "text": "{{upper(axis)}}", "align": "center",
									  "textColor": "{{axisColors[ai] ?? dimColor}}", "width": 14,
									  "anchors": { "left": 1, "top": 1, "bottom": 1 } },
									{ "type": "textarea", "label": "{{caption + ' ' + axis}}",
									  "text": "{{fixed(n, d)}}", "disabled": "{{off}}", "padding": 2, "color": "{{fieldColor}}",
									  "anchors": { "left": 15, "right": 1, "top": 1, "bottom": 1 }, "textColor": "{{textColor}}",
									  "focused": { "color": "@kit.selection" },
									  "onSubmit": [
										{ "if": "{{number(event.text) != null && number(event.text) == number(event.text)}}", "then": [
											{ "set": "{{at + '[' + ai + ']'}}", "to": "{{clamp(number(event.text), lo, hi)}}" },
											{ "if": "{{onChange != ''}}", "then": [{ "run": "{{onChange}}", "with": "{{ {key: r.key, value: map(range(max(length(v ?? []), ai + 1)), it == ai ? clamp(number(event.text), lo, hi) : number((v ?? [])[it]) ?? 0)} }}" }] }] }],
									  "keys": {
										"up": [{ "set": "{{at + '[' + ai + ']'}}", "to": "{{clamp(number(fixed(n + step, d)), lo, hi)}}" },
										       { "if": "{{onChange != ''}}", "then": [{ "run": "{{onChange}}", "with": "{{ {key: r.key, value: map(range(max(length(v ?? []), ai + 1)), it == ai ? clamp(number(fixed(n + step, d)), lo, hi) : number((v ?? [])[it]) ?? 0)} }}" }] }],
										"down": [{ "set": "{{at + '[' + ai + ']'}}", "to": "{{clamp(number(fixed(n - step, d)), lo, hi)}}" },
										       { "if": "{{onChange != ''}}", "then": [{ "run": "{{onChange}}", "with": "{{ {key: r.key, value: map(range(max(length(v ?? []), ai + 1)), it == ai ? clamp(number(fixed(n - step, d)), lo, hi) : number((v ?? [])[it]) ?? 0)} }}" }] }]
									  } }
								]
							  }] }
						  ] },
						{ "color": "{{color}}", "width": 12, "height": 1 }
					]}]
				  }] }
			]
		}]
	}
}
})";

} // namespace

std::string_view builtInKitPanels() noexcept { return kitPanels; }

} // namespace affresco::format
