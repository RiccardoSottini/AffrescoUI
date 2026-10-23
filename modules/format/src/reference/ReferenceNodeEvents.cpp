/// @file
/// @brief format::nodeEventKeys(): the reference of the events, the form's and the carousel's keys, and the keys no
/// table lists.
///
/// Part of nodeKeys() (ReferenceNodes.cpp), which puts it last.

#include "Reference.hpp"

#include <array>

namespace affresco::format {

namespace {

/// The keys, in the order of the tables.
constexpr auto eventTable = std::to_array<KeyReference>({
    {"onClick", "handler", "Left click, two clicks on the same node, right click.", "events"},
    {"onDoubleClick", "handler", "", "events"},
    {"onContextMenu", "handler", "", "events"},
    {"onPress", "handler", "Left button down, up.", "events"},
    {"onRelease", "handler", "", "events"},
    {"onDrag", "handler", "The pointer moves while pressed: `event.dx`, `event.dy` since the press.", "events"},
    {"onDrop", "handler",
	 "A dragged node was dropped here: `event.data` (on `tabs`, also `event.index`, where it goes; on a `list` or "
	 "`tree`, `event.index`, `id` and `place`: `\"before\"`, `\"inside\"` or `\"after\"` that row); or files from the "
	 "OS: `event.files` (their paths).",
	 "events"},
    {"onHover", "handler", "The pointer enters or leaves the node (or a child).", "events"},
    {"onLeave", "handler", "", "events"},
    {"onFocus", "handler", "Keyboard focus.", "events"},
    {"onBlur", "handler", "", "events"},
    {"onScroll", "handler", "`event.dx`, `event.dy`.", "events"},
    {"onKey", "handler", "A key on the focused node: `event.key`, `shortcut`, `shift`, `ctrl`, `alt`, `cmd`.",
	 "events"},
    {"onChange", "",
	 "A bound or value control changed: `event.value`; a timeline's track muted or locked: `event.type` `\"track\"`, "
	 "`id`, `muted`, `locked`.",
	 "events"},
    {"onSubmit", "handler", "Enter in a text area: `event.text`.", "events"},
    {"onLink", "handler", "A press on a part of `runs` with a link: `event.link`.", "events"},
    {"onShow", "handler", "The node was built, or removed (by `when`, `repeat` or a revision).", "events"},
    {"onHide", "handler", "", "events"},
    {"onClose", "handler", "A tab's close button or Delete: `event.id`.", "events"},
    {"onMove", "handler",
	 "A tab dragged along its strip: `event.id`, `from`, `to`; a graph's nodes dragged or moved with the arrows: "
	 "`event.ids`, `dx`, `dy` (in graph units) and `nodes`, each `{ id, index, x, y }`.",
	 "events"},
    {"onExpand", "handler", "A tree row or a timeline's group opened or closed: `event.id`, `expanded`.", "events"},
    {"onOpen", "handler", "Enter or a double click on a tree, list or table row: `event.id`.", "events"},
    {"onSort", "handler", "A table header clicked: `event.column`, `descending`.", "events"},
    {"onColumnResize", "handler", "A table column's edge dragged and let go: `event.column`, `width`.", "events"},
    {"onTransform", "handler",
	 "Canvas items moved, resized, rotated or nudged: `event.items`, each `{ \"id\", \"x\", \"y\", \"width\", "
	 "\"height\", \"rotate\" }`, where they should go; the document applies them.",
	 "events"},
    {"onConnect", "handler",
	 "A graph's link made, by a drag from a port or Enter on two ports: `event.from`, `output`, `to`, `input`.",
	 "events"},
    {"onDisconnect", "handler",
	 "A graph's link removed (Shift+Delete on its port) or its end picked up: `event.index`, `from`, `output`, `to`, "
	 "`input`. Moving an end runs `onDisconnect` then `onConnect`.",
	 "events"},
    {"onLinkDrop", "handler",
	 "A graph's link dropped on empty space: `event.node`, `port`, `output` (whether it started at an output), `x`, "
	 "`y` (in graph units), e.g. to add a node there.",
	 "events"},
    {"onSelect", "handler", "A graph's or a timeline's selection changed: `event.ids` (a timeline's item keys).",
	 "events"},
    {"onDelete", "handler", "Delete or Backspace on a graph: `event.ids`, the selected nodes.", "events"},
    {"onScrub", "handler", "A timeline's playhead moved by the user: `event.time` (seconds), `frame`.", "events"},
    {"onKeyframeMove", "handler",
	 "A timeline's keyframe dragged or moved with Alt and the arrows: `event.track` (its id), `index`, `time`, "
	 "`from`, `value`.",
	 "events"},
    {"onClipChange", "handler",
	 "A timeline's clip moved or trimmed: `event.id`, `track` (where it is now), `fromTrack`, `index` (in "
	 "`fromTrack`), `start`, `duration`, `from` (the start before) and `edge` (`\"move\"`, `\"start\"` or `\"end\"`).",
	 "events"},
    {"onGutterClick", "handler",
	 "A press in an editor's gutter: `event.line` (from 0), `shift`, `alt`, `control`, `super`. While a node has "
	 "one, the press doesn't move the caret.",
	 "events"},
    {"onWordHover", "handler",
	 "The pointer moved onto a word of an editor, or off one: `event.word`, `line`, `column` (where it starts, "
	 "from 0); off a word, `word` is empty and `line` null.",
	 "events"},
    {"onType", "handler",
	 "Typing in an editor that has a `language` or not: `event.text` (what was typed; `\"\"` for a deletion), "
	 "`word` (the word that now ends at the caret, by the language's `wordPattern`), `line`, `column` (the caret, "
	 "from 0), and `x`, `y`, `top` (the caret's box, for a popup).",
	 "events"},
    {"onTextHover", "handler",
	 "The pointer moved over an editor's text: `event.over` (`false` when it left the text), `line`, `column` (from "
	 "0), and `x`, `y`, `top` (the place's box). For hover cards, e.g. a language server's.",
	 "events"},
    {"onPopupKey", "handler",
	 "A popup key while an editor's `popup` is `true`: `event.key` is `up`, `down`, `pageUp`, `pageDown`, `enter`, "
	 "`tab` or `escape`.",
	 "events"},
    {"secret", "flag",
	 "A text area or a code input showing a dot per character, for a password; the screen reader gets no value, and "
	 "the text is never copied.",
	 "form"},
    {"maxLength", R"json({"anyOf": [{"type": "number"}, {"$ref": "#/$defs/v2/dynamic"}]})json",
	 "The most characters typing and pasting may give a text area (0, the default: no limit; up to 100,000).", "form"},
    {"accept", R"json({"anyOf": [{"enum": ["any", "digits", "letters", "alphanumeric"]}, {"$ref": "#/$defs/v2/dynamic"}]})json",
	 "The characters typing and pasting may add: `any` (the default for a text area), `digits` (a code input's "
	 "default), `letters` or `alphanumeric`; others are dropped (a pasted `123-456` gives `123456`).",
	 "form"},
    {"length", R"json({"anyOf": [{"type": "number"}, {"$ref": "#/$defs/v2/dynamic"}]})json",
	 "A code input's cells (1 to 12, default 6), which is also the most characters.", "form"},
    {"cellColor", "color", "A code input's cells, their outline and the space between them (default 8) and their corners "
	 "(default 6).", "form"},
    {"cellBorderColor", "color", "", "form"},
    {"cellGap", R"json({"anyOf": [{"type": "number"}, {"$ref": "#/$defs/v2/dynamic"}]})json", "", "form"},
    {"cellRadius", R"json({"anyOf": [{"type": "number"}, {"$ref": "#/$defs/v2/dynamic"}]})json", "", "form"},
    {"index", R"json({"anyOf": [{"type": "number"}, {"$ref": "#/$defs/v2/dynamic"}]})json",
	 "The page shown, from 0, when there is no `bind` (default 0). With `bind`, the user's turns write the state and "
	 "run `onChange`.",
	 "carousel"},
    {"loop", "flag", "Whether turning past the last page goes to the first, and back (default false).", "carousel"},
    {"arrowButtons", "flag",
	 "The arrows in a gutter on each side, and the dots in a strip below (both default true).", "carousel"},
    {"dots", "flag", "", "carousel"},
    {"arrowColor", "color", "The arrows, and the other pages' dots (`activeColor`: the page shown's).", "carousel"},
    {"dotColor", "color", "", "carousel"},
    {"slot", R"json({"const": true, "description": "In a component: where the use's children go"})json", "", ""},
    {"placeholder", R"json({"description": "Text, or a {{binding}}"})json", "", ""},
    {"placeholderColor", "color", "", ""},
    {"checked", "flag", "", ""},
    {"today", R"json({"description": "calendar: the day outlined, \"YYYY-MM-DD\""})json", "", ""},
    {"marked", R"json({"anyOf": [{"type": "array", "items": {"type": "string"}}, {"$ref": "#/$defs/v2/dynamic"}],
		"description": "calendar: the days with a dot, \"YYYY-MM-DD\" each"})json",
	 "", ""},
    {"firstDayOfWeek", R"json({"anyOf": [{"type": "integer", "minimum": 0, "maximum": 6}, {"$ref": "#/$defs/v2/dynamic"}],
		"description": "calendar: 0 for Sunday to 6 for Saturday (default 1, Monday)"})json",
	 "", ""},
    {"monthNames", R"json({"anyOf": [{"type": "array", "items": {"type": "string"}}, {"$ref": "#/$defs/v2/dynamic"}],
		"description": "calendar: twelve month names, January first"})json",
	 "", ""},
    {"dayNames", R"json({"anyOf": [{"type": "array", "items": {"type": "string"}}, {"$ref": "#/$defs/v2/dynamic"}],
		"description": "calendar: seven short day names, Sunday first"})json",
	 "", ""},
    {"pickedTextColor", "color", "", ""},
});

} // namespace

std::span<const KeyReference> nodeEventKeys() { return eventTable; }

} // namespace affresco::format
