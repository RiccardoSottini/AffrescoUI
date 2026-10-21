/// @file
/// @brief The built-in components (BuiltInComponents.hpp): overlays written in the format itself.

#include "BuiltInComponents.hpp"

#include <map>
#include <string>

namespace affresco::format {

namespace {

/// @brief The colours every menu takes, as parameters.
constexpr std::string_view menuColours =
    R"("color": "#252526", "textColor": "#cccccc", "dimColor": "#8b8b8b", "highlight": "#04395e")";

/// @brief A menu's items: a column of rows (or separators) that the arrows, Home, End and mnemonics move through.
constexpr std::string_view menuItems = R"({
	"type": "column", "width": "{{width}}", "height": "{{sum(items, it.separator == true ? 9 : 26)}}",
	"color": "{{color}}", "radius": 4, "borderWidth": 1, "borderColor": "#454545", "arrows": "vertical",
	"children": [{
		"repeat": { "for": "item", "in": "{{items}}", "index": "i" },
		"anchors": { "left": 0, "right": 0 }, "height": "{{item.separator == true ? 9 : 26}}", "color": "{{color}}",
		"focusable": "{{item.separator != true}}",
		"disabled": "{{item.separator == true || item.enabled == false}}",
		"mnemonic": "{{item.mnemonic ?? item.title ?? ''}}",
		"hover": { "color": "{{item.separator == true || item.enabled == false ? color : highlight}}" },
		"focused": { "color": "{{highlight}}" },
		"onClick": [{ "if": "{{item.submenu != null}}",
		              "then": [{ "toggle": "{{(sub ?? open + 'Sub') + '.m' + i}}" }],
		              "else": [{ "dismiss": true },
		                       { "if": "{{item.run != null}}", "then": [{ "run": "{{item.run}}", "with": "{{item.with}}" }] }] }],
		"children": [
			{ "when": "{{item.separator == true}}", "anchors": { "left": 8, "right": 8, "top": 4 }, "height": 1,
			  "color": "#454545" },
			{ "when": "{{item.separator != true}}", "type": "label", "text": "{{item.title ?? item.name ?? ''}}",
			  "textColor": "{{item.enabled == false ? dimColor : textColor}}", "anchors": { "left": 12, "top": 5 },
			  "fitText": true },
			{ "when": "{{item.separator != true}}", "type": "label",
			  "text": "{{item.submenu != null ? '»' : item.keyLabel ?? ''}}", "textColor": "{{dimColor}}",
			  "anchors": { "right": 12, "top": 5 }, "fitText": true }
		]
	}]
})";

/// @brief Builds a menu component: its items in an overlay that a click outside or Escape closes.
/// @param placement Its default placement.
/// @param extraParams More parameters, as JSON members ("" for none).
/// @param nested Whether its items may open a submenu (one level, as a component can't use itself).
/// @return The definition.
json menu(std::string_view placement, std::string_view extraParams, bool nested) {
	json node = json::parse(menuItems);
	node["overlay"] = {{"anchor", "{{anchor}}"}, {"placement", "{{placement}}"}, {"open", "{{open}}"},
	                   {"dismiss", "outside"},   {"modal", true}};
	if(nested)
		node["children"][0]["children"].push_back(
		    {{"when", "{{item.submenu != null}}"},
		     {"use", "submenu"},
		     {"with",
		      {{"items", "{{item.submenu}}"}, {"open", "{{(sub ?? open + 'Sub') + '.m' + i}}"},
		       {"width", "{{width}}"}, {"color", "{{color}}"}, {"textColor", "{{textColor}}"},
		       {"dimColor", "{{dimColor}}"}, {"highlight", "{{highlight}}"}}}});
	if(placement == "window") { // a context menu, where the pointer is
		node["x"] = "{{x}}";
		node["y"] = "{{y}}";
	}
	json params = json::parse(std::string(R"({"items": [], "open": "state.menuOpen", "sub": null,
		"anchor": "", "width": 240, )") +
	                          std::string(menuColours) + std::string(extraParams) + "}");
	params["placement"] = placement;
	return {{"params", std::move(params)}, {"node", std::move(node)}};
}

