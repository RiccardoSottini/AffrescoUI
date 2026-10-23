/// @file
/// @brief The built-in components (BuiltInComponents.hpp): overlays written in the format itself.

#include "BuiltInComponents.hpp"

#include <map>
#include <string>

namespace affresco::format {

namespace {

/// @brief The colours every menu takes, as parameters.
constexpr std::string_view menuColours =
    R"("color": "@kit.surface", "textColor": "@kit.text", "dimColor": "@kit.dim", "highlight": "@kit.selection")";

/// @brief A menu's items: a column of rows (or separators) that the arrows, Home, End and mnemonics move through.
constexpr std::string_view menuItems = R"({
	"type": "column", "width": "{{width}}", "height": "{{sum(items, it.separator == true ? 9 : 26)}}",
	"color": "{{color}}", "radius": "@kit.radius", "borderWidth": "@kit.borderWidth", "borderColor": "@kit.border",
	"arrows": "vertical", "role": "menu",
	"children": [{
		"repeat": { "for": "item", "in": "{{items}}", "index": "i" },
		"anchors": { "left": 0, "right": 0 }, "height": "{{item.separator == true ? 9 : 26}}", "color": "{{color}}",
		"role": "{{item.separator == true ? 'separator' : 'menuitem'}}", "label": "{{item.title ?? item.name ?? ''}}",
		"focusable": "{{item.separator != true}}",
		"disabled": "{{item.separator == true || item.enabled == false}}",
		"mnemonic": "{{item.mnemonic ?? item.title ?? ''}}",
		"hover": { "color": "{{item.separator == true || item.enabled == false ? color : highlight}}" },
		"focused": { "color": "{{highlight}}" }, "selected": "{{item.checked == null ? null : item.checked == true}}",
		"onClick": [{ "if": "{{item.submenu != null}}",
		              "then": [{ "toggle": "{{(sub ?? open + 'Sub') + '.m' + i}}" }],
		              "else": [{ "dismiss": true },
		                       { "if": "{{item.run != null}}", "then": [{ "run": "{{item.run}}", "with": "{{item.with}}" }] }] }],
		"children": [
			{ "when": "{{item.separator == true}}", "anchors": { "left": 8, "right": 8, "top": 4 }, "height": 1,
			  "color": "@kit.border" },
			{ "when": "{{item.separator != true && (item.checked == true || icons && item.icon != null)}}", "type": "label",
			  "text": "", "icon": "{{item.checked == true ? 'check' : item.icon}}",
			  "textColor": "{{item.enabled == false ? dimColor : textColor}}", "anchors": { "left": 10, "top": 5 },
			  "fitText": true },
			{ "when": "{{item.separator != true}}", "type": "label", "text": "{{item.title ?? item.name ?? ''}}",
			  "textColor": "{{item.enabled == false ? dimColor : textColor}}",
			  "anchors": { "left": "{{icons || some(items, it.checked != null) ? 34 : 12}}", "top": 5 },
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
		       {"dimColor", "{{dimColor}}"}, {"highlight", "{{highlight}}"}, {"icons", "{{icons}}"}}}});
	if(placement == "window") { // a context menu, where the pointer is
		node["x"] = "{{x}}";
		node["y"] = "{{y}}";
	}
	json params = json::parse(std::string(R"({"items": [], "open": "state.menuOpen", "sub": null,
		"anchor": "", "width": 240, "icons": false, )") +
	                          std::string(menuColours) + std::string(extraParams) + "}");
	params["placement"] = placement;
	return {{"params", std::move(params)}, {"node", std::move(node)}};
}

/// @brief The quick pick: a search field over the matching items of its providers, the recently chosen first. Its
/// state is under state[name]: open, query, index (the selected row), top (the first row shown) and recent.
constexpr std::string_view quickPickNode = R"({
	"overlay": { "placement": "window", "open": "{{'state.' + name + '.open'}}", "dismiss": "outside", "modal": true },
	"x": "{{max(0, (window.width - width) / 2)}}", "y": 48, "width": "{{width}}",
	"height": "{{50 + max(1, min(total, rows)) * rowHeight}}", "color": "{{color}}", "radius": "@kit.cardRadius",
	"borderWidth": "@kit.borderWidth", "borderColor": "@kit.border",
	"let": {
		"base": "{{'state.' + name}}",
		"qp": "{{state[name]}}",
		"q": "{{qp.query ?? ''}}",
		"provider": "{{find(providers, length(it.prefix ?? '') > 0 && indexOf(q, it.prefix) == 0) ?? find(providers, length(it.prefix ?? '') == 0)}}",
		"search": "{{slice(q, length(provider.prefix ?? ''))}}",
		"key": "{{provider.key ?? 'title'}}",
		"home": "{{length(provider.prefix ?? '') == 0}}",
		"recent": "{{home ? qp.recent ?? [] : []}}",
		"recentKeys": "{{map(recent, it[key])}}",
		"recentHits": "{{fuzzySort(recent, search, key)}}",
		"results": "{{slice(concat(recentHits, filter(fuzzySort(provider.items, search, key, limit + length(recent)), !contains(recentKeys, it.item[key]))), 0, limit)}}",
		"total": "{{length(results)}}",
		"sel": "{{clamp(qp.index ?? 0, 0, max(0, total - 1))}}",
		"top": "{{clamp(qp.top ?? 0, 0, max(0, total - rows))}}"
	},
	"keys": {
		"down": [{ "set": "{{base + '.top'}}", "to": "{{max(top, min(sel + 1, total - 1) - rows + 1)}}" },
		         { "set": "{{base + '.index'}}", "to": "{{min(sel + 1, max(0, total - 1))}}" }],
		"up": [{ "set": "{{base + '.top'}}", "to": "{{min(top, max(sel - 1, 0))}}" },
		       { "set": "{{base + '.index'}}", "to": "{{max(sel - 1, 0)}}" }],
		"enter": [{ "if": "{{total > 0}}", "then": [{ "forEach": "{{[results[sel]]}}", "as": "pick", "do": [] }] }]
	},
	"children": [
		{ "type": "textarea", "bind": "{{base + '.query'}}", "autofocus": true,
		  "placeholder": "{{provider.placeholder ?? 'Type to search'}}", "anchors": { "left": 8, "right": 8, "top": 8 },
		  "height": 28, "padding": 6, "color": "{{field}}", "textColor": "{{textColor}}", "placeholderColor": "{{dimColor}}",
		  "onChange": [{ "set": "{{base + '.index'}}", "to": 0 }, { "set": "{{base + '.top'}}", "to": 0 }] },
		{ "when": "{{total == 0}}", "type": "label", "text": "No matching results", "textColor": "{{dimColor}}",
		  "anchors": { "left": 14, "top": 48 }, "fitText": true },
		{ "type": "column", "anchors": { "left": 0, "right": 0, "top": 44 }, "height": "{{min(total, rows) * rowHeight}}",
		  "onScroll": { "set": "{{base + '.top'}}",
		                "to": "{{clamp(top + (event.dy < 0 ? 1 : event.dy > 0 ? -1 : 0), 0, max(0, total - rows))}}" },
		  "children": [{
			"repeat": { "for": "r", "in": "{{slice(results, top, top + rows)}}", "index": "i" },
			"anchors": { "left": 0, "right": 0 }, "height": "{{rowHeight}}", "cursor": "hand",
			"color": "{{top + i == sel ? highlight : color}}", "hover": { "color": "{{highlight}}" },
			"onClick": [{ "set": "{{base + '.index'}}", "to": "{{top + i}}" },
			            { "forEach": "{{[r]}}", "as": "pick", "do": [] }],
			"children": [
				{ "type": "row", "anchors": { "left": 14, "right": 140, "top": "{{(rowHeight - 18) / 2}}" }, "height": 18,
				  "children": [{ "repeat": { "for": "p", "in": "{{r.parts}}" }, "type": "label", "text": "{{p.text}}",
				                 "textColor": "{{p.match ? accent : textColor}}", "fitText": true }] },
				{ "type": "label", "text": "{{top + i < length(recentHits) ? 'recently used' : r.item.detail ?? ''}}",
				  "textColor": "{{dimColor}}", "anchors": { "right": 14, "top": "{{(rowHeight - 18) / 2}}" },
				  "fitText": true }
			]
		  }]
		}
	]
})";

