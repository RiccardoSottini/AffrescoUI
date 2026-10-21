/// @file
/// @brief format::nodeKeys(): the reference of the keys a node can have, its events among them.
///
/// Each entry is a key: the schema of its value and its row in SCREEN_FORMAT.md (Reference.hpp). Which keys exist,
/// and in what order, the registries decide; ReferenceTest checks this table against them.

#include "Reference.hpp"

namespace affresco::format {

namespace {

/// The keys, in the order of the tables.
constexpr KeyReference nodeTable[] = {
    {"type", "", "{types}, or a type the application registers.", "node"},
    {"id", R"json({"type": "string", "description": "Addresses the node: #id in edits, nodes.id, anchors, focus"})json",
	 "A name to address the node (`#id` in edits, `nodes.id`, anchors, `focus`). Every node should have one, and no "
	 "two nodes the same (the checker reports a duplicate). `affrescoui ids` gives readable ones to a file's nodes "
	 "(`label-blocked`, `panel-top-bar`), and a node a revision adds without one gets one in the commit. A `use` keeps "
	 "its own id over its component's.",
	 "node"},
    {"style", R"json({"anyOf": [{"type": "string"}, {"type": "array", "items": {"type": "string"}}]})json",
	 "Style name or list of names; may be computed: `\"{{on ? 'chipOn' : 'chip'}}\"`.", "node"},
    {"children", R"json({"type": "array", "items": {"$ref": "#/$defs/v1/node"}})json", "Array of nodes.", "node"},
    {"x", "length", "Position inside the parent (used when the matching edge isn't anchored).", "node"},
    {"y", "length", "", "node"},
    {"width", "length", "Size. The minimum wins over a smaller size.", "node"},
    {"height", "length", "", "node"},
    {"minWidth", "length", "", "node"},
    {"minHeight", "length", "", "node"},
    {"maxWidth", R"json({"description": "The largest width (the minimum wins); \"none\" for no maximum",
		"anyOf": [{"const": "none"}, {"$ref": "#/$defs/v1/length"}]})json",
	 "The largest size (a length, or `\"none\"`, the default). The minimum wins over the maximum, as in CSS.", "node"},
    {"maxHeight", R"json({"description": "The largest height (the minimum wins); \"none\" for no maximum",
		"anyOf": [{"const": "none"}, {"$ref": "#/$defs/v1/length"}]})json",
	 "", "node"},
    {"aspectRatio",
	 R"json({"description": "Width divided by height (1.5 or \"16/9\"): the height follows the width, or the width follows the height when only the height is set",
		"anyOf": [{"type": "number", "exclusiveMinimum": 0}, {"type": "string",
		"pattern": "^[0-9]+(\\.[0-9]+)? */ *[0-9]+(\\.[0-9]+)?$"}, {"$ref": "#/$defs/v1/dynamic"}]})json",
	 "Width divided by height, `1.5` or `\"16/9\"`: the height follows the width, unless only the height is set (a "
	 "non-zero `height`, opposite anchors, or a container's stretch), then the width follows the height. With both "
	 "set, it is ignored, as in CSS.",
	 "node"},
    {"anchors", R"json({"type": "object", "additionalProperties": false,
		"properties": {"top": {"$ref": "#/$defs/v1/edge"}, "bottom": {"$ref": "#/$defs/v1/edge"},
		"left": {"$ref": "#/$defs/v1/edge"}, "right": {"$ref": "#/$defs/v1/edge"}}})json",
	 "`{ \"top\", \"bottom\", \"left\", \"right\" }`: keep that edge this far from the parent's same edge. Anchoring "
	 "opposite edges stretches the node. A value `\"#id.edge + 8\"` puts the edge at another node's edge (`\"top\": "
	 "\"#header.bottom + 12\"`, `\"bottom\": \"#footer.top - 8\"`).",
	 "node"},
    {"fill", R"json({"oneOf": [{"const": true}, {"$ref": "#/$defs/v1/length"}],
		"description": "Anchor all four edges, this far in"})json",
	 "Anchor all four edges this far in (`0` or `true` to cover the parent).", "node"},
    {"grow", R"json({"anyOf": [{"type": "number"}, {"$ref": "#/$defs/v1/dynamic"}]})json",
	 "In a `row`/`column`: the share of the spare room it takes, like CSS flex-grow.", "node"},
    {"shrink",
	 R"json({"description": "In a row or column: how much of an overflow it gives up, in proportion to its size (CSS flex-shrink). Default 0",
		"anyOf": [{"type": "number", "minimum": 0}, {"$ref": "#/$defs/v1/dynamic"}]})json",
	 "In a `row`/`column`: how much of an overflow it gives up, in proportion to `shrink` times its size, down to its "
	 "minimum (CSS flex-shrink). Default `0`: it keeps its size and the children overflow, as they always have (CSS's "
	 "default is 1).",
	 "node"},
    {"basis",
	 R"json({"description": "In a row or column: its size along it before growing or shrinking (CSS flex-basis); \"auto\" for its own",
		"anyOf": [{"const": "auto"}, {"$ref": "#/$defs/v1/length"}]})json",
	 "In a `row`/`column`: its size along it before growing or shrinking (CSS flex-basis), a length; `\"auto\"` (the "
	 "default) for its own `width` or `height`.",
	 "node"},
    {"alignSelf",
	 R"json({"description": "In a row, column or grid: where it sits across, over the container's alignItems",
		"$ref": "#/$defs/v1/crossAlign"})json",
	 "In a `row`/`column`/`grid`: where it sits across (`start`, `center`, `end`, `stretch`), over the container's "
	 "`alignItems`.",
	 "node"},
    {"cell",
	 R"json({"description": "In a grid: its first column and row (from 1) and how many it spans; left out, the next free cell",
		"type": "object", "additionalProperties": false, "properties": {"column": {"type": "integer",
		"minimum": 1, "maximum": 1000}, "row": {"type": "integer", "minimum": 1, "maximum": 1000},
		"columnSpan": {"type": "integer", "minimum": 1, "maximum": 1000},
		"rowSpan": {"type": "integer", "minimum": 1, "maximum": 1000}}})json",
	 "In a `grid`: `{ \"column\": 2, \"row\": 1, \"columnSpan\": 2, \"rowSpan\": 1 }`, counted from 1 (each key "
	 "optional). Without `column` and `row`, it takes the next free cell, row after row.",
	 "node"},
    {"color", "color", "Background. Default: light grey for panels, transparent for labels, stacks and controls.",
	 "look"},
    {"radius",
	 R"json({"description": "Rounds the corners: one radius for all four, or one per corner (left out: square)",
		"anyOf": [{"$ref": "#/$defs/v1/pixels"}, {"type": "object", "additionalProperties": false,
		"properties": {"topLeft": {"$ref": "#/$defs/v1/pixels"},
		"topRight": {"$ref": "#/$defs/v1/pixels"}, "bottomRight": {"$ref": "#/$defs/v1/pixels"},
		"bottomLeft": {"$ref": "#/$defs/v1/pixels"}}}]})json",
	 "Rounds the corners, in pixels: one number for all four (`8`), or `{ \"topLeft\", \"topRight\", \"bottomRight\", "
	 "\"bottomLeft\" }` (corners left out are square). A radius larger than half the shorter side is clamped to it, so "
	 "`999` makes a pill or a circle. Edges are anti-aliased. Children are still clipped to the square bounds, and the "
	 "corners still take clicks. Default: `0`.",
	 "look"},
    {"borderWidth", R"json({"description": "A border inside the node's edges; it doesn't change the layout",
		"$ref": "#/$defs/v1/pixels"})json",
	 "A border drawn inside the node's edges (it doesn't change the layout), over the background and under the "
	 "children, following `radius`. Default: no border; `borderColor` defaults to black.",
	 "look"},
    {"borderColor", R"json({"description": "Default: black", "$ref": "#/$defs/v1/color"})json", "", "look"},
    {"shadow", R"json({"type": "object", "additionalProperties": false,
		"description": "Like CSS box-shadow: the node's shape, moved and blurred, under it (clipped by the parent)",
		"properties": {"x": {"anyOf": [{"type": "number"}, {"$ref": "#/$defs/v1/dynamic"}]},
		"y": {"anyOf": [{"type": "number"}, {"$ref": "#/$defs/v1/dynamic"}]},
		"blur": {"description": "A Gaussian blur with a standard deviation of blur / 2",
		"$ref": "#/$defs/v1/pixels"}, "color": {"description": "Default: #00000040",
		"$ref": "#/$defs/v1/color"}}})json",
	 "`{ \"x\": 0, \"y\": 4, \"blur\": 12, \"color\": \"#00000040\" }`, like CSS box-shadow: the node's shape, moved "
	 "by `x` and `y` and blurred by `blur` (a Gaussian with a standard deviation of `blur / 2`), drawn under the node. "
	 "Every key is optional; `color` defaults to `#00000040`. A shadow can fall outside the node, but it is clipped by "
	 "the parent, like the node itself.",
	 "look"},
    {"visible", "flag", "`false` hides the node and its children.", "look"},
    {"cursor", R"json({"anyOf": [{"enum": ["arrow", "ibeam", "hand", "resize-horizontal", "resize-vertical"]},
		{"$ref": "#/$defs/v1/dynamic"}]})json",
	 "`arrow`, `ibeam`, `hand`, `resize-horizontal`, `resize-vertical`.", "look"},
    {"tooltip", R"json({"description": "Text, or a {{binding}}"})json",
	 "Text shown after the pointer rests on the node.", "look"},
    {"disabled", "flag", "`true`: no events (hover styling still applies).", "look"},
    {"scroll", R"json({"anyOf": [{"enum": ["vertical", "horizontal", "both", "none"]}, {"type": "boolean"},
		{"$ref": "#/$defs/v1/dynamic"}]})json",
	 "`\"vertical\"`, `\"horizontal\"`, `\"both\"` or `\"none\"`: the wheel or trackpad scrolls the children; a thin "
	 "bar shows where.",
	 "look"},
    {"overlay", R"json({"type": "object", "additionalProperties": false,
		"description": "Drawn above everything, next to its anchor",
		"properties": {"anchor": {"type": "string"}, "placement": {"enum": ["below", "below-end",
		"above", "above-end", "right", "left", "center", "window"]},
		"open": {"description": "A state path (\"state.menuOpen\") or a value"},
		"dismiss": {"const": "outside"}}})json",
	 "`{ \"anchor\": \"#id\", \"placement\": \"below\", \"open\": \"state.menuOpen\", \"dismiss\": \"outside\" }`: "
	 "drawn above everything, unclipped, next to its anchor (default: its parent). Placements: `below`, `below-end`, "
	 "`above`, `above-end`, `right`, `left`, `center` (in the window), `window` (placed by its own x, y, anchors in "
	 "the window). `open` is a state path (or a value); with `\"dismiss\": \"outside\"`, a click outside it or Escape "
	 "sets it to false.",
	 "look"},
    {"hover", "stateBlock",
	 "Properties shown in that state (later ones win: pressed over drag over over dragging over focused over hover). "
	 "Any visual property: `{ \"color\", \"textColor\", \"borderColor\", \"shadow\", \"style\", ... }`. A node counts "
	 "as hovered and pressed while one of its children is.",
	 "look"},
    {"focused", "stateBlock", "", "look"},
    {"dragging", "stateBlock", "", "look"},
    {"dragOver", "stateBlock", "", "look"},
    {"pressed", "stateBlock", "", "look"},
    {"transition", R"json({"type": "object", "additionalProperties": false, "properties": {"color": {"type": "number"},
		"textColor": {"type": "number"}, "borderColor": {"type": "number"}}})json",
	 "`{ \"color\": 150, \"textColor\": 150, \"borderColor\": 150 }`: changes of these colours animate over that many "
	 "milliseconds.",
	 "look"},
    {"repeat", R"json({"type": "object", "required": ["in"], "additionalProperties": false,
		"description": "One node per list item", "properties": {"for": {"type": "string"},
		"in": {"description": "A path like \"state.items\", a data model of the application like \"models.files\", or any value"},
		"index": {"type": "string"},
		"key": {"description": "Identifies an item when the list is reordered"}}})json",
	 "`{ \"for\": \"card\", \"in\": \"state.cards\", \"index\": \"i\", \"key\": \"{{card.id}}\" }`: one node per list "
	 "item. With `key`, a node follows its item when the list is reordered (keeping focus and scroll).",
	 "look"},
    {"when", R"json({"description": "Build the node only if truthy"})json",
	 "Build the node only if the value is truthy.", "look"},
    {"let", R"json({"type": "object", "description": "Named values for the node and its children, in order"})json",
	 "`{ \"on\": \"{{tag == state.filter}}\" }`: named values for the node and its children, evaluated in order.",
	 "look"},
    {"use", R"json({"type": "string", "description": "A component from \"components\""})json",
	 "Build a component: `{ \"use\": \"chip\", \"with\": { \"label\": \"{{tag}}\" } }` (see Components).", "look"},
    {"with", R"json({"type": "object", "description": "A component's parameters"})json", "", "look"},
    {"check", R"json({"type": "object", "additionalProperties": false, "description": "Gives valid and error",
		"properties": {"rule": {}, "message": {}, "value": {}}})json",
	 "`{ \"rule\": \"{{len(value) >= 3}}\", \"message\": \"Too short\", \"value\": ... }`: `value` defaults to the "
	 "node's bound value; gives the node and its children `valid` and `error`.",
	 "look"},
    {"every", R"json({"type": "object", "required": ["ms", "do"], "additionalProperties": false,
		"properties": {"ms": {"anyOf": [{"type": "number"}, {"$ref": "#/$defs/v1/dynamic"}]},
		"do": {"$ref": "#/$defs/v1/handler"}}})json",
	 "`{ \"ms\": 1000, \"do\": steps }`: runs while the node exists.", "look"},
    {"drag", R"json({"type": "object", "additionalProperties": false, "properties": {"data": {}}})json",
	 "`{ \"data\": {...} }`: the node can be dragged; a node with `onDrop` receives `event.data`.", "look"},
    {"keys", R"json({"type": "object", "additionalProperties": {"$ref": "#/$defs/v1/handler"},
		"description": "Shortcuts while focused: \"mod+k\" -> handler"})json",
	 "Shortcuts while the node or a child has the focus: `\"enter\" → handler`.", "look"},
    {"locked", R"json({"type": "boolean"})json",
	 "`true`: no revision may change this node or anything inside it. Needs an `id`.", "look"},
    {"note", R"json({"type": "string", "description": "A comment"})json", "A comment.", "look"},
    {"on…", "", "Event handlers (see Events).", "look"},
    {"text", R"json({"description": "Text, or a {{binding}}"})json", "The text.", "label"},
    {"textColor", "color", "Default black.", "label"},
    {"font", R"json({"type": "string"})json", "Font name. Default: `default`.", "label"},
    {"padding", R"json({"anyOf": [{"type": "number"}, {"$ref": "#/$defs/v1/dynamic"}]})json",
	 "Pixels between the edges and the text.", "label"},
    {"align", R"json({"oneOf": [{"$ref": "#/$defs/v1/align"}, {"type": "object", "additionalProperties": false,
		"properties": {"horizontal": {"$ref": "#/$defs/v1/align"},
		"vertical": {"$ref": "#/$defs/v1/align"}}}],
		"description": "label: text alignment; stack: v1 name of \"from\""})json",
	 "One value for both axes, or `{ \"horizontal\", \"vertical\" }`.", "label"},
    {"wrap", "flag", "Break lines that don't fit the width.", "label"},
    {"fitText", R"json({"oneOf": [{"type": "boolean"}, {"type": "object", "required": ["maxWidth"],
		"additionalProperties": false, "properties": {"maxWidth": {"type": "number",
		"exclusiveMinimum": 0}}}]})json",
	 "`true` or `{ \"maxWidth\" }`: the label sizes itself to its text (plus padding).", "label"},
    {"icon", R"json({"type": "string",
		"description": "label: an icon before the text, by its glyph name in the label's font or its fallbacks (e.g. Lucide's \"search\")"})json",
	 "An icon before the text, by name: `\"search\"`, `\"arrow-left\"`. A glyph of that name in the label's font or "
	 "its fallbacks, drawn in `textColor` with a space before the text (alone if there is no text). A name no font has "
	 "shows nothing. Give the text font an icon font as a fallback (`\"fallback\": [\"icons\"]`) and icons are drawn "
	 "at the text's size, in the same batch; or give an icon-only label the icon font itself, at its own size. The "
	 "examples ship Lucide (`examples/assets/fonts/Lucide.ttf`, 1,890 icons: https://lucide.dev/icons). Not on text "
	 "areas.",
	 "label"},
    {"bind", R"json({"type": "string",
		"description": "A state path kept in sync both ways (textarea, editor, toggle, checkbox, slider)"})json",
	 "A state path holding the text (lines joined by `\"\\n\"`), kept in sync both ways. Without it, `text` sets the "
	 "text.",
	 "editor"},
    {"caret", R"json({"type": "string",
		"description": "editor: a state path the editor writes its caret to, as { line, column, selected } (from 1)"})json",
	 "A state path the editor writes its caret to: `{ \"line\", \"column\", \"selected\" }`, counted from 1.",
	 "editor"},
    {"font", "", "Font and text colour.", "editor"},
    {"textColor", "", "", "editor"},
    {"lineNumbers", R"json({"type": "boolean", "description": "editor: show line numbers in a gutter"})json",
	 "`true` shows line numbers in a gutter.", "editor"},
    {"lineNumberColor", "color", "Line numbers, and the caret's line number.", "editor"},
    {"activeLineNumberColor", "color", "", "editor"},
    {"lineColor", "color", "The caret's line background, the selection, the caret.", "editor"},
    {"selectionColor", "color", "", "editor"},
    {"caretColor", "color", "", "editor"},
    {"autofocus", R"json({"type": "boolean"})json", "Focus when built; `event.value` is the new text.", "editor"},
    {"onChange", "handler", "", "editor"},
    {"gap", R"json({"anyOf": [{"type": "number"}, {"$ref": "#/$defs/v1/dynamic"}]})json",
	 "Pixels between children (and between lines, when it wraps).", "stack"},
    {"rowGap",
	 R"json({"description": "Pixels between rows (a column's children, a wrapping row's lines, a grid's rows); default: gap",
		"anyOf": [{"type": "number", "minimum": 0}, {"$ref": "#/$defs/v1/dynamic"}]})json",
	 "Pixels between rows and between columns, over `gap`: in a column `rowGap` is between children, in a wrapping row "
	 "between lines.",
	 "stack"},
    {"columnGap",
	 R"json({"description": "Pixels between columns (a row's children, a wrapping column's lines, a grid's columns); default: gap",
		"anyOf": [{"type": "number", "minimum": 0}, {"$ref": "#/$defs/v1/dynamic"}]})json",
	 "", "stack"},
    {"justify",
	 R"json({"description": "row, column, stack: how the children are spread along it (from is the older name of the first three)",
		"anyOf": [{"enum": ["start", "center", "end", "space-between", "space-around",
		"space-evenly"]}, {"$ref": "#/$defs/v1/dynamic"}]})json",
	 "How the children are spread along it: `start` (default), `center`, `end` (a column from the bottom, like a chat: "
	 "overflow is clipped at the top), `space-between`, `space-around` or `space-evenly`, as in CSS.",
	 "stack"},
    {"from", "align",
	 "The older form of `justify`, with `start`, `center` or `end`. `align` is an older name for it on `stack`.",
	 "stack"},
    {"alignItems",
	 R"json({"description": "row, column, stack: where children sit across (left out: their own anchors); grid: down their cells (default start)",
		"$ref": "#/$defs/v1/crossAlign"})json",
	 "Where the children sit across: `start`, `center`, `end` or `stretch` (as tall as a row, as wide as a column, "
	 "within their maximum). Left out, they keep their own anchors across.",
	 "stack"},
    {"direction", R"json({"anyOf": [{"enum": ["row", "column"]}, {"$ref": "#/$defs/v1/dynamic"}]})json",
	 "On a `stack`: `\"row\"` or `\"column\"`.", "stack"},
    {"wrap", "",
	 "Continue on a new line (in a column, a new column) when a child doesn't fit; each line grows, shrinks and is "
	 "spread on its own, and sits at the start of its line unless aligned.",
	 "stack"},
    {"columns",
	 R"json({"description": "grid: a number of equal columns, or a list of tracks, e.g. [200, \"1fr\", \"minmax(100, 2fr)\"]",
		"anyOf": [{"type": "integer", "minimum": 1, "maximum": 1000}, {"type": "array", "minItems": 1,
		"maxItems": 1000, "items": {"$ref": "#/$defs/v1/track"}}]})json",
	 "A number of equal columns (default `2`), or a list of tracks: pixels (`200`), a percentage of the grid "
	 "(`\"25%\"`), a share of the room left (`\"1fr\"`), `\"auto\"` (as wide as its widest child) or `\"minmax(100, "
	 "1fr)\"`. `\"1fr\"` is `minmax(0, 1fr)`, so equal fractions stay equal whatever their content; write "
	 "`\"minmax(auto, 1fr)\"` to keep a track as wide as its content. At most 1,000.",
	 "grid"},
    {"rows", R"json({"description": "grid: the rows' tracks; rows past them are as tall as their tallest child",
		"type": "array", "maxItems": 1000, "items": {"$ref": "#/$defs/v1/track"}})json",
	 "Tracks for the rows, as `columns`. Rows past them, and every row without `rows`, are as tall as their tallest "
	 "child.",
	 "grid"},
    {"gap", "", "Pixels between cells; `rowGap` and `columnGap` over `gap`.", "grid"},
    {"rowGap", "", "", "grid"},
    {"columnGap", "", "", "grid"},
    {"justifyItems", R"json({"description": "grid: where children sit across their cells (default stretch)",
		"$ref": "#/$defs/v1/crossAlign"})json",
	 "Where children sit across their cells: `stretch` (default), `start`, `center` or `end`.", "grid"},
    {"alignItems", "", "Where children sit down their cells: `start` (default), `center`, `end` or `stretch`.", "grid"},
    {"trackColor", "color", "The empty or off part.", "control"},
    {"activeColor", "color", "The filled or on part.", "control"},
    {"knobColor", "color", "The knob (or the inside of the checkbox).", "control"},
    {"min", R"json({"anyOf": [{"type": "number"}, {"$ref": "#/$defs/v1/dynamic"}]})json",
	 "Slider range (default 0 to 100) and snapping.", "control"},
    {"max", R"json({"anyOf": [{"type": "number"}, {"$ref": "#/$defs/v1/dynamic"}]})json", "", "control"},
    {"step", R"json({"anyOf": [{"type": "number"}, {"$ref": "#/$defs/v1/dynamic"}]})json", "", "control"},
    {"file", R"json({"type": "string",
		"description": "image: a PNG or JPEG file, relative to the screen file and inside its folder (or one the host allows)"})json",
	 "A PNG or JPEG file, relative to the screen file. It must be inside the screen file's folder (or the folders "
	 "inside it), or a folder the host allows (`Screen::allowImageDirectory`): a revision naming any other file is "
	 "refused. At most 64 MiB, 8,192 pixels a side and 4,096 × 4,096 pixels in all. A file that can't be read or "
	 "decoded shows nothing (and is logged).",
	 "image"},
    {"source", R"json({"type": "string",
		"description": "image: an image the host names (Screen::setImage), instead of a file"})json",
	 "Instead of `file`: an image the host names (`Screen::setImage(\"chart\", …)`), e.g. a picture it made; the node "
	 "shows each new one the host sets.",
	 "image"},
    {"fit", R"json({"anyOf": [{"enum": ["contain", "cover", "fill"]}, {"$ref": "#/$defs/v1/dynamic"}],
		"description": "image: contain (default), cover or fill, like CSS object-fit"})json",
	 "`\"contain\"` (the default: all of it, as large as fits, centred), `\"cover\"` (the whole node, the image's "
	 "edges cut off) or `\"fill\"` (stretched), like CSS `object-fit`.",
	 "image"},
    {"onClick", "handler", "Left click, two clicks on the same node, right click.", "events"},
    {"onDoubleClick", "handler", "", "events"},
    {"onContextMenu", "handler", "", "events"},
    {"onPress", "handler", "Left button down, up.", "events"},
    {"onRelease", "handler", "", "events"},
    {"onDrag", "handler", "The pointer moves while pressed: `event.dx`, `event.dy` since the press.", "events"},
    {"onDrop", "handler", "A dragged node was dropped here: `event.data`.", "events"},
    {"onHover", "handler", "The pointer enters or leaves the node (or a child).", "events"},
    {"onLeave", "handler", "", "events"},
    {"onFocus", "handler", "Keyboard focus.", "events"},
    {"onBlur", "handler", "", "events"},
    {"onScroll", "handler", "`event.dx`, `event.dy`.", "events"},
    {"onKey", "handler", "A key on the focused node: `event.key`, `shortcut`, `shift`, `ctrl`, `alt`, `cmd`.",
	 "events"},
    {"onChange", "", "A bound or value control changed: `event.value`.", "events"},
    {"onSubmit", "handler", "Enter in a text area: `event.text`.", "events"},
    {"onShow", "handler", "The node was built, or removed (by `when`, `repeat` or a revision).", "events"},
    {"onHide", "handler", "", "events"},
    {"slot", R"json({"const": true, "description": "In a component: where the use's children go"})json", "", ""},
    {"placeholder", R"json({"description": "Text, or a {{binding}}"})json", "", ""},
    {"placeholderColor", "color", "", ""},
    {"checked", "flag", "", ""},
    {"value", R"json({"anyOf": [{"type": "number"}, {"$ref": "#/$defs/v1/dynamic"}]})json", "", ""},
};

} // namespace

std::span<const KeyReference> nodeKeys() { return nodeTable; }

} // namespace affresco::format