/// @brief The other built-ins, in the format.
constexpr std::string_view others = R"({
"menuBar": {
	"params": { "menus": [], "open": "state.menuBar", "height": 30, "width": 240,
	            "color": "#3c3c3c", "textColor": "#cccccc", "dimColor": "#8b8b8b", "highlight": "#505050" },
	"node": {
		"type": "row", "height": "{{height}}", "color": "{{color}}", "arrows": "horizontal",
		"children": [{
			"repeat": { "for": "m", "in": "{{menus}}", "index": "n" },
			"width": "{{len(m.title ?? m.name ?? '') * 8 + 20}}", "anchors": { "top": 0, "bottom": 0 },
			"color": "{{color}}", "focusable": true, "mnemonic": "{{m.mnemonic ?? m.title ?? ''}}",
			"hover": { "color": "{{highlight}}" }, "focused": { "color": "{{highlight}}" },
			"onClick": { "toggle": "{{open + '.' + (m.name ?? n)}}" },
			"children": [
				{ "type": "label", "text": "{{m.title ?? m.name ?? ''}}", "textColor": "{{textColor}}",
				  "anchors": { "left": 10, "top": 7 }, "fitText": true },
				{ "use": "menu", "with": { "items": "{{m.items ?? []}}",
				  "open": "{{open + '.' + (m.name ?? n)}}", "width": "{{width}}" } }
			]
		}]
	}
},
"dialog": {
	"params": { "open": "state.dialogOpen", "title": "", "message": "", "buttons": [{ "title": "OK" }],
	            "width": 420, "height": 160, "color": "#252526", "textColor": "#cccccc", "dimColor": "#a0a0a0",
	            "highlight": "#0e639c", "backdrop": "#00000088" },
	"node": {
		"overlay": { "placement": "window", "open": "{{open}}", "dismiss": "outside", "modal": true },
		"fill": true, "color": "{{backdrop}}",
		"children": [{
			"overlay": { "placement": "center", "open": "{{open}}" },
			"width": "{{width}}", "height": "{{height}}", "color": "{{color}}", "radius": 6,
			"borderWidth": 1, "borderColor": "#454545",
			"children": [
				{ "type": "label", "text": "{{title}}", "textColor": "{{textColor}}",
				  "anchors": { "left": 20, "top": 16 }, "fitText": true },
				{ "type": "label", "text": "{{message}}", "textColor": "{{dimColor}}", "wrap": true,
				  "anchors": { "left": 20, "right": 20, "top": 44, "bottom": 52 } },
				{ "slot": true },
				{ "type": "row", "anchors": { "right": 16, "bottom": 14 }, "height": 28,
				  "width": "{{len(buttons) * 96}}", "gap": 8, "arrows": "horizontal",
				  "children": [{
					"repeat": { "for": "b", "in": "{{buttons}}", "index": "i" },
					"width": 88, "anchors": { "top": 0, "bottom": 0 }, "radius": 4, "focusable": true,
					"color": "{{i == 0 ? highlight : '#3a3d41'}}", "focused": { "borderWidth": 1, "borderColor": "#ffffff" },
					"mnemonic": "{{b.mnemonic ?? b.title ?? ''}}",
					"onClick": [{ "dismiss": true },
					            { "if": "{{b.run != null}}", "then": [{ "run": "{{b.run}}", "with": "{{b.with}}" }] }],
					"children": [{ "type": "label", "text": "{{b.title ?? ''}}", "textColor": "#ffffff",
					               "anchors": { "left": 12, "top": 6 }, "fitText": true }]
				  }]
				}
			]
		}]
	}
},
"toast": {
	"params": { "open": "state.toastOpen", "message": "", "progress": null, "actions": [], "width": 360,
	            "color": "#252526", "textColor": "#cccccc", "accent": "#0e639c" },
	"node": {
		"overlay": { "placement": "window", "open": "{{open}}" },
		"anchors": { "right": 16, "bottom": 16 }, "width": "{{width}}", "height": "{{progress == null ? 64 : 72}}",
		"color": "{{color}}", "radius": 6, "borderWidth": 1, "borderColor": "#454545", "arrows": "horizontal",
		"children": [
			{ "type": "label", "text": "{{message}}", "textColor": "{{textColor}}", "anchors": { "left": 14, "top": 10 },
			  "fitText": true },
			{ "type": "label", "text": "×", "textColor": "{{textColor}}", "anchors": { "right": 12, "top": 8 },
			  "fitText": true, "cursor": "hand", "onClick": { "set": "{{open}}", "to": false } },
			{ "when": "{{progress != null}}", "anchors": { "left": 14, "right": 14, "bottom": 10 }, "height": 3,
			  "color": "#3a3d41",
			  "children": [{ "anchors": { "left": 0, "top": 0, "bottom": 0 }, "color": "{{accent}}",
			                 "width": "{{max(0, min(1, progress ?? 0)) * 100 + '%'}}" }] },
			{ "type": "row", "anchors": { "left": 14, "bottom": "{{progress == null ? 8 : 18}}" }, "height": 22,
			  "width": "{{len(actions) * 96}}", "gap": 8,
			  "children": [{
				"repeat": { "for": "a", "in": "{{actions}}", "index": "i" },
				"width": 88, "anchors": { "top": 0, "bottom": 0 }, "focusable": true, "cursor": "hand",
				"mnemonic": "{{a.mnemonic ?? a.title ?? ''}}",
				"color": "{{color}}", "hover": { "color": "#3a3d41" }, "focused": { "color": "#3a3d41" },
				"onClick": [{ "set": "{{open}}", "to": false },
				            { "if": "{{a.run != null}}", "then": [{ "run": "{{a.run}}", "with": "{{a.with}}" }] }],
				"children": [{ "type": "label", "text": "{{a.title ?? ''}}", "textColor": "{{accent}}",
				               "anchors": { "left": 0, "top": 3 }, "fitText": true }]
			  }]
			}
		]
	}
},
"hoverCard": {
	"params": { "open": "state.hoverCard", "title": "", "text": "", "width": 260, "height": 72,
	            "placement": "below", "color": "#252526", "textColor": "#cccccc", "dimColor": "#a0a0a0" },
	"node": {
		"onHover": { "set": "{{open}}", "to": true }, "onLeave": { "set": "{{open}}", "to": false },
		"children": [
			{ "slot": true },
			{ "overlay": { "placement": "{{placement}}", "open": "{{open}}" },
			  "width": "{{width}}", "height": "{{height}}", "color": "{{color}}", "radius": 6,
			  "borderWidth": 1, "borderColor": "#454545",
			  "children": [
				{ "type": "label", "text": "{{title}}", "textColor": "{{textColor}}", "anchors": { "left": 12, "top": 10 },
				  "fitText": true },
				{ "type": "label", "text": "{{text}}", "textColor": "{{dimColor}}", "wrap": true,
				  "anchors": { "left": 12, "right": 12, "top": 32, "bottom": 8 } }
			  ] }
		]
	}
}
})";

/// @brief Every built-in, by name, made once.
/// @return The map.
const std::map<std::string, json, std::less<>>& all() {
	static const std::map<std::string, json, std::less<>> components = [] {
		std::map<std::string, json, std::less<>> made;
		made.emplace("menu", menu("below", "", true));
		made.emplace("submenu", menu("right", "", false));
		made.emplace("contextMenu", menu("window", R"(, "x": 0, "y": 0)", true));
		for(auto& [name, definition] : json::parse(others).items()) made.emplace(name, std::move(definition));
		return made;
	}();
	return components;
}

} // namespace

const json* builtInComponent(std::string_view name) {
	const auto& components = all();
	const auto found = components.find(name);
	return found == components.end() ? nullptr : &found->second;
}

} // namespace affresco::format