/// @brief What choosing does, with the chosen result as "pick" (captured, as closing and clearing change the
/// results): closes, puts it first in the recent items (for the default provider), clears the query, then runs the
/// item's "run" (with its "with"), or else the provider's "run" with the item as "item".
constexpr std::string_view quickPickChoose = R"([
	{ "set": "{{base + '.open'}}", "to": false },
	{ "if": "{{home}}", "then": [{ "set": "{{base + '.recent'}}",
		"to": "{{slice(concat([pick.item], filter(qp.recent ?? [], it[key] != pick.item[key])), 0, recentLimit)}}" }] },
	{ "forEach": "{{[provider.run]}}", "as": "providerRun", "do": [
		{ "set": "{{base + '.query'}}", "to": "" }, { "set": "{{base + '.index'}}", "to": 0 },
		{ "set": "{{base + '.top'}}", "to": 0 },
		{ "if": "{{pick.item.run != null}}", "then": [{ "run": "{{pick.item.run}}", "with": "{{pick.item.with}}" }],
		  "else": [{ "if": "{{providerRun != null}}", "then": [{ "run": "{{providerRun}}", "with": "{{ {item: pick.item} }}" }] }] }
	] }
])";

/// @brief Builds the quick pick component, its two ways to choose sharing quickPickChoose.
/// @return The definition.
json quickPick() {
	json node = json::parse(quickPickNode);
	const json choose = json::parse(quickPickChoose);
	node["keys"]["enter"][0]["then"][0]["do"] = choose;
	node["children"][2]["children"][0]["onClick"][1]["do"] = choose;
	json params = json::parse(R"({"name": "quickPick", "providers": [], "rows": 10, "rowHeight": 26, "width": 600,
		"limit": 1000, "recentLimit": 10, "color": "@kit.surface", "textColor": "@kit.text", "dimColor": "@kit.dim",
		"accent": "@kit.accent", "highlight": "@kit.selection", "field": "@kit.field"})");
	return {{"params", std::move(params)}, {"node", std::move(node)}};
}

