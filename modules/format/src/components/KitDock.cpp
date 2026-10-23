/// @file
/// @brief The built-in dock of a desktop window (BuiltInComponents.hpp): panels in tabbed areas at the left, the
/// right, the bottom and the centre, which collapse, maximise, resize by their edge and take a panel dragged from
/// another area or moved from a menu, the layout kept in the state.

#include "BuiltInComponents.hpp"

#include <string_view>

namespace affresco::format {

namespace {

/// @brief The dock, in the format. Its state at state[name] is {left, right, bottom, center}, each {tabs (panel ids),
/// active, collapsed}, plus sizes {left, right, bottom}, maximized (an area or null), resizing (the area whose edge is
/// being dragged) and menu (the area whose "move" menu is open). The use's children are shown in every area (every
/// slot gets them), with `area` and `panel`, the area's active panel, so each one says which panel it is with
/// "when". The areas mirror right to left; `dir` tells the sashes which way the arrow keys go.
constexpr std::string_view kitDock = R"({
"dock": {
	"params": { "name": "dock", "panels": {}, "dir": "inherit", "label": "Panels", "headerHeight": 30, "stripWidth": 40,
	            "minSize": 120, "maxSize": 2000, "sizes": { "left": 240, "right": 240, "bottom": 200 },
	            "menuWidth": 180, "maximizeLabel": "Maximize", "restoreLabel": "Restore",
	            "collapseLabel": "Collapse", "moveLabel": "Move to", "resizeLabel": "Resize",
	            "areaTitles": { "left": "Left", "right": "Right", "bottom": "Bottom", "center": "Center" },
	            "color": "@kit.bar", "bodyColor": "@kit.background", "panelColor": "@kit.surface", "textColor": "@kit.dim",
	            "activeTextColor": "@kit.text", "accentColor": "@kit.primary", "highlight": "@kit.barHover",
	            "borderColor": "@kit.border", "sashColor": "@kit.primary", "menuColor": "@kit.surface" },
	"node": {
		"let": {
			"base": "{{'state.' + name}}",
			"d": "{{state[name] ?? {}}}",
			"most": "{{(state[name] ?? {}).maximized ?? ''}}",
			"has": "{{ {left: length(d.left.tabs ?? []) > 0, right: length(d.right.tabs ?? []) > 0, bottom: length(d.bottom.tabs ?? []) > 0} }}",
			"columns": "{{filter(['left', 'middle', 'right'], it == 'middle' ? most == '' || most == 'center' || most == 'bottom' : has[it] && (most == '' || most == it))}}",
			"middle": "{{filter(['center', 'bottom'], it == 'center' ? most != 'bottom' : has.bottom && (most == '' || most == 'bottom'))}}",
			"size": "{{ {left: clamp(d.sizes.left ?? sizes.left, minSize, maxSize), right: clamp(d.sizes.right ?? sizes.right, minSize, maxSize), bottom: clamp(d.sizes.bottom ?? sizes.bottom, minSize, maxSize)} }}"
		},
		"type": "row", "role": "group", "label": "{{label}}", "dir": "{{dir}}",
		"children": [{
			"repeat": { "for": "c", "in": "{{columns}}" },
			"type": "column", "anchors": { "top": 0, "bottom": 0 }, "grow": "{{c == 'middle' || most == c ? 1 : 0}}",
			"width": "{{c == 'middle' || most == c ? 0 : d[c].collapsed == true ? stripWidth : size[c]}}",
			"children": [{
				"repeat": { "for": "area", "in": "{{c == 'middle' ? middle : [c]}}" },
				"let": {
					"ar": "{{d[area] ?? {}}}",
					"tabs": "{{d[area].tabs ?? []}}",
					"panel": "{{contains(d[area].tabs ?? [], d[area].active) ? d[area].active : (d[area].tabs ?? [])[0] ?? ''}}",
					"closed": "{{d[area].collapsed == true && area != 'center' && most != area}}",
					"side": "{{area == 'left' || area == 'right'}}",
					"edge": "{{area != 'center' && most != area && !(d[area].collapsed == true)}}",
					"forward": "{{area == 'bottom' ? -1 : (area == 'left') == (dir != 'rtl') ? 1 : -1}}"
				},
				"width": "100%", "grow": "{{area == 'bottom' && most != 'bottom' ? 0 : 1}}",
				"height": "{{area == 'bottom' && most != 'bottom' ? (closed ? headerHeight : size.bottom) : 0}}",
				"role": "region", "label": "{{panels[panel].title ?? areaTitles[area]}}",
				"color": "{{area == 'center' ? bodyColor : panelColor}}",
				"onPress": [{ "if": "{{edge && (area == 'bottom' ? event.top < 6 : event.x < window.width / 2 ? event.left > event.width - 6 : event.left < 6)}}",
				              "then": [{ "set": "{{base + '.resizing'}}", "to": "{{ {area: area, from: size[area], sign: event.x < window.width / 2 ? 1 : -1} }}" }],
				              "else": [{ "if": "{{d.resizing != null}}", "then": [{ "set": "{{base + '.resizing'}}", "to": null }] }] }],
				"onDrag": [{ "if": "{{state[name].resizing.area == area}}", "then": [{ "set": "{{base + '.sizes.' + area}}",
				             "to": "{{clamp(round(state[name].resizing.from + (area == 'bottom' ? -event.dy : state[name].resizing.sign * event.dx)), minSize, maxSize)}}" }] }],
				"onDrop": [{ "if": "{{type(event.data) == 'object' && contains(concat(d.left.tabs ?? [], d.right.tabs ?? [], d.bottom.tabs ?? [], d.center.tabs ?? []), event.data.id)}}",
				             "then": [
					{ "forEach": "{{['left', 'right', 'bottom', 'center']}}", "as": "from", "do": [
						{ "if": "{{contains(state[name][from].tabs ?? [], event.data.id)}}", "then": [
							{ "set": "{{base + '.' + from + '.tabs'}}", "to": "{{filter(state[name][from].tabs, it != event.data.id)}}" }] }] },
					{ "set": "{{base + '.' + area + '.tabs'}}",
					  "to": "{{concat(slice(state[name][area].tabs ?? [], 0, event.index ?? length(state[name][area].tabs ?? [])), [event.data.id], slice(state[name][area].tabs ?? [], event.index ?? length(state[name][area].tabs ?? [])))}}" },
					{ "set": "{{base + '.' + area + '.active'}}", "to": "{{event.data.id}}" }
				  ] }],
				"children": [
					{ "when": "{{closed && side}}", "type": "column", "anchors": { "left": 0, "right": 0, "top": 0, "bottom": 0 },
					  "color": "{{color}}", "gap": 2, "arrows": "vertical", "role": "toolbar", "label": "{{areaTitles[area]}}",
					  "children": [{ "color": "transparent", "height": 4 }, {
						"repeat": { "for": "t", "in": "{{tabs}}" },
						"anchors": { "left": 4, "right": 4 }, "height": "{{stripWidth - 8}}", "radius": "@kit.radius",
						"role": "button", "label": "{{panels[t].title ?? t}}", "tooltip": "{{panels[t].title ?? t}}",
						"focusable": true, "cursor": "hand", "color": "{{color}}", "hover": { "color": "{{highlight}}" },
						"focused": { "borderWidth": "@kit.borderWidth", "borderColor": "@kit.focus" },
						"onClick": [{ "set": "{{base + '.' + area + '.active'}}", "to": "{{t}}" },
						            { "set": "{{base + '.' + area + '.collapsed'}}", "to": false }],
						"children": [{ "type": "label", "icon": "{{panels[t].icon ?? 'panel-top'}}", "align": "center",
						               "textColor": "{{t == panel ? activeTextColor : textColor}}",
						               "anchors": { "left": 0, "right": 0, "top": 0, "bottom": 0 } }]
					  }] },
					{ "when": "{{!(closed && side) && length(tabs) > 0}}", "type": "row",
					  "anchors": { "left": 0, "right": 0, "top": 0 }, "height": "{{headerHeight}}", "color": "{{color}}",
					  "children": [
						{ "type": "tabs", "grow": 1, "height": "{{headerHeight}}", "label": "{{areaTitles[area]}}",
						  "tabs": "{{map(tabs, {id: it, label: panels[it].title ?? it, closable: false})}}", "value": "{{panel}}",
						  "drag": {}, "color": "{{color}}", "textColor": "{{textColor}}",
						  "activeTextColor": "{{activeTextColor}}", "activeBackground": "{{area == 'center' ? bodyColor : panelColor}}",
						  "accentColor": "{{accentColor}}",
						  "onChange": [{ "set": "{{base + '.' + area + '.active'}}", "to": "{{event.value}}" },
						               { "if": "{{closed}}", "then": [{ "set": "{{base + '.' + area + '.collapsed'}}", "to": false }] }],
						  "onMove": { "set": "{{base + '.' + area + '.tabs'}}",
						              "to": "{{concat(slice(filter(tabs, index != event.from), 0, event.to), [tabs[event.from]], slice(filter(tabs, index != event.from), event.to))}}" } },
						{ "width": 26, "anchors": { "top": 2, "bottom": 2 }, "radius": "@kit.radius", "role": "button",
						  "label": "{{moveLabel}}", "tooltip": "{{moveLabel}}", "focusable": true, "cursor": "hand",
						  "isOpen": "{{d.menu == area}}", "color": "{{color}}", "hover": { "color": "{{highlight}}" },
						  "focused": { "borderWidth": "@kit.borderWidth", "borderColor": "@kit.focus" },
						  "onClick": { "set": "{{base + '.menu'}}", "to": "{{d.menu == area ? null : area}}" },
						  "children": [
							{ "type": "label", "icon": "ellipsis", "align": "center", "textColor": "{{activeTextColor}}",
							  "anchors": { "left": 0, "right": 0, "top": 0, "bottom": 0 } },
							{ "type": "column", "role": "menu", "label": "{{moveLabel}}", "arrows": "vertical",
							  "overlay": { "placement": "below-end", "flip": true, "open": "{{d.menu == area}}" },
							  "width": "{{menuWidth}}", "height": "{{3 * 26 + 34}}", "color": "{{menuColor}}",
							  "radius": "@kit.cardRadius", "borderWidth": "@kit.borderWidth", "borderColor": "@kit.border",
							  "keys": { "escape": { "set": "{{base + '.menu'}}", "to": null } },
							  "children": [
								{ "color": "transparent", "height": 4 },
								{ "type": "label", "text": "{{moveLabel}}", "textColor": "{{textColor}}", "height": 24,
								  "anchors": { "left": 12, "right": 8 } },
								{ "repeat": { "for": "to", "in": "{{filter(['left', 'right', 'bottom', 'center'], it != area)}}" },
								  "width": "{{menuWidth}}", "height": 26, "role": "menuitem", "label": "{{areaTitles[to]}}",
								  "focusable": true, "cursor": "hand", "color": "{{menuColor}}",
								  "hover": { "color": "@kit.selection" }, "focused": { "color": "@kit.selection" },
								  "onClick": [
									{ "set": "{{base + '.' + area + '.tabs'}}", "to": "{{filter(tabs, it != panel)}}" },
									{ "set": "{{base + '.' + to + '.tabs'}}", "to": "{{concat(d[to].tabs ?? [], [panel])}}" },
									{ "set": "{{base + '.' + to + '.active'}}", "to": "{{panel}}" },
									{ "set": "{{base + '.' + to + '.collapsed'}}", "to": false },
									{ "set": "{{base + '.maximized'}}", "to": null },
									{ "set": "{{base + '.menu'}}", "to": null }
								  ],
								  "children": [{ "type": "label", "text": "{{areaTitles[to]}}", "textColor": "{{activeTextColor}}",
								                 "anchors": { "left": 12, "right": 8, "top": 0, "bottom": 0 } }] }
							  ] }
						  ] },
						{ "width": 26, "anchors": { "top": 2, "bottom": 2 }, "radius": "@kit.radius", "role": "button",
						  "label": "{{most == area ? restoreLabel : maximizeLabel}}",
						  "tooltip": "{{most == area ? restoreLabel : maximizeLabel}}", "focusable": true, "cursor": "hand",
						  "selected": "{{most == area}}", "color": "{{color}}", "hover": { "color": "{{highlight}}" },
						  "focused": { "borderWidth": "@kit.borderWidth", "borderColor": "@kit.focus" },
						  "onClick": { "set": "{{base + '.maximized'}}", "to": "{{most == area ? null : area}}" },
						  "children": [{ "type": "label", "icon": "{{most == area ? 'minimize-2' : 'maximize-2'}}",
						                 "align": "center", "textColor": "{{activeTextColor}}",
						                 "anchors": { "left": 0, "right": 0, "top": 0, "bottom": 0 } }] },
						{ "when": "{{area != 'center' && most != area}}", "width": 26, "anchors": { "top": 2, "bottom": 2 },
						  "radius": "@kit.radius", "role": "button", "label": "{{collapseLabel}}",
						  "tooltip": "{{collapseLabel}}", "focusable": true, "cursor": "hand", "isOpen": "{{!closed}}",
						  "color": "{{color}}", "hover": { "color": "{{highlight}}" },
						  "focused": { "borderWidth": "@kit.borderWidth", "borderColor": "@kit.focus" },
						  "onClick": { "set": "{{base + '.' + area + '.collapsed'}}", "to": "{{!closed}}" },
						  "children": [{ "type": "label", "icon": "{{closed ? 'chevron-up' : 'minus'}}", "align": "center",
						                 "textColor": "{{activeTextColor}}",
						                 "anchors": { "left": 0, "right": 0, "top": 0, "bottom": 0 } }] },
						{ "color": "transparent", "width": 4, "height": 1 }
					  ] },
					{ "when": "{{!closed}}", "color": "transparent",
					  "anchors": { "left": 0, "right": 0, "top": "{{length(tabs) > 0 ? headerHeight : 0}}", "bottom": 0 },
					  "children": [{ "slot": true }] },
					{ "when": "{{edge}}", "color": "transparent", "role": "separator", "label": "{{resizeLabel}}", "focusable": true,
					  "cursor": "{{side ? 'resize-horizontal' : 'resize-vertical'}}",
					  "anchors": "{{area == 'bottom' ? {left: 0, right: 0, top: 0} : area == 'left' ? {right: 0, top: 0, bottom: 0} : {left: 0, top: 0, bottom: 0}}}",
					  "width": "{{side ? 5 : 0}}", "height": "{{side ? 0 : 5}}", "focused": { "color": "{{sashColor}}" },
					  "keys": {
						"left": { "if": "{{side}}", "then": [{ "set": "{{base + '.sizes.' + area}}", "to": "{{clamp(size[area] - 16 * forward, minSize, maxSize)}}" }] },
						"right": { "if": "{{side}}", "then": [{ "set": "{{base + '.sizes.' + area}}", "to": "{{clamp(size[area] + 16 * forward, minSize, maxSize)}}" }] },
						"up": { "if": "{{!side}}", "then": [{ "set": "{{base + '.sizes.' + area}}", "to": "{{clamp(size[area] + 16, minSize, maxSize)}}" }] },
						"down": { "if": "{{!side}}", "then": [{ "set": "{{base + '.sizes.' + area}}", "to": "{{clamp(size[area] - 16, minSize, maxSize)}}" }] }
					  },
					  "children": [{ "anchors": "{{area == 'bottom' ? {left: 0, right: 0, top: 0} : area == 'left' ? {right: 0, top: 0, bottom: 0} : {left: 0, top: 0, bottom: 0}}}",
					                 "width": "{{side ? 1 : 0}}", "height": "{{side ? 0 : 1}}", "color": "{{borderColor}}" }] }
				]
			}]
		}]
	}
}
})";

} // namespace

std::string_view builtInKitDock() noexcept { return kitDock; }

} // namespace affresco::format
