/// @file
/// @brief The built-in chat components (BuiltInComponents.hpp): chatBubble, typingIndicator, proposalCard,
/// chatComposer and chatLauncher, the parts of a conversation with an assistant.

#include "BuiltInComponents.hpp"

#include <string_view>

namespace affresco::format {

namespace {

/// @brief The chat components, in the format. They hold no state of their own: the document passes the values
/// (the text, the draft, the unread count) and the actions to run (send, close, accept), and each runs a given action
/// with `{ "run", "with" }`, as the dialog's buttons do. Every colour is a parameter, so an app passes its theme's.
/// The composer lays out left to right in every direction, as the text editor starts its lines at the left, so its
/// placeholder sits where the caret is.
constexpr std::string_view chatComponents = R"({
"chatBubble": {
	"params": { "text": "", "mine": false, "from": "", "time": "", "error": false, "maxWidth": 280,
	            "color": "#f6f8fa", "textColor": "#1f2328", "mineColor": "#0969da", "mineTextColor": "#ffffff",
	            "errorColor": "#cf222e" },
	"node": {
		"type": "label", "text": "{{text}}", "wrap": true, "fitText": { "maxWidth": "{{maxWidth}}" }, "padding": 10,
		"radius": 12, "alignSelf": "{{mine ? 'end' : 'start'}}", "justifySelf": "{{mine ? 'end' : 'start'}}",
		"role": "listitem", "tooltip": "{{time}}",
		"label": "{{length(from) > 0 ? from + ': ' + text : text}}",
		"color": "{{mine ? mineColor : color}}",
		"textColor": "{{error ? errorColor : mine ? mineTextColor : textColor}}",
		"borderWidth": "{{error ? 1 : 0}}", "borderColor": "{{errorColor}}"
	}
},
"typingIndicator": {
	"params": { "label": "Typing", "phase": 0, "color": "#f6f8fa", "dotColor": "#8c959f", "activeColor": "#57606a" },
	"node": {
		"width": 56, "height": 32, "radius": 12, "color": "{{color}}", "alignSelf": "start",
		"justifySelf": "start", "role": "status", "live": "polite", "label": "{{label}}",
		"children": [
			{ "anchors": { "left": 12, "top": 12 }, "width": 8, "height": 8, "radius": 4,
			  "color": "{{phase % 3 == 0 ? activeColor : dotColor}}" },
			{ "anchors": { "left": 24, "top": 12 }, "width": 8, "height": 8, "radius": 4,
			  "color": "{{phase % 3 == 1 ? activeColor : dotColor}}" },
			{ "anchors": { "left": 36, "top": 12 }, "width": 8, "height": 8, "radius": 4,
			  "color": "{{phase % 3 == 2 ? activeColor : dotColor}}" }
		]
	}
},
"proposalCard": {
	"params": { "title": "", "summary": "", "status": "pending", "preview": null, "accept": null, "reject": null,
	            "lines": [], "lineFont": "default", "with": null, "width": 300, "height": null,
	            "labels": { "preview": "Preview", "accept": "Accept", "reject": "Reject", "accepted": "Accepted",
	                        "rejected": "Rejected", "closed": "Closed" },
	            "color": "#ffffff", "textColor": "#1f2328", "dimColor": "#57606a", "borderColor": "#d0d7de",
	            "accent": "#1f883d", "accentTextColor": "#ffffff", "buttonColor": "#f6f8fa" },
	"node": {
		"width": "{{width}}", "height": "{{height ?? 100 + 18 * min(length(lines), 3)}}", "radius": 8,
		"color": "{{color}}", "alignSelf": "start",
		"justifySelf": "start", "borderWidth": 1, "borderColor": "{{borderColor}}", "role": "group", "label": "{{title}}",
		"description": "{{summary}}",
		"children": [
			{ "type": "label", "text": "{{title}}", "textColor": "{{textColor}}", "anchors": { "left": 12, "top": 10 },
			  "fitText": { "maxWidth": "{{width - 24}}" } },
			{ "type": "label", "text": "{{summary}}", "textColor": "{{dimColor}}",
			  "anchors": { "left": 12, "top": 32 }, "fitText": { "maxWidth": "{{width - 24}}" } },
			{ "repeat": { "for": "line", "in": "{{slice(lines, 0, 3)}}", "index": "l" }, "type": "label",
			  "text": "{{line}}", "textColor": "{{textColor}}", "font": "{{lineFont}}",
			  "anchors": { "left": 12, "top": "{{54 + 18 * l}}" }, "fitText": { "maxWidth": "{{width - 24}}" } },
			{ "when": "{{status != 'pending' && status != 'previewing'}}", "type": "label", "role": "status",
			  "text": "{{labels[status] ?? status}}", "textColor": "{{status == 'accepted' ? accent : dimColor}}",
			  "anchors": { "left": 12, "bottom": 14 }, "fitText": true },
			{ "when": "{{status == 'pending' || status == 'previewing'}}", "type": "row", "gap": 8,
			  "anchors": { "left": 12, "right": 12, "bottom": 10 }, "height": 28, "arrows": "horizontal",
			  "children": [
				{ "when": "{{preview != null}}", "width": 84, "anchors": { "top": 0, "bottom": 0 }, "radius": 6,
				  "role": "button", "focusable": true, "cursor": "hand", "color": "{{buttonColor}}",
				  "borderWidth": 1, "borderColor": "{{borderColor}}", "selected": "{{status == 'previewing'}}",
				  "focused": { "borderColor": "{{accent}}" }, "label": "{{labels.preview}}",
				  "onClick": [{ "run": "{{preview}}", "with": "{{with}}" }],
				  "children": [{ "type": "label", "text": "{{labels.preview}}", "textColor": "{{textColor}}",
				                 "anchors": { "left": 0, "right": 0, "top": 5 }, "height": 18, "align": "center" }] },
				{ "when": "{{accept != null}}", "width": 84, "anchors": { "top": 0, "bottom": 0 }, "radius": 6,
				  "role": "button", "focusable": true, "cursor": "hand", "color": "{{accent}}",
				  "focused": { "borderWidth": 1, "borderColor": "{{textColor}}" }, "label": "{{labels.accept}}",
				  "onClick": [{ "run": "{{accept}}", "with": "{{with}}" }],
				  "children": [{ "type": "label", "text": "{{labels.accept}}", "textColor": "{{accentTextColor}}",
				                 "anchors": { "left": 0, "right": 0, "top": 5 }, "height": 18, "align": "center" }] },
				{ "when": "{{reject != null}}", "width": 84, "anchors": { "top": 0, "bottom": 0 }, "radius": 6,
				  "role": "button", "focusable": true, "cursor": "hand", "color": "{{buttonColor}}",
				  "borderWidth": 1, "borderColor": "{{borderColor}}",
				  "focused": { "borderColor": "{{accent}}" }, "label": "{{labels.reject}}",
				  "onClick": [{ "run": "{{reject}}", "with": "{{with}}" }],
				  "children": [{ "type": "label", "text": "{{labels.reject}}", "textColor": "{{textColor}}",
				                 "anchors": { "left": 0, "right": 0, "top": 5 }, "height": 18, "align": "center" }] }
			  ] }
		]
	}
},
"chatComposer": {
	"params": { "bind": "state.draft", "value": "", "send": null, "close": null, "with": null, "label": "Message",
	            "placeholder": "", "sendLabel": "Send", "height": 76, "autofocus": false,
	            "color": "#ffffff", "textColor": "#1f2328", "dimColor": "#6e7781", "borderColor": "#d0d7de",
	            "accent": "#0969da", "accentTextColor": "#ffffff" },
	"node": {
		"height": "{{height}}", "color": "{{color}}", "radius": 8, "borderWidth": 1, "borderColor": "{{borderColor}}",
		"dir": "ltr",
		"let": { "ready": "{{send != null && length(trim(value ?? '')) > 0}}" },
		"children": [
			{ "type": "editor", "bind": "{{bind}}", "wrap": true, "label": "{{label}}", "autofocus": "{{autofocus}}",
			  "anchors": { "left": 8, "right": 76, "top": 6, "bottom": 6 }, "color": "{{color}}",
			  "textColor": "{{textColor}}", "caretColor": "{{accent}}", "lineColor": "{{color}}",
			  "keys": {
				"enter": [{ "if": "{{ready}}", "then": [{ "run": "{{send}}", "with": "{{with}}" }] }],
				"escape": [{ "if": "{{close != null}}", "then": [{ "run": "{{close}}", "with": "{{with}}" }] }]
			  } },
			{ "when": "{{length(value ?? '') == 0}}", "type": "label", "text": "{{placeholder}}", "disabled": true,
			  "textColor": "{{dimColor}}", "anchors": { "left": 22, "top": 10 }, "fitText": true },
			{ "anchors": { "right": 8, "bottom": 8 }, "width": 60, "height": 30, "radius": 6, "role": "button",
			  "focusable": true, "cursor": "hand", "label": "{{sendLabel}}", "disabled": "{{!ready}}",
			  "color": "{{ready ? accent : borderColor}}", "focused": { "borderWidth": 1, "borderColor": "{{textColor}}" },
			  "onClick": [{ "if": "{{ready}}", "then": [{ "run": "{{send}}", "with": "{{with}}" }] }],
			  "children": [{ "type": "label", "text": "{{sendLabel}}", "textColor": "{{ready ? accentTextColor : dimColor}}",
			                 "anchors": { "left": 0, "right": 0, "top": 6 }, "height": 18, "align": "center" }] }
		]
	}
},
"chatLauncher": {
	"params": { "open": "state.chatOpen", "isOpen": false, "run": null, "unread": 0, "label": "Assistant",
	            "tooltip": "", "color": "#0969da", "hoverColor": "#0550ae", "iconColor": "#ffffff",
	            "badgeColor": "#cf222e", "badgeTextColor": "#ffffff" },
	"node": {
		"width": 56, "height": 56, "radius": 999, "color": "{{color}}", "hover": { "color": "{{hoverColor}}" },
		"focused": { "borderWidth": 2, "borderColor": "{{iconColor}}" },
		"shadow": { "x": 0, "y": 4, "blur": 12, "color": "#00000040" }, "transition": { "color": 150 },
		"role": "button", "focusable": true, "cursor": "hand", "isOpen": "{{isOpen == true}}",
		"tooltip": "{{length(tooltip) > 0 ? tooltip : label}}",
		"label": "{{unread > 0 ? label + ' (' + unread + ')' : label}}",
		"onClick": [{ "if": "{{run != null}}", "then": [{ "run": "{{run}}" }], "else": [{ "toggle": "{{open}}" }] }],
		"children": [
			{ "when": "{{isOpen != true}}", "anchors": { "left": 15, "top": 17 }, "width": 26, "height": 18,
			  "radius": 6, "color": "{{iconColor}}" },
			{ "when": "{{isOpen != true}}", "anchors": { "left": 19, "top": 29 }, "width": 8, "height": 8,
			  "rotate": 45, "color": "{{iconColor}}" },
			{ "when": "{{isOpen == true}}", "anchors": { "left": 17, "top": 27 }, "width": 22, "height": 3,
			  "radius": 1, "rotate": 45, "color": "{{iconColor}}" },
			{ "when": "{{isOpen == true}}", "anchors": { "left": 17, "top": 27 }, "width": 22, "height": 3,
			  "radius": 1, "rotate": -45, "color": "{{iconColor}}" },
			{ "when": "{{unread > 0}}", "anchors": { "right": 0, "top": 0 }, "width": "{{unread > 9 ? 26 : 20}}",
			  "height": 20, "radius": 999, "color": "{{badgeColor}}",
			  "children": [{ "type": "label", "text": "{{unread > 99 ? '99+' : text(unread)}}",
			                 "textColor": "{{badgeTextColor}}", "anchors": { "left": 0, "right": 0, "top": 1 },
			                 "height": 18, "align": "center" }] }
		]
	}
}
})";

} // namespace

std::string_view builtInChatComponents() noexcept { return chatComponents; }

} // namespace affresco::format
