/// @file
/// @brief format::nodeKeys(): the reference of the keys a node can have, its events among them.
///
/// Each entry is a key: the schema of its value and its row in SCREEN_FORMAT.md (Reference.hpp). Which keys exist,
/// and in what order, the registries decide; ReferenceTest checks this table against them.
/// The table is in three files, to keep each short: this one, ReferenceNodeTypes.cpp and ReferenceNodeEvents.cpp.

#include "Reference.hpp"

#include <array>
#include <initializer_list>
#include <span>

namespace affresco::format {

namespace {

/// The keys every node has and the label's, in the order of the tables.
constexpr auto commonTable = std::to_array<KeyReference>({
    {"type", "", "{types}, or a type the application registers.", "node"},
    {"id", R"json({"type": "string", "description": "Addresses the node: #id in edits, nodes.id, anchors, focus"})json",
	 "A name to address the node (`#id` in edits, `nodes.id`, anchors, `focus`). Every node should have one, and no "
	 "two nodes the same (the checker reports a duplicate). `affrescoui ids` gives readable ones to a file's nodes "
	 "(`label-blocked`, `panel-top-bar`), and a node a revision adds without one gets one in the commit. A `use` keeps "
	 "its own id over its component's.",
	 "node"},
    {"style", R"json({"anyOf": [{"type": "string"}, {"type": "array", "items": {"type": "string"}}]})json",
	 "Style name or list of names; may be computed: `\"{{on ? 'chipOn' : 'chip'}}\"`.", "node"},
    {"children", R"json({"type": "array", "items": {"$ref": "#/$defs/v2/node"}})json", "Array of nodes.", "node"},
    {"x", "length", "Position inside the parent (used when the matching edge isn't anchored).", "node"},
    {"y", "length", "", "node"},
    {"width", "length", "Size. The minimum wins over a smaller size.", "node"},
    {"height", "length", "", "node"},
    {"minWidth", "length", "", "node"},
    {"minHeight", "length", "", "node"},
    {"maxWidth", R"json({"description": "The largest width (the minimum wins); \"none\" for no maximum",
		"anyOf": [{"const": "none"}, {"$ref": "#/$defs/v2/length"}]})json",
	 "The largest size (a length, or `\"none\"`, the default). The minimum wins over the maximum, as in CSS.", "node"},
    {"maxHeight", R"json({"description": "The largest height (the minimum wins); \"none\" for no maximum",
		"anyOf": [{"const": "none"}, {"$ref": "#/$defs/v2/length"}]})json",
	 "", "node"},
    {"aspectRatio",
	 R"json({"description": "Width divided by height (1.5 or \"16/9\"): the height follows the width, or the width follows the height when only the height is set",
		"anyOf": [{"type": "number", "exclusiveMinimum": 0}, {"type": "string",
		"pattern": "^[0-9]+(\\.[0-9]+)? */ *[0-9]+(\\.[0-9]+)?$"}, {"$ref": "#/$defs/v2/dynamic"}]})json",
	 "Width divided by height, `1.5` or `\"16/9\"`: the height follows the width, unless only the height is set (a "
	 "non-zero `height`, opposite anchors, or a container's stretch), then the width follows the height. With both "
	 "set, it is ignored, as in CSS.",
	 "node"},
    {"anchors", R"json({"oneOf": [{"type": "object", "additionalProperties": false,
		"properties": {"top": {"$ref": "#/$defs/v2/edge"}, "bottom": {"$ref": "#/$defs/v2/edge"},
		"left": {"$ref": "#/$defs/v2/edge"}, "right": {"$ref": "#/$defs/v2/edge"}}},
		{"type": "string", "pattern":
		"^ *(top|bottom|left|right)(:-?[0-9.]+(px|%)?)?( +(top|bottom|left|right)(:-?[0-9.]+(px|%)?)?)* *$"}]})json",
	 "`{ \"top\", \"bottom\", \"left\", \"right\" }`: keep that edge this far from the parent's same edge. Anchoring "
	 "opposite edges stretches the node. A value `\"#id.edge + 8\"` puts the edge at another node's edge (`\"top\": "
	 "\"#header.bottom + 12\"`, `\"bottom\": \"#footer.top - 8\"`). The same as text: edge names, each at 0 or "
	 "at a length after a colon, `\"left right:12 top:8\"` is `{ \"left\": 0, \"right\": 12, \"top\": 8 }`.",
	 "node"},
    {"fill", R"json({"oneOf": [{"const": true}, {"$ref": "#/$defs/v2/length"}],
		"description": "Anchor all four edges, this far in"})json",
	 "Anchor all four edges this far in (`0` or `true` to cover the parent).", "node"},
    {"grow", R"json({"anyOf": [{"type": "number"}, {"$ref": "#/$defs/v2/dynamic"}]})json",
	 "In a `row`/`column`: the share of the spare room it takes, like CSS flex-grow.", "node"},
    {"shrink",
	 R"json({"description": "In a row or column: how much of an overflow it gives up, in proportion to its size (CSS flex-shrink). Default 0",
		"anyOf": [{"type": "number", "minimum": 0}, {"$ref": "#/$defs/v2/dynamic"}]})json",
	 "In a `row`/`column`: how much of an overflow it gives up, in proportion to `shrink` times its size, down to its "
	 "minimum (CSS flex-shrink). Default `0`: it keeps its size and the children overflow, as they always have (CSS's "
	 "default is 1).",
	 "node"},
    {"basis",
	 R"json({"description": "In a row or column: its size along it before growing or shrinking (CSS flex-basis); \"auto\" for its own",
		"anyOf": [{"const": "auto"}, {"$ref": "#/$defs/v2/length"}]})json",
	 "In a `row`/`column`: its size along it before growing or shrinking (CSS flex-basis), a length; `\"auto\"` (the "
	 "default) for its own `width` or `height`.",
	 "node"},
    {"alignSelf",
	 R"json({"description": "In a row, column or grid: where it sits across, over the container's alignItems",
		"$ref": "#/$defs/v2/crossAlign"})json",
	 "In a `row`/`column`/`grid`: where it sits across (`start`, `center`, `end`, `stretch`), over the container's "
	 "`alignItems`.",
	 "node"},
    {"justifySelf",
	 R"json({"description": "In a grid: where it sits along its cell's row, over the grid's justifyItems",
		"$ref": "#/$defs/v2/crossAlign"})json",
	 "In a `grid`: where it sits along its cell's row (`start`, `center`, `end`, `stretch`), over the grid's "
	 "`justifyItems`.",
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
    {"dir", R"json({"description": "Which way its children and text run: ltr, rtl, auto (the locale's) or inherit",
		"anyOf": [{"enum": ["ltr", "rtl", "auto", "inherit"]}, {"$ref": "#/$defs/v2/dynamic"}]})json",
	 "Which way its children and text run, inherited: `\"ltr\"`, `\"rtl\"`, `\"auto\"` (the locale's: Arabic and "
	 "Hebrew are right to left) or `\"inherit\"` (the default; the root is left to right). Right to left mirrors row "
	 "order, grid columns, left and right anchors, `x`, padding and scrolling, swaps a label's `align` start and end, "
	 "and draws an icon marked `mirror` mirrored.",
	 "node"},
    {"color", "color", "Background. Default: light grey for panels, transparent for labels, stacks and controls.",
	 "look"},
    {"radius",
	 R"json({"description": "Rounds the corners: one radius for all four, or one per corner (left out: square)",
		"anyOf": [{"$ref": "#/$defs/v2/pixels"}, {"type": "object", "additionalProperties": false,
		"properties": {"topLeft": {"$ref": "#/$defs/v2/pixels"},
		"topRight": {"$ref": "#/$defs/v2/pixels"}, "bottomRight": {"$ref": "#/$defs/v2/pixels"},
		"bottomLeft": {"$ref": "#/$defs/v2/pixels"}}}]})json",
	 "Rounds the corners, in pixels: one number for all four (`8`), or `{ \"topLeft\", \"topRight\", \"bottomRight\", "
	 "\"bottomLeft\" }` (corners left out are square). A radius larger than half the shorter side is clamped to it, so "
	 "`999` makes a pill or a circle. Edges are anti-aliased. Children are still clipped to the square bounds, and the "
	 "corners still take clicks. Default: `0`.",
	 "look"},
    {"borderWidth", R"json({"description": "A border inside the node's edges; it doesn't change the layout",
		"$ref": "#/$defs/v2/pixels"})json",
	 "A border drawn inside the node's edges (it doesn't change the layout), over the background and under the "
	 "children, following `radius`. Default: no border; `borderColor` defaults to black.",
	 "look"},
    {"borderColor", R"json({"description": "Default: black", "$ref": "#/$defs/v2/color"})json", "", "look"},
    {"shadow", R"json({"type": "object", "additionalProperties": false,
		"description": "Like CSS box-shadow: the node's shape, moved and blurred, under it (clipped by the parent)",
		"properties": {"x": {"anyOf": [{"type": "number"}, {"$ref": "#/$defs/v2/dynamic"}]},
		"y": {"anyOf": [{"type": "number"}, {"$ref": "#/$defs/v2/dynamic"}]},
		"blur": {"description": "A Gaussian blur with a standard deviation of blur / 2",
		"$ref": "#/$defs/v2/pixels"}, "color": {"description": "Default: #00000040",
		"$ref": "#/$defs/v2/color"}}})json",
	 "`{ \"x\": 0, \"y\": 4, \"blur\": 12, \"color\": \"#00000040\" }`, like CSS box-shadow: the node's shape, moved "
	 "by `x` and `y` and blurred by `blur` (a Gaussian with a standard deviation of `blur / 2`), drawn under the node. "
	 "Every key is optional; `color` defaults to `#00000040`. A shadow can fall outside the node, but it is clipped by "
	 "the parent, like the node itself.",
	 "look"},
    {"gradient",
	 R"json({"anyOf": [{"type": "array", "minItems": 1, "maxItems": 32, "items": {"$ref": "#/$defs/v2/color"}},
		{"type": "object", "additionalProperties": false, "required": ["stops"],
		"description": "Like CSS linear-gradient() and radial-gradient(), over the node's color",
		"properties": {"kind": {"enum": ["linear", "radial"]},
		"angle": {"description": "Degrees: 0 up, 90 right, 180 down (the default)", "type": "number"},
		"stops": {"type": "array", "minItems": 1, "maxItems": 32, "items": {"anyOf": [{"$ref": "#/$defs/v2/color"},
		{"type": "object", "additionalProperties": false, "required": ["color"],
		"properties": {"at": {"type": "number", "minimum": 0, "maximum": 1},
		"color": {"$ref": "#/$defs/v2/color"}}}]}}}}]})json",
	 "The background as a gradient, painted over `color` within the corners, like CSS `linear-gradient()` and "
	 "`radial-gradient()`: `[\"#1e3a5f\", \"#0b1220\"]` (colours spread evenly from top to bottom), or "
	 "`{ \"kind\": \"linear\", \"angle\": 90, \"stops\": [\"@accent\", { \"at\": 0.6, \"color\": \"#00000000\" }] }`. "
	 "`angle` is in degrees (0 up, 90 right, 180 down, the default); the line reaches the corners, as in CSS. A "
	 "`radial` gradient spreads from the centre to the farthest corner. A stop is a colour, spread evenly, or "
	 "`{ \"at\": 0..1, \"color\" }`; two stops at one place change colour sharply. At most 32 stops. It can be set "
	 "in `hover` and the other states.",
	 "look"},
    {"visible", "flag", "`false` hides the node and its children.", "look"},
    {"cache", "flag",
	 "`true` keeps the node and its children drawn in a texture, drawn as one quad each frame until something in them "
	 "changes. Worth it for a large part of a screen that rarely changes (a sidebar, a chart); it looks the same (a "
	 "pixel of its own translucent edge, such as a rounded corner, may be one level of 255 off). "
	 "Default: `false`.",
	 "look"},
    {"overflow", R"json({"anyOf": [{"enum": ["clip", "visible"]}, {"$ref": "#/$defs/v2/dynamic"}],
		"description": "visible lets the children draw and be hit outside the node"})json",
	 "`\"visible\"` lets the children draw and be hit outside the node, clipped to what clips the node instead, as "
	 "CSS `overflow: visible`: a line whose arrowhead passes its box, text longer than its frame. A cached or "
	 "scrolling node still clips. Default: `\"clip\"`.",
	 "look"},
    {"snapshot", R"json({"anyOf": [{"type": "string"}, {"type": "object", "required": ["image"],
		"additionalProperties": false, "properties": {"image": {"type": "string"},
		"scale": {"type": "number", "exclusiveMinimum": 0, "maximum": 4}}}],
		"description": "Draws the node into a named image, out of the layout, instead of on the screen"})json",
	 "`\"thumb\"` or `{ \"image\": \"thumb\", \"scale\": 0.25 }`: the node and its children are drawn into the image "
	 "of that name instead of on the screen, again each time something in them changes, and an `image` node with "
	 "`\"source\": \"thumb\"` shows it (a thumbnail, a preview, a slide sorter). Out of the layout: laid out at its "
	 "own `width` and `height` from the window's top left, and never hit, focused or read. `scale` is the image's "
	 "pixels per layout pixel (above 0, at most 4; default `1`). Overlays inside it are left out. Not on the root or "
	 "with `overlay`; an empty name draws the node in place.",
	 "look"},
    {"rotate", R"json({"anyOf": [{"type": "number"}, {"$ref": "#/$defs/v2/dynamic"}],
		"description": "Draws the node turned, in degrees clockwise, about transformOrigin; layout doesn't change"})json",
	 "Draws the node and its children turned, in degrees clockwise, about `transformOrigin`: `\"rotate\": 8`. Like "
	 "every transform key it only changes how the node is drawn and hit: its place in the layout, and its siblings', "
	 "stay. Bind it to a number in the state and an `animate` step turns it. Default: `0`.",
	 "look"},
    {"scale", R"json({"anyOf": [{"type": "number"}, {"type": "array", "minItems": 2, "maxItems": 2,
		"items": {"type": "number"}}, {"$ref": "#/$defs/v2/dynamic"}],
		"description": "Draws the node scaled, one factor or [x, y], about transformOrigin"})json",
	 "Draws the node scaled about `transformOrigin`: one factor (`0.9`) or `[x, y]`. Default: `1`.", "look"},
    {"skew", R"json({"anyOf": [{"type": "array", "minItems": 2, "maxItems": 2, "items": {"type": "number"}},
		{"$ref": "#/$defs/v2/dynamic"}], "description": "Draws the node skewed, [x, y] in degrees, as CSS skew()"})json",
	 "Draws the node skewed, `[x, y]` in degrees, as CSS `skew()`: `[-12, 0]` slants it. Default: `[0, 0]`.", "look"},
    {"translate", R"json({"anyOf": [{"type": "array", "minItems": 2, "maxItems": 2, "items": {"type": "number"}},
		{"$ref": "#/$defs/v2/dynamic"}], "description": "Draws the node moved by [x, y] pixels after layout"})json",
	 "Draws the node moved by `[x, y]` pixels after layout. The keys compose as CSS does: `translate`, `rotate`, "
	 "`scale`, then `skew`. Default: `[0, 0]`.",
	 "look"},
    {"transformOrigin", R"json({"anyOf": [{"enum": ["center", "top", "bottom", "left", "right", "topLeft", "topRight",
		"bottomLeft", "bottomRight"]}, {"type": "array", "minItems": 2, "maxItems": 2, "items": {"type": "number"}},
		{"$ref": "#/$defs/v2/dynamic"}],
		"description": "The point rotate, scale and skew turn about: center, an edge or a corner, or [x%, y%]"})json",
	 "The point `rotate`, `scale` and `skew` turn about: `\"center\"` (the default), an edge or a corner "
	 "(`\"top\"`, `\"bottomRight\"`), or `[x%, y%]` of the node's size (`[0, 100]` is the bottom-left corner).",
	 "look"},
    {"cursor", R"json({"anyOf": [{"enum": ["arrow", "ibeam", "hand", "resize-horizontal", "resize-vertical"]},
		{"$ref": "#/$defs/v2/dynamic"}]})json",
	 "`arrow`, `ibeam`, `hand`, `resize-horizontal`, `resize-vertical`.", "look"},
    {"tooltip", R"json({"description": "Text, or a {{binding}}"})json",
	 "Text shown after the pointer rests on the node.", "look"},
    {"disabled", "flag", "`true`: no events (hover styling still applies).", "look"},
    {"scroll", R"json({"anyOf": [{"enum": ["vertical", "horizontal", "both", "none"]}, {"type": "boolean"},
		{"$ref": "#/$defs/v2/dynamic"}]})json",
	 "`\"vertical\"`, `\"horizontal\"`, `\"both\"` or `\"none\"`: the wheel or trackpad scrolls the children; a thin "
	 "bar shows where.",
	 "look"},
    {"overlay", R"json({"type": "object", "additionalProperties": false,
		"description": "Drawn above everything, next to its anchor",
		"properties": {"anchor": {"type": "string"}, "placement": {"enum": ["below", "below-end",
		"above", "above-end", "right", "left", "center", "window"]}, "flip": {"$ref": "#/$defs/v2/flag"},
		"open": {"description": "A state path (\"state.menuOpen\") or a value"},
		"dismiss": {"const": "outside"}, "modal": {"$ref": "#/$defs/v2/flag"}}})json",
	 "`{ \"anchor\": \"#id\", \"placement\": \"below\", \"open\": \"state.menuOpen\", \"dismiss\": \"outside\" }`: "
	 "drawn above everything, unclipped, next to its anchor (default: its parent). Placements: `below`, `below-end`, "
	 "`above`, `above-end`, `right`, `left`, `center` (in the window), `window` (placed by its own x, y, anchors in "
	 "the window). `open` is a state path (or a value); with `\"dismiss\": \"outside\"`, a click outside it or Escape "
	 "sets it to false. `\"modal\": true` traps the focus inside while it is open (Tab cycles in it, the first "
	 "focusable node inside gets the focus unless one inside already has it) and gives it back when it closes. "
	 "`\"flip\": true` places it on the other side when only that side has room, its children mirrored on that axis.",
	 "look"},
    {"focusable", "flag",
	 "`true`: Tab reaches the node, and Enter or Space on it runs its `onClick` (text inputs are always focusable).",
	 "access"},
    {"role", R"json({"description": "A role, or a {{binding}}"})json",
	 "What the node is to assistive tools and to scenarios (ARIA's names: `button`, `checkbox`, `textbox`, `dialog`, "
	 "`list`, `listitem`, `menu`, `menuitem`, `tab`, ...). Default: `textbox` for inputs and editors, `text` for "
	 "labels, `checkbox` for toggles, `slider` for sliders, `img` for images, `button` for a focusable node with an "
	 "`onClick`; other nodes have none and are skipped in the accessibility tree.",
	 "access"},
    {"label", R"json({"description": "Text, or a {{binding}}"})json",
	 "The node's accessible name. Default: its text, else the texts of the labels inside it, else its `tooltip` (an "
	 "input's or a slider's text is its value, not its name).",
	 "access"},
    {"description", R"json({"description": "Text, or a {{binding}}"})json",
	 "A longer accessible description, read after the name.", "access"},
    {"isOpen", "flag",
	 "Whether the node is expanded (a tree item, a disclosure, a menu button: `expanded` to assistive tools); leave it "
	 "out for one that doesn't expand. Screen readers say it, and offer to expand or collapse it (a click).",
	 "access"},
    {"selected", "flag",
	 "Whether the node is selected (a tab, a list item, a menu's check); leave it out, or bind it to `null`, for one "
	 "that can't be.",
	 "access"},
    {"live", R"json({"anyOf": [{"enum": ["off", "polite", "assertive"]}, {"$ref": "#/$defs/v2/dynamic"}]})json",
	 "A live region: changes to its text are read without moving the focus, `polite` when the person is idle (a "
	 "toast), `assertive` at once (an error). Default: `off`.",
	 "access"},
    {"tabIndex", R"json({"anyOf": [{"type": "integer", "minimum": -1}, {"$ref": "#/$defs/v2/dynamic"}]})json",
	 "The tab order: positive ones first, in increasing order, then the others in tree order; `-1` leaves it out.",
	 "access"},
    {"arrows", R"json({"anyOf": [{"enum": ["vertical", "horizontal", "both"]}, {"$ref": "#/$defs/v2/dynamic"}]})json",
	 "The arrow keys, Home and End move the focus between the focusable nodes inside (a list, a menu, a toolbar), "
	 "wrapping; Tab moves past them in one step. A letter key clicks the node inside whose `mnemonic` it is.",
	 "access"},
    {"mnemonic", R"json({"description": "One letter, or a {{binding}}"})json",
	 "A letter: Alt with it clicks the node (the first one, in the open modal overlay if any); inside an `arrows` "
	 "node, the letter alone does.",
	 "access"},
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
	 "behaviour"},
    {"when", R"json({"description": "Build the node only if truthy"})json",
	 "Build the node only if the value is truthy.", "behaviour"},
    {"let", R"json({"type": "object", "description": "Named values for the node and its children, in order"})json",
	 R"(`{ "on": "{{tag == state.filter}}" }`: named values for the node and its children, evaluated in order.)",
	 "behaviour"},
    {"use", R"json({"type": "string", "description": "A component from \"components\""})json",
	 R"(Build a component: `{ "use": "chip", "with": { "label": "{{tag}}" } }` (see Components).)", "behaviour"},
    {"with", R"json({"type": "object", "description": "A component's parameters"})json", "", "behaviour"},
    {"point", R"json({"type": "string", "description": "A contribution point features add views to"})json",
	 R"(`"point": "statusBar.right"`: features' views for that point become the node's last children.)", "behaviour"},
    {"check", R"json({"type": "object", "additionalProperties": false, "description": "Gives valid and error",
		"properties": {"rule": {}, "message": {}, "value": {}}})json",
	 "`{ \"rule\": \"{{length(value) >= 3}}\", \"message\": \"Too short\", \"value\": ... }`: `value` defaults to the "
	 "node's bound value; gives the node and its children `valid` and `error`.",
	 "behaviour"},
    {"every", R"json({"type": "object", "required": ["ms", "do"], "additionalProperties": false,
		"properties": {"ms": {"anyOf": [{"type": "number"}, {"$ref": "#/$defs/v2/dynamic"}]},
		"do": {"$ref": "#/$defs/v2/handler"}}})json",
	 R"(`{ "ms": 1000, "do": steps }`: runs while the node exists.)", "behaviour"},
    {"drag", R"json({"type": "object", "additionalProperties": false, "properties": {"data": {}}})json",
	 "`{ \"data\": {...} }`: the node can be dragged; a node with `onDrop` receives `event.data`.", "behaviour"},
    {"context",
	 R"json({"type": "object", "description": "Keys this node adds to the context variable while it or a child has the focus"})json",
	 "`{ \"editorFocus\": true }`: keys added to the `context` variable while the node or a child has the focus "
	 "(an inner node wins); `when` clauses read them.",
	 "behaviour"},
    {"keys", R"json({"type": "object", "additionalProperties": {"$ref": "#/$defs/v2/keyHandler"},
		"description": "Shortcuts while focused: \"mod+k\" -> handler"})json",
	 "Shortcuts while the node or a child has the focus: `\"enter\" → handler`; `\"*\"` takes any other press (`event.key`).", "behaviour"},
    {"locked", R"json({"type": "boolean"})json",
	 "`true`: no revision may change this node or anything inside it. Needs an `id`.", "behaviour"},
    {"note", R"json({"type": "string", "description": "A comment"})json", "A comment.", "behaviour"},
    {"on…", "", "Event handlers (see Events).", "behaviour"},
    {"text", R"json({"description": "Text, or a {{binding}}"})json", "The text.", "label"},
    {"textColor", "color", "Default black.", "label"},
    {"font", R"json({"type": "string"})json", "Font name. Default: `default`.", "label"},
    {"padding", R"json({"anyOf": [{"type": "number"}, {"$ref": "#/$defs/v2/dynamic"}]})json",
	 "Pixels between the edges and the text.", "label"},
    {"align", R"json({"oneOf": [{"$ref": "#/$defs/v2/align"},
		{"type": "string", "pattern": "^(start|center|end) (start|center|end)$"},
		{"type": "object", "additionalProperties": false,
		"properties": {"horizontal": {"$ref": "#/$defs/v2/align"},
		"vertical": {"$ref": "#/$defs/v2/align"}}}],
		"description": "label: text alignment; stack: v1 name of \"from\""})json",
	 R"(One value for both axes, two for horizontal then vertical (`"start center"`), or `{ "horizontal", "vertical" }`.)",
	 "label"},
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
	 "examples ship Lucide (`examples/shared/fonts/Lucide.ttf`, 1,890 icons: https://lucide.dev/icons). Not on text "
	 "areas.",
	 "label"},
    {"mirror", "flag",
	 "Right to left (`dir`), draw the icon mirrored: `left` and `right` swap in its name (`\"chevron-left\"` draws "
	 "`\"chevron-right\"`), when the font has that one. For arrows and chevrons that point the reading way.",
	 "label"},
    {"runs", R"json({"type": "array", "maxItems": 256, "items": {"oneOf": [{"type": ["string", "number", "boolean"]},
		{"type": "object", "additionalProperties": false, "properties": {"text": {}, "icon": {"type": "string"},
		"font": {"type": "string"}, "size": {"$ref": "#/$defs/v2/pixels"}, "bold": {"$ref": "#/$defs/v2/flag"},
		"italic": {"$ref": "#/$defs/v2/flag"}, "color": {"$ref": "#/$defs/v2/color"}, "link": {"type": "string",
		"maxLength": 2048}}}]},
		"description": "label: rich text, parts with their own font, size, bold, italic, colour, link or icon"})json",
	 "Rich text instead of `text`: a list of parts, each text or `{ \"text\", \"bold\", \"italic\", \"size\", "
	 "\"font\", \"color\", \"link\", \"icon\" }`. `font` names a font of `fonts` (default: the label's); `bold`, "
	 "`italic` and `size` (4 to 256 pixels) make a variant of it (at most 32 per document). A part with `link` is "
	 "underlined and a press on it runs `onLink`; `icon` draws a named glyph inline, as `icon` does. Lines wrap at "
	 "spaces across parts. At most 256 parts and 65,536 characters. Not on text areas.",
	 "label"},
    {"selectable", "flag",
	 "The user can select the text with the mouse and copy it with Cmd/Ctrl+C (Cmd/Ctrl+A selects it all); icons "
	 "are left out of the copy.",
	 "label"},
    {"selectionColor", "color", "The selection's colour (default `#b4d5fe`).", "label"},
});

/// @brief The three parts of the table, joined once in order.
/// @return The keys.
std::vector<KeyReference> joinedKeys() {
	std::vector<KeyReference> keys(commonTable.begin(), commonTable.end());
	for(const std::span<const KeyReference> part : {nodeTypeKeys(), sceneKeys(), nodeEventKeys()})
		keys.insert(keys.end(), part.begin(), part.end());
	return keys;
}

} // namespace

std::span<const KeyReference> nodeKeys() {
	static const std::vector<KeyReference> keys = joinedKeys();
	return keys;
}

} // namespace affresco::format
