/// @file
/// @brief format::nodeTypeKeys(): the reference of the keys the node types add, from the editor's to the timeline's.
///
/// Part of nodeKeys() (ReferenceNodes.cpp), which puts it after the keys every node has.

#include "Reference.hpp"

#include <array>

namespace affresco::format {

namespace {

/// The keys, in the order of the tables.
constexpr auto typeTable = std::to_array<KeyReference>({
    {"bind", R"json({"type": "string",
		"description": "A state path kept in sync both ways (textarea, editor, toggle, checkbox, slider, split, tabs, list, tree)"})json",
	 "A state path holding the text (lines joined by `\"\\n\"`), kept in sync both ways. Without it, `text` sets the "
	 "text.",
	 "editor"},
    {"caret", R"json({"type": "string",
		"description": "editor: a state path the editor writes its caret to, as { line, column, selected } (from 1)"})json",
	 R"(A state path the editor writes its caret to: `{ "line", "column", "selected" }`, counted from 1.)", "editor"},
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
    {"wrap", "", "`true` wraps lines at the width (at spaces, else anywhere); the gutter numbers each line once.",
	 "editor"},
    {"folds", R"json({"type": "array", "maxItems": 100000, "description": "editor: folded line ranges, { line, endLine } from 0",
		"items": {"type": "object", "properties": {"line": {"type": "number", "minimum": 0},
		"endLine": {"type": "number", "minimum": 0}}}})json",
	 R"(Folded ranges, `[{ "line", "endLine" }]` from 0: the lines after `line` up to `endLine` are hidden behind it. )"
	 "The user folds and unfolds with the gutter or Cmd+Alt+[ and ]; the document's folds replace theirs only when "
	 "they change, and leaving the key out keeps them. At most 10,000.",
	 "editor"},
    {"decorations",
	 R"json({"type": "array", "maxItems": 100000, "description": "editor: backgrounds, squiggles, gutter icons and inline text on lines (from 0)",
		"items": {"type": "object", "properties": {"line": {"type": "number", "minimum": 0},
		"endLine": {"type": "number", "minimum": 0}, "column": {"type": "number", "minimum": 0},
		"endColumn": {"type": "number", "minimum": 0},
		"kind": {"enum": ["background", "squiggle", "gutter", "inline"]}, "color": {"$ref": "#/$defs/v2/color"},
		"icon": {"type": "string"}, "text": {"type": "string"}}}})json",
	 R"(`[{ "line", "endLine", "column", "endColumn", "kind", "color", "icon", "text" }]`, lines and columns from 0: )"
	 "`background` tints the lines, `squiggle` underlines the columns (or the whole lines) with a wave, `gutter` "
	 "draws `icon` (its first character) beside the line, `inline` writes `text` after the line's end. At most "
	 "10,000 and 8 on a line; inline text is cut at 256 characters.",
	 "editor"},
    {"find", R"json({"type": ["object", "null"], "description": "editor: highlights the matches of a pattern; Cmd+G and Cmd+Shift+G go to the next and previous",
		"properties": {"text": {"type": "string"}, "regex": {"type": "boolean"}, "wholeWord": {"type": "boolean"},
		"caseSensitive": {"type": "boolean"}, "inSelection": {"type": "boolean"}, "replace": {"type": "string"},
		"step": {"enum": ["first", "next", "previous", "replace", "replaceAll"]}, "request": {"type": "number"}}})json",
	 R"(`{ "text", "regex", "wholeWord", "caseSensitive" }` or `null` (none): highlights the matches of `text` (plain, or a PCRE2 )"
	 "pattern with `regex`, matched line by line within match and depth limits); Cmd+G and Cmd+Shift+G select "
	 "the next and previous. A pattern that doesn't compile highlights nothing. `inSelection` limits it to the "
	 "selection as it is when turned on. A find bar asks for a step with `step` (`first` from the selection's start, "
	 "`next`, `previous`, `replace` or `replaceAll`, with `replace` as the replacement) and a `request` number from "
	 "1, run once per new number. While a pattern is set, `caret` also gets `matches`, the count.",
	 "editor"},
    {"brackets", "flag", "`true` highlights the bracket that pairs with the one at the caret.", "editor"},
    {"indentGuides", "flag", "`true` draws a line at each indentation level.", "editor"},
    {"minimap", "flag",
	 "`true` shows a minimap at the right: an overview of the rows, with the visible ones marked; pressing or "
	 "dragging it scrolls.",
	 "editor"},
    {"composition", R"json({"description": "Text, or a {{binding}}"})json",
	 "Text an input method is composing, shown underlined at the caret without editing (empty: none).", "editor"},
    {"readOnly", "flag", "`true`: the user can select, find and fold, but not edit.", "editor"},
    {"guideColor", "color", "The indentation guides, find's matches and the paired brackets.", "editor"},
    {"matchColor", "color", "", "editor"},
    {"bracketColor", "color", "", "editor"},
    {"grammar", R"json({"type": "string", "description": "editor: the language that colours, folds and pairs brackets (e.g. cpp, or a file name)"})json",
	 "The language parsed for syntax colours, folds and bracket pairs: `c`, `cpp`, `json`, `python`, `javascript`, "
	 "`typescript`, `markdown` or an alias (`c++`, `js`, `ts`, `py`, `md`), or a file name whose extension says it. "
	 "Each capture is coloured by the theme's `@syntax.<capture>` token (e.g. `@syntax.keyword`; `keyword.return` "
	 "falls back to `@syntax.keyword`). Left out, plain text.",
	 "editor"},
    {"original", R"json({"type": "string", "description": "diff: the text compared from"})json",
	 "The text compared from (lines split at `\\n`, a `\\r` before it dropped).", "diff"},
    {"modified", R"json({"type": "string", "description": "diff: the text compared to"})json", "The text compared to.",
	 "diff"},
    {"mode", R"json({"anyOf": [{"enum": ["split", "inline", "square", "wheel"]}, {"$ref": "#/$defs/v2/dynamic"}],
		"description": "diff: split (default) or inline; colorPicker: square (default) or wheel"})json",
	 "`split` (default): the original on the left, the modified text on the right, rows aligned with blank rows and "
	 "scrolled together; `inline`: one editor, the deleted lines above the lines inserted for them.",
	 "diff"},
    {"language", R"json({"type": "string", "description": "diff: a language id for the application's editors (e.g. cpp)"})json",
	 "A language id given to the application's editor factory (syntax colours); left out, or without one, plain text.",
	 "diff"},
    {"deletedColor", "color",
	 "Behind deleted and inserted lines (the changed characters in the same colour, twice as strong).", "diff"},
    {"insertedColor", "color", "", "diff"},
    {"gap", R"json({"anyOf": [{"type": "number"}, {"$ref": "#/$defs/v2/dynamic"}]})json",
	 "Pixels between children (and between lines, when it wraps).", "stack"},
    {"rowGap",
	 R"json({"description": "Pixels between rows (a column's children, a wrapping row's lines, a grid's rows); default: gap",
		"anyOf": [{"type": "number", "minimum": 0}, {"$ref": "#/$defs/v2/dynamic"}]})json",
	 "Pixels between rows and between columns, over `gap`: in a column `rowGap` is between children, in a wrapping row "
	 "between lines.",
	 "stack"},
    {"columnGap",
	 R"json({"description": "Pixels between columns (a row's children, a wrapping column's lines, a grid's columns); default: gap",
		"anyOf": [{"type": "number", "minimum": 0}, {"$ref": "#/$defs/v2/dynamic"}]})json",
	 "", "stack"},
    {"justify",
	 R"json({"description": "row, column, stack: how the children are spread along it",
		"anyOf": [{"enum": ["start", "center", "end", "space-between", "space-around",
		"space-evenly"]}, {"$ref": "#/$defs/v2/dynamic"}]})json",
	 "How the children are spread along it: `start` (default), `center`, `end` (a column from the bottom, like a chat: "
	 "overflow is clipped at the top), `space-between`, `space-around` or `space-evenly`, as in CSS.",
	 "stack"},
    {"alignItems",
	 R"json({"description": "row, column, stack: where children sit across (left out: their own anchors); grid: down their cells (default start)",
		"$ref": "#/$defs/v2/crossAlign"})json",
	 "Where the children sit across: `start`, `center`, `end` or `stretch` (as tall as a row, as wide as a column, "
	 "within their maximum). Left out, they keep their own anchors across.",
	 "stack"},
    {"direction", R"json({"anyOf": [{"enum": ["row", "column"]}, {"$ref": "#/$defs/v2/dynamic"}]})json",
	 R"(On a `stack`: `"row"` or `"column"`.)", "stack"},
    {"wrap", "",
	 "Continue on a new line (in a column, a new column) when a child doesn't fit; each line grows, shrinks and is "
	 "spread on its own, and sits at the start of its line unless aligned.",
	 "stack"},
    {"columns",
	 R"json({"description": "grid: a number of equal columns, or a list of tracks, e.g. [200, \"1fr\", \"minmax(100, 2fr)\"]; table: the columns: an id (also the title), or { id, title, width }",
		"anyOf": [{"type": "integer", "minimum": 1, "maximum": 1000}, {"type": "array", "minItems": 1,
		"maxItems": 1000, "items": {"$ref": "#/$defs/v2/track"}}, {"type": "array", "maxItems": 256,
		"items": {"anyOf": [{"type": "string"}, {"type": "object", "required": ["id"],
		"properties": {"id": {"type": ["string", "number"]}, "title": {"type": ["string", "number"]},
		"width": {"type": "number", "minimum": 24, "maximum": 10000}}}]}}, {"$ref": "#/$defs/v2/dynamic"}]})json",
	 "A number of equal columns (default `2`), or a list of tracks: pixels (`200`), a percentage of the grid "
	 "(`\"25%\"`), a share of the room left (`\"1fr\"`), `\"auto\"` (as wide as its widest child) or `\"minmax(100, "
	 "1fr)\"`. `\"1fr\"` is `minmax(0, 1fr)`, so equal fractions stay equal whatever their content; write "
	 "`\"minmax(auto, 1fr)\"` to keep a track as wide as its content. At most 1,000. On a `table`: the columns, left "
	 "to right: a string (its id and title), or `{ \"id\", \"title\", \"width\" }` (pixels, default 120, at least "
	 "24). Column i shows each item's cell i. Dragging a header's right edge resizes it and runs `onColumnResize`; "
	 "keep the width in state there to keep it over a change of columns.",
	 "grid"},
    {"rows", R"json({"description": "grid: the rows' tracks; rows past them are as tall as their tallest child",
		"type": "array", "maxItems": 1000, "items": {"$ref": "#/$defs/v2/track"}})json",
	 "Tracks for the rows, as `columns`. Rows past them, and every row without `rows`, are as tall as their tallest "
	 "child.",
	 "grid"},
    {"gap", "", "Pixels between cells; `rowGap` and `columnGap` over `gap`.", "grid"},
    {"rowGap", "", "", "grid"},
    {"columnGap", "", "", "grid"},
    {"justifyItems", R"json({"description": "grid: where children sit across their cells (default stretch)",
		"$ref": "#/$defs/v2/crossAlign"})json",
	 "Where children sit across their cells: `stretch` (default), `start`, `center` or `end`.", "grid"},
    {"alignItems", "", "Where children sit down their cells: `start` (default), `center`, `end` or `stretch`.", "grid"},
    {"trackColor", "color", "The empty or off part.", "control"},
    {"activeColor", "color", "The filled or on part.", "control"},
    {"knobColor", "color", "The knob (or the inside of the checkbox).", "control"},
    {"min", R"json({"anyOf": [{"type": "number"}, {"$ref": "#/$defs/v2/dynamic"}]})json",
	 "Slider range (default 0 to 100) and snapping.", "control"},
    {"max", R"json({"anyOf": [{"type": "number"}, {"$ref": "#/$defs/v2/dynamic"}]})json", "", "control"},
    {"step", R"json({"anyOf": [{"type": "number"}, {"$ref": "#/$defs/v2/dynamic"}]})json", "", "control"},
    {"file", R"json({"type": "string",
		"description": "image: a PNG, JPEG or SVG file, relative to the screen file and inside its folder (or one the host allows)"})json",
	 "A PNG, JPEG or SVG file, relative to the screen file. It must be inside the screen file's folder (or the folders "
	 "inside it), or a folder the host allows (`Screen::allowImageDirectory`): a revision naming any other file is "
	 "refused. At most 64 MiB, 8,192 pixels a side and 4,096 × 4,096 pixels in all. A file that can't be read or "
	 "decoded shows nothing (and is logged).",
	 "image"},
    {"source", R"json({"type": "string",
		"description": "image: an image the host names (Screen::setImage), instead of a file"})json",
	 "Instead of `file`: an image the host names (`Screen::setImage(\"chart\", …)`), e.g. a picture it made; the node "
	 "shows each new one the host sets.",
	 "image"},
    {"fit", R"json({"anyOf": [{"enum": ["contain", "cover", "fill"]}, {"$ref": "#/$defs/v2/dynamic"}],
		"description": "image and path: contain (default), cover or fill, like CSS object-fit"})json",
	 "`\"contain\"` (the default: all of it, as large as fits, centred), `\"cover\"` (the whole node, the image's "
	 "edges cut off) or `\"fill\"` (stretched), like CSS `object-fit`.",
	 "image"},
    {"surface", R"json({"anyOf": [{"type": "string"}, {"$ref": "#/$defs/v2/dynamic"}],
		"description": "surface: the name of a GPU surface the host registered (Screen::registerSurface)"})json",
	 "The name of a GPU surface the host registered (`Screen::registerSurface(\"cube\", …)`). A document can only "
	 "name one: it never supplies shader source. A name that isn't registered is an error at the node.",
	 "surface"},
    {"params", R"json({"anyOf": [{"type": "object"}, {"$ref": "#/$defs/v2/dynamic"}],
		"description": "surface: what the surface draws, an object it reads (e.g. a colour, a speed)"})json",
	 "An object the surface reads each time it draws, e.g. `{ \"speed\": 2 }`: bind its values to the state and it "
	 "draws again when they change.",
	 "surface"},
    {"continuous", "flag",
	 "`true` draws it every frame (an animation), with the time since the screen started. Default: `false`, drawn "
	 "only when `params`, its size or its input change.",
	 "surface"},
    {"direction", "",
	 R"(`"row"` (the default: panes side by side, vertical sashes) or `"column"` (one above the other).)", "split"},
    {"value",
	 R"json({"description": "slider: a number; split: the panes' sizes; tabs: the active tab's id; list, tree: the selected id or ids; colorPicker: a colour; gradient: { kind, angle, stops }; curves: { points }",
		"anyOf": [{"type": "number"}, {"type": "string"}, {"type": "array", "maxItems": 100000},
		{"type": "object"}, {"$ref": "#/$defs/v2/dynamic"}]})json",
	 "The panes' sizes in pixels, in order (`bind` keeps them in a state path). The last pane takes what is left; "
	 "missing sizes share it equally. Moving a sash runs `onChange` with the new sizes.",
	 "split"},
    {"defaultSizes", R"json({"type": "array", "maxItems": 1000, "items": {"type": "number", "minimum": 0},
		"description": "split: the sizes a double click on a sash (or Home) goes back to"})json",
	 "The sizes a double click on a sash (or Home on the focused split) goes back to; left out, equal panes.", "split"},
    {"minSize", R"json({"anyOf": [{"type": "number", "minimum": 0}, {"type": "array", "maxItems": 1000,
		"items": {"type": "number", "minimum": 0}}, {"$ref": "#/$defs/v2/dynamic"}],
		"description": "split: the smallest size of every pane, or of each in order"})json",
	 "The smallest size of every pane (a number), or of each in order (a list; its last one is used for the rest).",
	 "split"},
    {"sashSize",
	 R"json({"anyOf": [{"type": "number", "minimum": 0, "maximum": 64}, {"$ref": "#/$defs/v2/dynamic"}]})json",
	 "Pixels between two panes (default 4); the pointer finds a thin sash a little either side.", "split"},
    {"sashColor", "color", "The sash (default transparent) and the sash being dragged or chosen by keyboard.", "split"},
    {"sashActiveColor", "color", "", "split"},
    {"tabs", R"json({"anyOf": [{"type": "array", "maxItems": 10000, "items": {"anyOf": [{"type": "string"},
		{"type": "object", "required": ["id"], "properties": {"id": {"type": "string"}, "label": {"type": "string"},
		"marker": {"type": "boolean"}, "closable": {"type": "boolean"}}}]}}, {"$ref": "#/$defs/v2/dynamic"}],
		"description": "tabs: the tabs, in order: an id, or { id, label, marker, closable }"})json",
	 "The tabs, in order (or a binding such as `\"{{state.open}}\"`): a string (its id and label), or `{ \"id\", "
	 "\"label\", \"marker\", \"closable\" }`. "
	 "`marker` shows a dot (e.g. unsaved changes) where the close button goes; `closable` defaults to true. At most "
	 "10,000; the strip scrolls sideways when they don't fit.",
	 "tabs"},
    {"value", "", "The active tab's id (`bind` keeps it in a state path); picking a tab runs `onChange`.", "tabs"},
    {"textColor", "", "The labels' colour (default muted), the active tab's, its background and its line.", "tabs"},
    {"activeTextColor", "color", "", "tabs"},
    {"activeBackground", "color", "", "tabs"},
    {"accentColor", "color", "", "tabs"},
    {"accentEdge", R"json({"anyOf": [{"enum": ["top", "bottom"]}, {"$ref": "#/$defs/v2/dynamic"}],
		"description": "tabs: the edge of the active tab its line is on: bottom (default) or top"})json",
	 "The edge of the active tab its accent line is on: `bottom` (default) or `top`.", "tabs"},
    {"items", R"json({"anyOf": [{"type": "array", "maxItems": 1000000, "items": {"anyOf": [{"type": "string"},
		{"type": "number"}, {"type": "object", "required": ["id"], "properties": {"id": {"type": ["string", "number"]},
		"label": {"type": ["string", "number"]}, "expandable": {"type": "boolean"}, "children": {"type": "array"},
		"icon": {"type": "string"}, "iconColor": {"type": ["string", "null"]},
		"cells": {"type": "array", "minItems": 1, "maxItems": 256,
		"items": {"type": ["string", "number", "boolean", "null"]}}}}]}},
		{"$ref": "#/$defs/v2/dynamic"}],
		"description": "list, tree, table: the rows: an id, or { id, label, icon, iconColor, children (tree), expandable (tree), cells (table) }"})json",
	 "The rows, in order (or a binding such as `\"{{state.files}}\"`): a string (its id and label), or `{ \"id\", "
	 "\"label\", \"icon\", \"iconColor\" }` (an icon before the label, by name, as a label's `icon`, in a hex colour "
	 "or "
	 "the text's); in a `tree`, also `\"children\"` (a list of items) and `\"expandable\"` (a closed node whose "
	 "children aren't loaded yet: `onExpand` can load them); in a `table`, `\"cells\"`: the texts of the row's "
	 "columns, in order (at most 256). Only the rows in view are drawn, so a list scrolls through a million items as "
	 "fast as through ten. At most 1,000,000 items, nested at most 256 deep.",
	 "list"},
    {"expanded", R"json({"anyOf": [{"type": "array", "maxItems": 1000000, "items": {"type": ["string", "number"]}},
		{"$ref": "#/$defs/v2/dynamic"}], "description": "tree: the ids of the open nodes"})json",
	 "On a `tree`: the ids of the open nodes. Opening or closing one runs `onExpand`; keep the ids in state there "
	 "to keep them over a rebuild.",
	 "list"},
    {"stickyRows", R"json({"anyOf": [{"type": "number", "minimum": 0, "maximum": 16}, {"$ref": "#/$defs/v2/dynamic"}],
		"description": "tree: how many open folders stay pinned at the top while their rows scroll (default 0)"})json",
	 "On a `tree`: how many open folders stay pinned at the top while their children scroll by, nearest the root "
	 "first (default 0, at most 16). A click on a pinned folder moves the cursor to it.",
	 "list"},
    {"sort", R"json({"anyOf": [{"type": "null"}, {"type": "object", "required": ["column"],
		"properties": {"column": {"type": ["string", "number"]}, "descending": {"type": "boolean"}}},
		{"$ref": "#/$defs/v2/dynamic"}], "description": "table: the order: { column, descending }, or null"})json",
	 "On a `table`: the rows' order, `{ \"column\": id, \"descending\": false }` (or null for the items' order). A "
	 "click on a header sorts by its column, ascending then descending, and runs `onSort`: numbers by value before "
	 "text, equal cells in the items' order.",
	 "list"},
    {"selection", R"json({"anyOf": [{"enum": ["none", "single", "multiple"]}, {"$ref": "#/$defs/v2/dynamic"}],
		"description": "list, tree: none, single (default) or multiple"})json",
	 R"(`"single"` (the default), `"multiple"` (Shift adds a range, Cmd or Ctrl one row) or `"none"`.)", "list"},
    {"value", "",
	 "The selected id, or with `\"multiple\"` the list of them (`bind` keeps it in a state path); a change runs "
	 "`onChange`.",
	 "list"},
    {"rowHeight", R"json({"anyOf": [{"type": "number", "minimum": 1, "maximum": 1000}, {"$ref": "#/$defs/v2/dynamic"}],
		"description": "list, tree: each row's height in pixels (default 24)"})json",
	 "Each row's height in pixels (default 24).", "list"},
    {"font", "",
	 "The rows' font and colours: their text, the selected rows' text and background, and the accent "
	 "(the keyboard cursor, the drop line, a tree's guides).",
	 "list"},
    {"textColor", "", "", "list"},
    {"selectedTextColor", "color", "", "list"},
    {"selectedBackground", "color", "", "list"},
    {"accentColor", "", "", "list"},
    {"d", R"json({"anyOf": [{"type": "string"}, {"$ref": "#/$defs/v2/dynamic"}],
		"description": "path: SVG path data (M L H V C S Q T A Z, absolute and relative)"})json",
	 "SVG path data: `M L H V C S Q T A Z`, upper case absolute and lower case relative, e.g. `\"M 0 0 L 10 0 L 5 8 "
	 "Z\"`. At most 64 KiB and 16,384 commands, coordinates within a million: a revision with data that can't be read "
	 "is refused, naming the place of the problem.",
	 "path"},
    {"viewBox", R"json({"anyOf": [{"type": "string"}, {"$ref": "#/$defs/v2/dynamic"}],
		"description": "path: the area of the data fitted to the node, \"x y width height\""})json",
	 "The area of the data's coordinates fitted to the node, `\"x y width height\"` like SVG's. Without it the data is "
	 "in the node's own pixels, from its top-left corner.",
	 "path"},
    {"fit", "", R"(How the view box fills the node: `"contain"` (the default), `"cover"` or `"fill"`, as for images.)",
	 "path"},
    {"fillColor", "color", "The inside's colour; without it the path isn't filled.", "path"},
    {"fillRule", R"json({"anyOf": [{"enum": ["nonzero", "evenodd"]}, {"$ref": "#/$defs/v2/dynamic"}],
		"description": "path: which points are inside, nonzero (default) or evenodd"})json",
	 R"(Which points are inside: `"nonzero"` (the default) or `"evenodd"` (a shape inside another is a hole).)",
	 "path"},
    {"stroke", "color", "The outline's colour; without it there is no outline.", "path"},
    {"strokeWidth", R"json({"anyOf": [{"type": "number", "minimum": 0}, {"$ref": "#/$defs/v2/dynamic"}]})json",
	 "The outline's width (default 1), in the view box's units (pixels when stretched by `\"fill\"`).", "path"},
    {"strokeJoin", R"json({"anyOf": [{"enum": ["miter", "round", "bevel"]}, {"$ref": "#/$defs/v2/dynamic"}]})json",
	 R"(Corners: `"miter"` (the default, bevelled past 4 widths), `"round"` or `"bevel"`.)", "path"},
    {"strokeCap", R"json({"anyOf": [{"enum": ["butt", "round", "square"]}, {"$ref": "#/$defs/v2/dynamic"}]})json",
	 R"(Ends: `"butt"` (the default), `"round"` or `"square"`.)", "path"},
    {"strokeDash", R"json({"anyOf": [{"type": "array", "maxItems": 64, "items": {"type": "number", "minimum": 0}},
		{"$ref": "#/$defs/v2/dynamic"}]})json",
	 "Dash and gap lengths, repeated, e.g. `[6, 3]` (at most 64); without it the outline is solid.", "path"},
    {"value", "",
	 "The colour, `\"#rrggbbaa\"` (`bind` keeps it in a state path); picking one runs `onChange` with it.",
	 "colorPicker"},
    {"mode", "",
	 R"(`"square"` (the default: saturation across, value down, a hue strip) or `"wheel"` (hue around, saturation )"
	 R"(out, a value strip).)",
	 "colorPicker"},
    {"alpha", "flag", "Whether the alpha strip shows (default true); hidden, the colour is opaque.", "colorPicker"},
    {"fields", "flag", "Whether the typed fields show (default true): hex, RGB, HSL, HSV and OKLCH.", "colorPicker"},
    {"swatches", R"json({"anyOf": [{"type": "array", "maxItems": 32, "items": {"type": "string"}},
		{"$ref": "#/$defs/v2/dynamic"}], "description": "colorPicker: colours or @tokens to pick from"})json",
	 "Colours to pick from: hex colours or design tokens (`\"@accent\"`), at most 32. The last 8 colours picked follow "
	 "them.",
	 "colorPicker"},
    {"textColor", "", "The fields' text and the accent of the part the keyboard is on.", "colorPicker"},
    {"accentColor", "", "", "colorPicker"},
    {"value", "",
	 "`{ \"kind\": \"linear\" or \"radial\", \"angle\": 90, \"stops\": [{ \"at\": 0, \"color\": \"#000000ff\" }] }`: "
	 "the angle in degrees (0 up, 90 right, as in CSS), the stops' positions 0 to 1, at most 32 stops. `bind` keeps "
	 "it in a state path; a change runs `onChange` with it. `gradientColor(value, at)` gives its colour at a position.",
	 "gradient"},
    {"accentColor", "", "The selected stop's outline, and the accent of the colour popover.", "gradient"},
    {"value", "",
	 "`{ \"points\": [[0, 0], [1, 1]] }`: the control points, 0 to 1, at most 32; with `channels`, also `\"red\"`, "
	 "`\"green\"` and `\"blue\"` lists. `bind` keeps it in a state path; a change runs `onChange` with it. "
	 "`curve(value, x)` gives the curve's output at `x`.",
	 "curves"},
    {"interpolation", R"json({"anyOf": [{"enum": ["monotone", "linear", "bezier"]}, {"$ref": "#/$defs/v2/dynamic"}],
		"description": "curves: monotone (default), linear or bezier"})json",
	 R"(`"monotone"` (the default: smooth through the points, never overshooting), `"linear"` or `"bezier"` (the )"
	 R"(points as a Bézier curve's control polygon).)",
	 "curves"},
    {"channels", "flag", "Whether the RGB, R, G and B tabs show (default false: the RGB curve only).", "curves"},
    {"histogram", R"json({"anyOf": [{"type": "array", "maxItems": 1024, "items": {"type": "number"}},
		{"type": "null"}, {"$ref": "#/$defs/v2/dynamic"}], "description": "curves: counts drawn behind the curve"})json",
	 "Counts from dark to light drawn behind the curve, e.g. `\"{{state.histogram}}\"`; at most 1,024.", "curves"},
    {"accentColor", "", "The selected point and tab.", "curves"},
    {"view", R"json({"anyOf": [{"type": "object", "properties": {"camera": {"type": "object", "properties": {
		"x": {"type": "number"}, "y": {"type": "number"}, "zoom": {"type": "number", "minimum": 0.01, "maximum": 256}}},
		"selection": {"type": "array", "maxItems": 10000, "items": {"type": ["string", "number"]}},
		"guides": {"type": "object", "properties": {"vertical": {"type": "array", "maxItems": 256, "items": {"type": "number"}},
		"horizontal": {"type": "array", "maxItems": 256, "items": {"type": "number"}}}}}}, {"$ref": "#/$defs/v2/dynamic"}],
		"description": "canvas: the camera { x, y, zoom }, the selected ids and the guides { vertical, horizontal }"})json",
	 "The view (`bind` keeps it in a state path, both ways): `{ \"camera\": { \"x\", \"y\", \"zoom\" }, \"selection\": "
	 "[ids], \"guides\": { \"vertical\": [x], \"horizontal\": [y] } }`, each part only if given. The camera's x and y "
	 "are the world point at the top-left; the zoom (screen units per world unit) stays within 0.01 and 256. Panning, "
	 "zooming, selecting and dragging guides out of the rulers run `onChange`; `animate` on `state.<bind>.camera.zoom` "
	 "(or x, y) moves the camera smoothly.",
	 "canvas"},
    {"items", "",
	 "Items the canvas draws itself, in world units, in drawing order (at most 10,000; an application gives more "
	 "through a model): `{ \"id\", \"x\", \"y\", \"width\", \"height\", \"rotate\", \"color\", \"borderColor\", "
	 "\"borderWidth\", \"radius\", \"text\", \"textSize\", \"textColor\" }`. Only those in view are drawn; text too "
	 "small to read draws as a bar. Children are placed in world units too, by their `x`, `y`, `width` and `height`.",
	 "canvas"},
    {"lod", R"json({"anyOf": [{"type": "array", "maxItems": 256, "items": {"type": "object",
		"required": ["for", "use"], "properties": {"for": {"type": "string"}, "use": {"type": "string"},
		"below": {"type": "number", "minimum": 0, "maximum": 256}}}}, {"$ref": "#/$defs/v2/dynamic"}],
		"description": "canvas: levels of detail: below a zoom, the child use stands in for the child for"})json",
	 "Levels of detail: `[{ \"for\": \"card\", \"below\": 0.25, \"use\": \"cardSmall\" }]`: below that zoom the child "
	 "`use` (a lighter sibling, by id) is drawn in place of the child `for`.",
	 "canvas"},
    {"grid",
	 R"json({"anyOf": [{"type": "number", "minimum": 0, "maximum": 1000000}, {"$ref": "#/$defs/v2/dynamic"}]})json",
	 "World units between grid lines (default 0: none); drawn when the lines are far enough apart, and snapped to.",
	 "canvas"},
    {"snap", "flag", "Moves and resizes snap to the grid, other items' edges and centres, and guides (default true).",
	 "canvas"},
    {"rulers", "flag",
	 "Rulers along the top and left, in world units; dragging out of one makes a guide (default true).", "canvas"},
    {"font", "", "The items' text font (default `default`), in a few sizes.", "canvas"},
    {"accentColor", "", "The selection, the marquee, the handles and the guides.", "canvas"},
    {"nodes", R"json({"anyOf": [{"type": "array", "maxItems": 10000, "items": {"type": "object", "required": ["id"]}},
		{"$ref": "#/$defs/v2/dynamic"}], "description": "graph: the nodes, { id, title, x, y, width, bodyHeight, inputs, outputs }"})json",
	 "The nodes, usually from state: `{ id, title, x, y, width, bodyHeight, inputs, outputs }` (at most 10,000, ids "
	 "unique). A port is an id, or `{ id, label, kind, links }`: links join ports of the same `kind` (or either "
	 "`\"any\"`, the default), and `links` is how many it takes (0, the default, for any number); at most 64 inputs "
	 "and 64 outputs. The node's children, made with `repeat` like the nodes, are their bodies: child i sits under "
	 "node i's ports, `bodyHeight` tall (0, the default, for none), hidden when the node is off screen or the zoom "
	 "below 0.5.",
	 "graph"},
    {"links", R"json({"anyOf": [{"type": "array", "maxItems": 50000, "items": {"type": "object",
		"required": ["from", "output", "to", "input"]}}, {"$ref": "#/$defs/v2/dynamic"}],
		"description": "graph: the links, { from, output, to, input }"})json",
	 "The links, `{ from, output, to, input }`: a node's output to a node's input (at most 50,000). The graph changes "
	 "nothing itself: its handlers change the state.",
	 "graph"},
    {"cycles", "flag", "Whether a link may close a cycle (a node linked to itself included); `false`, the default, "
	 "refuses it.",
	 "graph"},
    {"kindColors", R"json({"anyOf": [{"type": "object", "maxProperties": 64, "additionalProperties": {"$ref": "#/$defs/v2/color"}},
		{"$ref": "#/$defs/v2/dynamic"}], "description": "graph: each port kind's colour"})json",
	 R"(Each kind's colour, `{ "float": "#a3e635" }` (at most 64): its ports and the links from them.)", "graph"},
    {"nodeColor", "color", "The nodes' box and header; their text and the selection's colours are `textColor` and "
	 "`accentColor`.",
	 "graph"},
    {"headerColor", "color", "", "graph"},
    {"minimap", "flag", "Whether the minimap shows, at the bottom right (the default): a press or a drag there moves "
	 "the view.",
	 "graph"},
    {"zoom", R"json({"anyOf": [{"type": "number", "minimum": 0.1, "maximum": 4}, {"$ref": "#/$defs/v2/dynamic"}],
		"description": "graph: window units per graph unit, 0.1 to 4 (default 1)"})json",
	 "The camera: the zoom (0.1 to 4, the default 1) and where the graph's origin is in the view, `[x, y]`. They are "
	 "applied when they change, so the user's own zoom (+ and -) and pan (the wheel, or a drag with the right or "
	 "middle button) stay until then.",
	 "graph"},
    {"pan", R"json({"anyOf": [{"type": "array", "minItems": 2, "maxItems": 2, "items": {"type": "number"}},
		{"$ref": "#/$defs/v2/dynamic"}]})json",
	 "", "graph"},
    {"selectedIds", R"json({"description": "graph: the selected node's id, or a list of ids"})json",
	 "The selected nodes' ids (applied when it changes); a click, Shift and a click, or a marquee changes the "
	 "selection and runs `onSelect`.",
	 "graph"},
    {"tracks", R"json({"anyOf": [{"type": "array", "maxItems": 10000, "items": {"type": "object", "required": ["id"]}},
		{"$ref": "#/$defs/v2/dynamic"}], "description": "timeline: the tracks, { id, label, keyframes, clips, tracks, expanded, muted, locked }"})json",
	 "The tracks, usually from state: `{ id, label, keyframes, clips, tracks, expanded, muted, locked }` (at most "
	 "10,000 in all, ids unique). A track with `tracks` is a group (at most 16 deep) that its chevron or Enter opens "
	 "and closes (`onExpand`); its M and L buttons, or the keys, mute and lock it (`onChange`, `event.type` "
	 "`\"track\"`): a locked track's items don't move. A keyframe is `{ time, value, easing }`, `easing` being "
	 "`\"step\"` (drawn as a square), `\"linear\"` (a diamond, the default) or `\"bezier\"` (a circle; CSS's "
	 "ease-in-out, or `[x1, y1, x2, y2]` for others). A clip is `{ id, label, start, duration, color }` (ids unique "
	 "across tracks). At most 10,000 keyframes and 10,000 clips a track, 100,000 in all. The node's children, made "
	 "with `repeat`, are the headers: child i is the i-th track (a group's tracks following it), placed in its row and "
	 "hidden while the row is scrolled away or closed.",
	 "timeline"},
    {"duration", R"json({"anyOf": [{"type": "number", "minimum": 0, "maximum": 1000000}, {"$ref": "#/$defs/v2/dynamic"}],
		"description": "timeline: its length in seconds"})json",
	 "Its length, in seconds (default 10), and its frames per second (`fps`, 1 to 1000, default 30): times snap to "
	 "frames.",
	 "timeline"},
    {"fps", R"json({"anyOf": [{"type": "number", "minimum": 1, "maximum": 1000}, {"$ref": "#/$defs/v2/dynamic"}]})json",
	 "", "timeline"},
    {"time", R"json({"anyOf": [{"type": "number", "minimum": 0}, {"$ref": "#/$defs/v2/dynamic"}],
		"description": "timeline: the playhead, in seconds"})json",
	 "The playhead, in seconds, usually `{{state.time}}` (applied when it changes). A press or a drag on the ruler, "
	 "Left and Right (a frame; a second with Shift), Comma and Period (the previous or next keyframe, clip edge or "
	 "marker), Home and End (the play range's ends) move it and run `onScrub`.",
	 "timeline"},
    {"timeFormat", R"json({"anyOf": [{"enum": ["frames", "seconds", "timecode"]}, {"$ref": "#/$defs/v2/dynamic"}]})json",
	 "The ruler's labels: `\"frames\"` (the default), `\"seconds\"` or `\"timecode\"` (`HH:MM:SS:FF`). Its ticks "
	 "follow the zoom.",
	 "timeline"},
    {"playRange", R"json({"anyOf": [{"type": "array", "minItems": 2, "maxItems": 2, "items": {"type": "number", "minimum": 0}},
		{"$ref": "#/$defs/v2/dynamic"}]})json",
	 "`[start, end]`, shaded on the ruler, the rest of the lanes dimmed (without it, the whole duration).",
	 "timeline"},
    {"markers", R"json({"anyOf": [{"type": "array", "maxItems": 1000}, {"$ref": "#/$defs/v2/dynamic"}]})json",
	 "Times, or `{ time, label }`, marked on the ruler and across the lanes (at most 1,000).", "timeline"},
    {"snap", "flag",
	 "Whether drags snap (the default): to frames, and within a few pixels to keyframes, clip edges, markers, the "
	 "play range and the playhead. Alt held at the press leaves it off for that drag.",
	 "timeline"},
    {"pixelsPerSecond", R"json({"anyOf": [{"type": "number", "minimum": 1, "maximum": 10000}, {"$ref": "#/$defs/v2/dynamic"}],
		"description": "timeline: window units per second, 1 to 10000 (default 100)"})json",
	 "The camera: window units per second (1 to 10,000, the default 100) and the time at the lanes' left edge, "
	 "`offset`. They are applied when they change, so the user's own zoom (+ and -) and scroll (the wheel sideways, "
	 "or a drag with the right or middle button) stay until then.",
	 "timeline"},
    {"offset", R"json({"anyOf": [{"type": "number"}, {"$ref": "#/$defs/v2/dynamic"}]})json", "", "timeline"},
    {"selectedKeys", R"json({"description": "timeline: the selected items' keys"})json",
	 "The selected items' keys (applied when it changes): a clip's id, or a track's id, `:` and a keyframe's index "
	 "(`\"pos:2\"`). A click, Shift and a click, a marquee, or Page Up and Page Down on the keyboard's track changes "
	 "it and runs `onSelect`; dragging the selection (or Alt and the arrows) runs `onKeyframeMove` and "
	 "`onClipChange` for each item.",
	 "timeline"},
    {"headerWidth", R"json({"anyOf": [{"type": "number", "minimum": 0, "maximum": 2000}, {"$ref": "#/$defs/v2/dynamic"}]})json",
	 "The headers' column (default 160) and a row's height (`trackHeight`, 12 to 400, default 28).", "timeline"},
    {"trackHeight", R"json({"anyOf": [{"type": "number", "minimum": 12, "maximum": 400}, {"$ref": "#/$defs/v2/dynamic"}]})json",
	 "", "timeline"},
    {"headerColor", "", "The ruler and headers, the clips (a clip's own `color` first), the text and the selection.",
	 "timeline"},
    {"clipColor", "color", "", "timeline"},
    {"textColor", "", "", "timeline"},
    {"accentColor", "", "", "timeline"},
    {"font", "", "", "timeline"},
});

} // namespace

std::span<const KeyReference> nodeTypeKeys() { return typeTable; }

} // namespace affresco::format