/// @brief The other built-ins, in the format.
constexpr std::string_view others = R"({
"menuBar": {
	"params": { "source": "{{menus}}", "menus": "{{window.menuBar ?? []}}", "open": "state.menuBar", "height": 30,
	            "width": 240,
	            "color": "@kit.bar", "textColor": "@kit.text", "dimColor": "@kit.dim", "highlight": "@kit.barHover" },
	"node": {
		"type": "row", "height": "{{height}}", "color": "{{color}}", "arrows": "horizontal",
		"children": [{
			"repeat": { "for": "m", "in": "{{menus}}", "index": "n" },
			"let": { "key": "{{m.name ?? m.menu ?? n}}", "heading": "{{m.title ?? m.name ?? m.menu ?? ''}}" },
			"width": "{{length(heading) * 8 + 20}}", "anchors": { "top": 0, "bottom": 0 },
			"color": "{{color}}", "focusable": true, "mnemonic": "{{m.mnemonic ?? heading}}",
			"hover": { "color": "{{highlight}}" }, "focused": { "color": "{{highlight}}" },
			"onClick": { "toggle": "{{open + '.' + key}}" },
			"children": [
				{ "type": "label", "text": "{{heading}}", "textColor": "{{textColor}}",
				  "anchors": { "left": 10, "top": 7 }, "fitText": true },
				{ "use": "menu", "with": { "items": "{{m.items ?? source[m.menu ?? m.name ?? ''] ?? []}}",
				  "open": "{{open + '.' + key}}", "width": "{{width}}" } }
			]
		}]
	}
},
"dialog": {
	"params": { "open": "state.dialogOpen", "title": "", "message": "", "buttons": [{ "title": "OK" }],
	            "width": 420, "height": 160, "color": "@kit.surface", "textColor": "@kit.text", "dimColor": "@kit.body",
	            "highlight": "@kit.primary", "backdrop": "@kit.backdrop" },
	"node": {
		"overlay": { "placement": "window", "open": "{{open}}", "dismiss": "outside", "modal": true },
		"fill": true, "color": "{{backdrop}}",
		"children": [{
			"overlay": { "placement": "center", "open": "{{open}}" }, "role": "dialog", "label": "{{title}}",
			"width": "{{width}}", "height": "{{height}}", "color": "{{color}}", "radius": "@kit.cardRadius",
			"borderWidth": "@kit.borderWidth", "borderColor": "@kit.border",
			"children": [
				{ "type": "label", "text": "{{title}}", "textColor": "{{textColor}}",
				  "anchors": { "left": 20, "top": 16 }, "fitText": true },
				{ "type": "label", "text": "{{message}}", "textColor": "{{dimColor}}", "wrap": true,
				  "anchors": { "left": 20, "right": 20, "top": 44, "bottom": 52 } },
				{ "slot": true },
				{ "type": "row", "anchors": { "right": 16, "bottom": 14 }, "height": 28,
				  "width": "{{length(buttons) * 96}}", "gap": 8, "arrows": "horizontal",
				  "children": [{
					"repeat": { "for": "b", "in": "{{buttons}}", "index": "i" },
					"width": 88, "anchors": { "top": 0, "bottom": 0 }, "radius": "@kit.radius", "focusable": true,
					"color": "{{i == 0 ? highlight : '@kit.secondary'}}",
					"focused": { "borderWidth": "@kit.borderWidth", "borderColor": "@kit.focus" },
					"mnemonic": "{{b.mnemonic ?? b.title ?? ''}}",
					"onClick": [{ "dismiss": true },
					            { "if": "{{b.run != null}}", "then": [{ "run": "{{b.run}}", "with": "{{b.with}}" }] }],
					"children": [{ "type": "label", "text": "{{b.title ?? ''}}",
					               "textColor": "{{i == 0 ? '@kit.onPrimary' : '@kit.onSecondary'}}",
					               "anchors": { "left": 12, "top": 6 }, "fitText": true }]
				  }]
				}
			]
		}]
	}
},
"toast": {
	"params": { "open": "state.toastOpen", "message": "", "progress": null, "actions": [], "width": 360,
	            "color": "@kit.surface", "textColor": "@kit.text", "accent": "@kit.primary" },
	"node": {
		"overlay": { "placement": "window", "open": "{{open}}" },
		"anchors": { "right": 16, "bottom": 16 }, "width": "{{width}}", "height": "{{progress == null ? 64 : 72}}",
		"color": "{{color}}", "radius": "@kit.cardRadius", "borderWidth": "@kit.borderWidth",
		"borderColor": "@kit.border", "arrows": "horizontal", "role": "status", "live": "polite",
		"children": [
			{ "type": "label", "text": "{{message}}", "textColor": "{{textColor}}", "anchors": { "left": 14, "top": 10 },
			  "fitText": true },
			{ "type": "label", "text": "×", "textColor": "{{textColor}}", "anchors": { "right": 12, "top": 8 },
			  "fitText": true, "cursor": "hand", "onClick": { "set": "{{open}}", "to": false } },
			{ "when": "{{progress != null}}", "anchors": { "left": 14, "right": 14, "bottom": 10 }, "height": 3,
			  "color": "@kit.secondary",
			  "children": [{ "anchors": { "left": 0, "top": 0, "bottom": 0 }, "color": "{{accent}}",
			                 "width": "{{max(0, min(1, progress ?? 0)) * 100 + '%'}}" }] },
			{ "type": "row", "anchors": { "left": 14, "bottom": "{{progress == null ? 8 : 18}}" }, "height": 22,
			  "width": "{{length(actions) * 96}}", "gap": 8,
			  "children": [{
				"repeat": { "for": "a", "in": "{{actions}}", "index": "i" },
				"width": 88, "anchors": { "top": 0, "bottom": 0 }, "focusable": true, "cursor": "hand",
				"mnemonic": "{{a.mnemonic ?? a.title ?? ''}}",
				"color": "{{color}}", "hover": { "color": "@kit.secondary" }, "focused": { "color": "@kit.secondary" },
				"onClick": [{ "set": "{{open}}", "to": false },
				            { "if": "{{a.run != null}}", "then": [{ "run": "{{a.run}}", "with": "{{a.with}}" }] }],
				"children": [{ "type": "label", "text": "{{a.title ?? ''}}", "textColor": "{{accent}}",
				               "anchors": { "left": 0, "top": 3 }, "fitText": true }]
			  }]
			}
		]
	}
},
"settingsPage": {
	"params": { "query": "state.settingsQuery", "search": "", "color": "@kit.background", "textColor": "@kit.text",
	            "dimColor": "@kit.dim", "accent": "@kit.primary", "field": "@kit.field" },
	"node": {
		"type": "column", "color": "{{color}}", "gap": 12,
		"children": [
			{ "type": "textarea", "bind": "{{query}}", "placeholder": "Search settings", "height": 28, "width": "100%",
			  "color": "{{field}}", "textColor": "{{textColor}}", "placeholderColor": "{{dimColor}}", "padding": 6 },
			{ "type": "column", "grow": 1, "width": "100%", "scroll": "vertical", "gap": 4,
			  "children": [{
				"repeat": { "for": "s", "in": "{{filter(settingsList, contains(lower(it.title + ' ' + it.key + ' ' + it.description), lower(search)))}}" },
				"width": "100%", "height": 56, "color": "{{color}}",
				"children": [
					{ "when": "{{s.modified}}", "anchors": { "left": 0, "top": 6, "bottom": 6 }, "width": 2,
					  "color": "{{accent}}" },
					{ "type": "label", "text": "{{s.title}}", "textColor": "{{textColor}}",
					  "anchors": { "left": 12, "top": 6 }, "fitText": true },
					{ "type": "label", "text": "{{s.description}}", "textColor": "{{dimColor}}",
					  "anchors": { "left": 12, "right": 220, "top": 28 } },
					{ "when": "{{s.type == 'boolean'}}", "type": "checkbox", "bind": "{{'settings.' + s.key}}",
					  "anchors": { "right": 40, "top": 16 }, "width": 18, "height": 18 },
					{ "when": "{{s.type != 'boolean' && s.minimum != null && s.maximum != null}}", "type": "slider",
					  "bind": "{{'settings.' + s.key}}", "min": "{{s.minimum ?? 0}}", "max": "{{s.maximum ?? 1}}",
					  "step": "{{s.type == 'integer' ? 1 : 0}}", "anchors": { "right": 40, "top": 18 }, "width": 160,
					  "height": 20 },
					{ "when": "{{s.type == 'string'}}", "type": "textarea", "bind": "{{'settings.' + s.key}}",
					  "anchors": { "right": 40, "top": 14 }, "width": 160, "height": 24, "color": "{{field}}",
					  "textColor": "{{textColor}}", "padding": 4 },
					{ "when": "{{s.type != 'boolean' && s.type != 'string' && (s.minimum == null || s.maximum == null)}}",
					  "type": "textarea", "text": "{{text(s.value)}}", "anchors": { "right": 40, "top": 14 },
					  "width": 160, "height": 24, "color": "{{field}}", "textColor": "{{textColor}}", "padding": 4,
					  "onChange": { "if": "{{number(event.value) != null}}",
					                "then": [{ "set": "{{'settings.' + s.key}}", "to": "{{number(event.value)}}" }] } },
					{ "when": "{{s.modified}}", "type": "label", "text": "Reset", "textColor": "{{dimColor}}", "cursor": "hand",
					  "anchors": { "right": 12, "top": 16 }, "fitText": true,
					  "onClick": { "remove": "{{'settings.' + s.key}}" } }
				]
			  }]
			}
		]
	}
},
"hoverCard": {
	"params": { "open": "state.hoverCard", "title": "", "text": "", "width": 260, "height": 72,
	            "placement": "below", "color": "@kit.surface", "textColor": "@kit.text", "dimColor": "@kit.body" },
	"node": {
		"onHover": { "set": "{{open}}", "to": true }, "onLeave": { "set": "{{open}}", "to": false },
		"children": [
			{ "slot": true },
			{ "overlay": { "placement": "{{placement}}", "open": "{{open}}" },
			  "width": "{{width}}", "height": "{{height}}", "color": "{{color}}", "radius": "@kit.cardRadius",
			  "borderWidth": "@kit.borderWidth", "borderColor": "@kit.border",
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
		made.emplace("quickPick", quickPick());
		// Kept in a variable: a temporary in a range-for's range isn't kept alive before C++23's P2718 (GCC 15, clang 19)
		json parsed = json::parse(others);
		for(auto& [name, definition] : parsed.items()) made.emplace(name, std::move(definition));
		json form = json::parse(builtInFormComponents());
		for(auto& [name, definition] : form.items()) made.emplace(name, std::move(definition));
		json bars = json::parse(builtInKitBars());
		for(auto& [name, definition] : bars.items()) made.emplace(name, std::move(definition));
		json overlays = json::parse(builtInKitOverlays());
		for(auto& [name, definition] : overlays.items()) made.emplace(name, std::move(definition));
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
