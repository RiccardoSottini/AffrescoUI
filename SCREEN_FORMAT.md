# AffrescoUI Screen Format

A JSON format for describing a user interface that AffrescoUI builds and shows at runtime: **what is on screen**, plus the
app's **state, bindings and behaviour**, so a whole app can live in the file. Application code can add more (native
functions, custom types), and the catalog tells agents what it added.

Screens are **live**: the UI follows the state, and **revisions** (compact edits or JSON Patch) can change the document,
layout and behaviour while the app runs. A revision can come from the app itself or from a program connected over a
channel, for example an AI agent through the MCP server.

**Current version: 1.** Every file declares `"version"`. The schema keeps each version's definition separately
(`$defs/v1`, later `$defs/v2`, ...) and the loader checks the version first.

- File extension: `.screen.json`. JSON Schema (editor completion and validation):
  [`schema/affrescoui-screen.schema.json`](schema/affrescoui-screen.schema.json), referenced with `"$schema"`.
- Try any file without writing code: `./build/affrescoui_host path/to/file.screen.json`

```json
{
	"$schema": "../schema/affrescoui-screen.schema.json",
	"format": "affrescoui-screen",
	"version": 1,
	"window": { "title": "Hello", "width": 600, "height": 400 },
	"fonts": { "default": { "file": "Roboto-Regular.ttf", "size": 16 } },
	"colors": { "accent": "#6366f1" },
	"state": { "clicks": 0 },
	"root": {
		"color": "#f8fafc",
		"children": [
			{ "type": "label", "text": "Clicked {{state.clicks}} times", "anchors": { "left": 24, "top": 24 }, "width": 200,
			  "height": 44, "align": "center", "color": "@accent", "textColor": "#ffffff", "cursor": "hand",
			  "onClick": { "increment": "state.clicks" } }
		]
	}
}
```

## Design rules

Every part of the format follows these, and so should every extension:

1. **Words, not codes.** Full English keys (`textColor`, `left`, `minWidth`); no abbreviations.
2. **Familiar notation.** CSS-style layout names, `{{ }}` bindings, JS-style expressions inside them.
3. **One key, one meaning** for each type of node.
4. **Named arguments beyond two.** Steps take objects: `{"animate": "state.x", "to": 1, "ms": 300}`.
5. **Locality first, names for reuse.** Behaviour sits on the node that triggers it; an action, style or component gets
   a name once it is used twice.
6. **Reviewable changes.** Edits address nodes by id (`#statusBar/color`), and reviewers see each change as a line:
   `set #statusBar/color to @topBar`. Give every node an agent may edit an `id`.
7. **Comments.** `"note"` is allowed on the document, any node, style, component, channel, and as a step. It is ignored.
8. **Errors name the place**: `root.children[0].anchors: unknown edge 'lefft'`.

## Top level

<!-- generated: sections -->
| Key | Meaning |
|---|---|
| `version` | Required. `1`. |
| `root` | Required. The window's root node. It always fills the window, so layout properties aren't allowed on it. |
| `format` | `"affrescoui-screen"`, for tools. |
| `note` | A comment. |
| `window` | `title`, `width`, `height`, `minWidth`, `minHeight` (integers). |
| `fonts` | `name → { "file", "size", "fallback" }`, paths relative to the screen file (TrueType or OpenType, a collection's first font). `default` is used by text without a `font`. Text is UTF-8, shaped as each script needs (kerning, ligatures, marks, Arabic joining, Indic conjuncts), and shows any character the font has; colour emoji too, from colour (COLR) fonts. `"fallback": ["symbols"]` names other fonts, asked in order for a character this one lacks; their glyphs are drawn at this font's em size. A character none of them has shows as `?`. A right-to-left run (Arabic, Hebrew) reads right to left; the runs of a line follow each other left to right. |
| `colors` | `name → "#rrggbb"`, or per theme `name → { "light": "#fff", "dark": "#111827" }`. Used as `"@name"`. |
| `theme` | Which variant of the colors is shown, usually bound: `"{{state.theme}}"`. Without a match, the first variant. |
| `styles` | `name → properties`, applied with `"style": "name"` or `["a", "b"]`. A style may itself have a `style`. |
| `components` | `name → { "params": {defaults}, "node": node }`: reusable nodes with parameters (see Components). |
| `screens` | `name → node`: the screens an `outlet` shows; `{"navigate": "name"}` switches. The first is shown first. |
| `templates` | `name → node`, built by code on demand (`Screen::instantiate`). |
| `state` | The app's data, any JSON object. |
| `stateTypes` | `key → type`: what each state key holds, written as JSON Schema (see Typed state). |
| `computed` | `name → expression`: values derived from the state, read as `computed.name` (see Values). |
| `watch` | `"state.path" → handler`: runs after an update in which that value changed (`event.value`, `event.previous`). |
| `actions` | `name → steps`, run by handlers, `run` steps, shortcuts and channels. |
| `keys` | Screen-wide shortcuts: `"mod+k" → handler` (see Events). |
| `onLoad` | A handler run once after the first update. |
| `channels` | Programs the screen talks to (see Channels). A capability: revisions can't change it. |
| `persist` | `["state.habits", ...]`: state paths kept on disk across restarts. A capability: revisions can't change it. |
| `include` | `["parts/sidebar.json", ...]`: files merged in at load, section by section (this file's entries win). |
| `permissions` | Who may change the running screen (see [Version control](VERSION_CONTROL.md)). Revisions can't change it. |
<!-- end generated -->

## Nodes

A node is a panel: a rectangle positioned relative to its parent and clipped to it. Children are drawn on top of their
parent, later siblings on top of earlier ones.

### Values
- **Length:** pixels (`120`, `"120px"`) or a percentage of the parent's matching dimension (`"50%"`).
- **Color:** `"#rgb"`, `"#rrggbb"`, `"#rrggbbaa"`, `"@name"` (from `colors`, in the current theme) or `"transparent"`.
- **Align:** `"start"`, `"center"` or `"end"`.

### Every type
<!-- generated: node -->
| Key | Meaning |
|---|---|
| `type` | `panel` (default), `label`, `textarea`, `editor`, `stack`, `row`, `column`, `grid`, `toggle`, `checkbox`, `slider`, `image`, `split`, `tabs`, `list`, `tree`, `table`, `outlet`, or a type the application registers. |
| `id` | A name to address the node (`#id` in edits, `nodes.id`, anchors, `focus`). Every node should have one, and no two nodes the same (the checker reports a duplicate). `affrescoui ids` gives readable ones to a file's nodes (`label-blocked`, `panel-top-bar`), and a node a revision adds without one gets one in the commit. A `use` keeps its own id over its component's. |
| `style` | Style name or list of names; may be computed: `"{{on ? 'chipOn' : 'chip'}}"`. |
| `children` | Array of nodes. |
| `x`, `y` | Position inside the parent (used when the matching edge isn't anchored). |
| `width`, `height`, `minWidth`, `minHeight` | Size. The minimum wins over a smaller size. |
| `maxWidth`, `maxHeight` | The largest size (a length, or `"none"`, the default). The minimum wins over the maximum, as in CSS. |
| `aspectRatio` | Width divided by height, `1.5` or `"16/9"`: the height follows the width, unless only the height is set (a non-zero `height`, opposite anchors, or a container's stretch), then the width follows the height. With both set, it is ignored, as in CSS. |
| `anchors` | `{ "top", "bottom", "left", "right" }`: keep that edge this far from the parent's same edge. Anchoring opposite edges stretches the node. A value `"#id.edge + 8"` puts the edge at another node's edge (`"top": "#header.bottom + 12"`, `"bottom": "#footer.top - 8"`). |
| `fill` | Anchor all four edges this far in (`0` or `true` to cover the parent). |
| `grow` | In a `row`/`column`: the share of the spare room it takes, like CSS flex-grow. |
| `shrink` | In a `row`/`column`: how much of an overflow it gives up, in proportion to `shrink` times its size, down to its minimum (CSS flex-shrink). Default `0`: it keeps its size and the children overflow, as they always have (CSS's default is 1). |
| `basis` | In a `row`/`column`: its size along it before growing or shrinking (CSS flex-basis), a length; `"auto"` (the default) for its own `width` or `height`. |
| `alignSelf` | In a `row`/`column`/`grid`: where it sits across (`start`, `center`, `end`, `stretch`), over the container's `alignItems`. |
| `cell` | In a `grid`: `{ "column": 2, "row": 1, "columnSpan": 2, "rowSpan": 1 }`, counted from 1 (each key optional). Without `column` and `row`, it takes the next free cell, row after row. |
<!-- end generated -->

### Every type: look and behaviour
<!-- generated: look -->
| Key | Meaning |
|---|---|
| `color` | Background. Default: light grey for panels, transparent for labels, stacks and controls. |
| `radius` | Rounds the corners, in pixels: one number for all four (`8`), or `{ "topLeft", "topRight", "bottomRight", "bottomLeft" }` (corners left out are square). A radius larger than half the shorter side is clamped to it, so `999` makes a pill or a circle. Edges are anti-aliased. Children are still clipped to the square bounds, and the corners still take clicks. Default: `0`. |
| `borderWidth`, `borderColor` | A border drawn inside the node's edges (it doesn't change the layout), over the background and under the children, following `radius`. Default: no border; `borderColor` defaults to black. |
| `shadow` | `{ "x": 0, "y": 4, "blur": 12, "color": "#00000040" }`, like CSS box-shadow: the node's shape, moved by `x` and `y` and blurred by `blur` (a Gaussian with a standard deviation of `blur / 2`), drawn under the node. Every key is optional; `color` defaults to `#00000040`. A shadow can fall outside the node, but it is clipped by the parent, like the node itself. |
| `visible` | `false` hides the node and its children. |
| `cursor` | `arrow`, `ibeam`, `hand`, `resize-horizontal`, `resize-vertical`. |
| `tooltip` | Text shown after the pointer rests on the node. |
| `disabled` | `true`: no events (hover styling still applies). |
| `scroll` | `"vertical"`, `"horizontal"`, `"both"` or `"none"`: the wheel or trackpad scrolls the children; a thin bar shows where. |
| `overlay` | `{ "anchor": "#id", "placement": "below", "open": "state.menuOpen", "dismiss": "outside" }`: drawn above everything, unclipped, next to its anchor (default: its parent). Placements: `below`, `below-end`, `above`, `above-end`, `right`, `left`, `center` (in the window), `window` (placed by its own x, y, anchors in the window). `open` is a state path (or a value); with `"dismiss": "outside"`, a click outside it or Escape sets it to false. |
| `hover`, `focused`, `dragging`, `dragOver`, `pressed` | Properties shown in that state (later ones win: pressed over drag over over dragging over focused over hover). Any visual property: `{ "color", "textColor", "borderColor", "shadow", "style", ... }`. A node counts as hovered and pressed while one of its children is. |
| `transition` | `{ "color": 150, "textColor": 150, "borderColor": 150 }`: changes of these colours animate over that many milliseconds. |
| `repeat` | `{ "for": "card", "in": "state.cards", "index": "i", "key": "{{card.id}}" }`: one node per list item. With `key`, a node follows its item when the list is reordered (keeping focus and scroll). |
| `when` | Build the node only if the value is truthy. |
| `let` | `{ "on": "{{tag == state.filter}}" }`: named values for the node and its children, evaluated in order. |
| `use`, `with` | Build a component: `{ "use": "chip", "with": { "label": "{{tag}}" } }` (see Components). |
| `check` | `{ "rule": "{{len(value) >= 3}}", "message": "Too short", "value": ... }`: `value` defaults to the node's bound value; gives the node and its children `valid` and `error`. |
| `every` | `{ "ms": 1000, "do": steps }`: runs while the node exists. |
| `drag` | `{ "data": {...} }`: the node can be dragged; a node with `onDrop` receives `event.data`. |
| `keys` | Shortcuts while the node or a child has the focus: `"enter" → handler`. |
| `locked` | `true`: no revision may change this node or anything inside it. Needs an `id`. |
| `note` | A comment. |
| `on…` | Event handlers (see Events). |
<!-- end generated -->

### `label`
<!-- generated: label -->
| Key | Meaning |
|---|---|
| `text` | The text. |
| `textColor` | Default black. |
| `font` | Font name. Default: `default`. |
| `padding` | Pixels between the edges and the text. |
| `align` | One value for both axes, or `{ "horizontal", "vertical" }`. |
| `wrap` | Break lines that don't fit the width. |
| `fitText` | `true` or `{ "maxWidth" }`: the label sizes itself to its text (plus padding). |
| `icon` | An icon before the text, by name: `"search"`, `"arrow-left"`. A glyph of that name in the label's font or its fallbacks, drawn in `textColor` with a space before the text (alone if there is no text). A name no font has shows nothing. Give the text font an icon font as a fallback (`"fallback": ["icons"]`) and icons are drawn at the text's size, in the same batch; or give an icon-only label the icon font itself, at its own size. The examples ship Lucide (`examples/assets/fonts/Lucide.ttf`, 1,890 icons: https://lucide.dev/icons). Not on text areas. |
<!-- end generated -->

```json
"fonts": { "default": { "file": "Roboto-Regular.ttf", "size": 15, "fallback": ["icons"] },
           "icons": { "file": "Lucide.ttf", "size": 15 } },
...
{ "type": "label", "icon": "trash-2", "text": "Delete", "textColor": "@danger" }
```

### `textarea`
Everything `label` has, plus `placeholder`, `placeholderColor`, `bind` (a state path kept in sync both ways, or
`"{{...}}"` giving one), `onChange` (`event.value`), `onSubmit` (Enter, `event.text`) and `autofocus`. Single line.

### `editor`
A multi-line text editor (Cmd on macOS, Ctrl elsewhere): click, drag, double and triple click select; arrows,
Home/End, Page Up/Down, Alt+arrows (words), Cmd+arrows (line and document ends), Shift to extend; Enter keeps the
indentation; Tab/Shift+Tab indent; Cmd+A/C/X/V/Z, Cmd+Shift+Z. The caret's line is highlighted; the wheel scrolls.
Tabs become 4 spaces. Applications build richer editors on it (syntax colours, commands): see
`examples/CodeEditor.cpp`, the Forge template's `code` type.

<!-- generated: editor -->
| Key | Meaning |
|---|---|
| `bind` | A state path holding the text (lines joined by `"\n"`), kept in sync both ways. Without it, `text` sets the text. |
| `caret` | A state path the editor writes its caret to: `{ "line", "column", "selected" }`, counted from 1. |
| `font`, `textColor` | Font and text colour. |
| `lineNumbers` | `true` shows line numbers in a gutter. |
| `lineNumberColor`, `activeLineNumberColor` | Line numbers, and the caret's line number. |
| `lineColor`, `selectionColor`, `caretColor` | The caret's line background, the selection, the caret. |
| `autofocus`, `onChange` | Focus when built; `event.value` is the new text. |
<!-- end generated -->

### `row`, `column`, `stack`, `grid`
`column` (also `stack`) places its children one below the other, `row` side by side, like a CSS flex container; they
manage their children's positions along that direction and leave the other direction to the children's own anchors,
unless `alignItems` or a child's `alignSelf` places them across too. Each child starts at its `basis` or its own size,
within its minimum and maximum; children with `grow` share the room left over, and children with `shrink` give up an
overflow.

<!-- generated: stack -->
| Key | Meaning |
|---|---|
| `gap` | Pixels between children (and between lines, when it wraps). |
| `rowGap`, `columnGap` | Pixels between rows and between columns, over `gap`: in a column `rowGap` is between children, in a wrapping row between lines. |
| `justify` | How the children are spread along it: `start` (default), `center`, `end` (a column from the bottom, like a chat: overflow is clipped at the top), `space-between`, `space-around` or `space-evenly`, as in CSS. |
| `from` | The older form of `justify`, with `start`, `center` or `end`. `align` is an older name for it on `stack`. |
| `alignItems` | Where the children sit across: `start`, `center`, `end` or `stretch` (as tall as a row, as wide as a column, within their maximum). Left out, they keep their own anchors across. |
| `direction` | On a `stack`: `"row"` or `"column"`. |
| `wrap` | Continue on a new line (in a column, a new column) when a child doesn't fit; each line grows, shrinks and is spread on its own, and sits at the start of its line unless aligned. |
<!-- end generated -->

`grid` places its children in columns and rows, like a CSS grid: each in its `cell`, or in the next free cell, row
after row. A child is as wide as its cell and keeps its own height at the top of it, unless `justifyItems`,
`alignItems` or its `alignSelf` say otherwise.

<!-- generated: grid -->
| Key | Meaning |
|---|---|
| `columns` | A number of equal columns (default `2`), or a list of tracks: pixels (`200`), a percentage of the grid (`"25%"`), a share of the room left (`"1fr"`), `"auto"` (as wide as its widest child) or `"minmax(100, 1fr)"`. `"1fr"` is `minmax(0, 1fr)`, so equal fractions stay equal whatever their content; write `"minmax(auto, 1fr)"` to keep a track as wide as its content. At most 1,000. On a `table`: the columns, left to right: a string (its id and title), or `{ "id", "title", "width" }` (pixels, default 120, at least 24). Column i shows each item's cell i. Dragging a header's right edge resizes it and runs `onColumnResize`; keep the width in state there to keep it over a change of columns. |
| `rows` | Tracks for the rows, as `columns`. Rows past them, and every row without `rows`, are as tall as their tallest child. |
| `gap`, `rowGap`, `columnGap` | Pixels between cells; `rowGap` and `columnGap` over `gap`. |
| `justifyItems` | Where children sit across their cells: `stretch` (default), `start`, `center` or `end`. |
| `alignItems` | Where children sit down their cells: `start` (default), `center`, `end` or `stretch`. |
<!-- end generated -->

### `toggle`, `checkbox`, `slider`
Value controls. `bind` keeps the value in a state path; without it, `checked` (toggle, checkbox) or `value` (slider)
shows a value and `onChange` (`event.value`) reacts. A control keeps its own clicks (they don't bubble).

<!-- generated: control -->
| Key | Meaning |
|---|---|
| `trackColor` | The empty or off part. |
| `activeColor` | The filled or on part. |
| `knobColor` | The knob (or the inside of the checkbox). |
| `min`, `max`, `step` | Slider range (default 0 to 100) and snapping. |
<!-- end generated -->

### `image`
A PNG or JPEG picture, fitted to the node. Transparent until it arrives: files are decoded on a worker thread, and the
picture appears when it is ready (a picture of the screen, `render`, waits for every one).

<!-- generated: image -->
| Key | Meaning |
|---|---|
| `file` | A PNG or JPEG file, relative to the screen file. It must be inside the screen file's folder (or the folders inside it), or a folder the host allows (`Screen::allowImageDirectory`): a revision naming any other file is refused. At most 64 MiB, 8,192 pixels a side and 4,096 × 4,096 pixels in all. A file that can't be read or decoded shows nothing (and is logged). |
| `source` | Instead of `file`: an image the host names (`Screen::setImage("chart", …)`), e.g. a picture it made; the node shows each new one the host sets. |
| `fit` | `"contain"` (the default: all of it, as large as fits, centred), `"cover"` (the whole node, the image's edges cut off) or `"fill"` (stretched), like CSS `object-fit`. |
<!-- end generated -->

```json
{ "type": "image", "id": "avatar", "file": "images/avatar.png", "width": 48, "height": 48, "fit": "cover" }
```

Small images (up to 256 pixels a side) share textures, so a list of thumbnails draws in one batch with its text.

### `split`
Its children are panes laid side by side (or one above the other), with a sash between each two: drag a sash to
resize, double-click it to go back to the default sizes. With the split focused, the arrow keys move the chosen sash
by 8 pixels (32 with Shift), Page Up and Page Down choose another, Home resets. A pane never gets smaller than its
`minSize`; the last pane takes what is left.

<!-- generated: split -->
| Key | Meaning |
|---|---|
| `direction` | `"row"` (the default: panes side by side, vertical sashes) or `"column"` (one above the other). |
| `value` | The panes' sizes in pixels, in order (`bind` keeps them in a state path). The last pane takes what is left; missing sizes share it equally. Moving a sash runs `onChange` with the new sizes. |
| `defaultSizes` | The sizes a double click on a sash (or Home on the focused split) goes back to; left out, equal panes. |
| `minSize` | The smallest size of every pane (a number), or of each in order (a list; its last one is used for the rest). |
| `sashSize` | Pixels between two panes (default 4); the pointer finds a thin sash a little either side. |
| `sashColor`, `sashActiveColor` | The sash (default transparent) and the sash being dragged or chosen by keyboard. |
<!-- end generated -->

```json
{ "type": "split", "id": "workbench", "bind": "state.layout.sizes", "minSize": [160, 240], "defaultSizes": [240],
  "children": [ { "type": "panel", "id": "side" }, { "type": "panel", "id": "editors" } ] }
```

### `tabs`
A strip of tabs, one of them active. The tabs come from the node (`tabs`, usually bound to state), and the strip
reports what the user does, so the state stays the one source: picking a tab changes `value` (and `onChange`), the
close button or Delete runs `onClose`, and dragging a tab along the strip runs `onMove` (`event.from`, `event.to`,
counted without the tab). The arrow keys, Home and End pick tabs; the strip scrolls with the wheel when they don't fit.
With `"drag": {}` on the node, a tab dragged onto another `tabs` drops there: that strip's `onDrop` gets
`event.data` (`{ "id", "index" }`) and `event.index`, the place it goes, shown by a line while dragging.

<!-- generated: tabs -->
| Key | Meaning |
|---|---|
| `tabs` | The tabs, in order (or a binding such as `"{{state.open}}"`): a string (its id and label), or `{ "id", "label", "marker", "closable" }`. `marker` shows a dot (e.g. unsaved changes) where the close button goes; `closable` defaults to true. At most 10,000; the strip scrolls sideways when they don't fit. |
| `value` | The active tab's id (`bind` keeps it in a state path); picking a tab runs `onChange`. |
| `textColor`, `activeTextColor`, `activeBackground`, `accentColor` | The labels' colour (default muted), the active tab's, its background and its line. |
<!-- end generated -->

```json
{ "type": "tabs", "id": "editor-tabs", "height": 32, "tabs": "{{state.open}}", "bind": "state.active",
  "onClose": [ { "set": "state.open", "to": "{{filter(state.open, it.id != event.id)}}" } ] }
```

### `list`, `tree` and `table`
Rows of items, one or more of them selected, drawn only where they are in view: a list of a million items scrolls
and selects as fast as one of ten. The items come from the node (`items`, usually bound to state) and the selection
goes back to it (`value`, `bind`, `onChange`). A click selects a row, Shift a range, Cmd or Ctrl one more; the arrow
keys, Page Up and Page Down, Home and End move the selection, and typing a row's first letters finds it. Enter or a
double click runs `onOpen`. A `tree` nests its items' `children` with indent guides: a click on the chevron, or Right
and Left, open and close a node (`onExpand`), and Left goes to the parent. With `"drag": {}` on the node, rows drag
(`event.data` is `{ "id", "ids" }`: the row, and every selected one if it is selected) and drop onto any `list` or
`tree`, whose `onDrop` gets the row and the place (a line between rows, or an outline around a tree's node). With
`stickyRows`, a tree pins the open folders of the top row while their children scroll by. A `table` shows each item's
`cells` under its `columns`, with a header: a click on a title sorts by it (`sort`, `onSort`), and dragging a title's
right edge resizes the column (`onColumnResize`). When the items change after the view has scrolled, the row at the
top stays in place.

<!-- generated: list -->
| Key | Meaning |
|---|---|
| `items` | The rows, in order (or a binding such as `"{{state.files}}"`): a string (its id and label), or `{ "id", "label" }`; in a `tree`, also `"children"` (a list of items) and `"expandable"` (a closed node whose children aren't loaded yet: `onExpand` can load them); in a `table`, `"cells"`: the texts of the row's columns, in order (at most 256). Only the rows in view are drawn, so a list scrolls through a million items as fast as through ten. At most 1,000,000 items, nested at most 256 deep. |
| `expanded` | On a `tree`: the ids of the open nodes. Opening or closing one runs `onExpand`; keep the ids in state there to keep them over a rebuild. |
| `stickyRows` | On a `tree`: how many open folders stay pinned at the top while their children scroll by, nearest the root first (default 0, at most 16). A click on a pinned folder moves the cursor to it. |
| `sort` | On a `table`: the rows' order, `{ "column": id, "descending": false }` (or null for the items' order). A click on a header sorts by its column, ascending then descending, and runs `onSort`: numbers by value before text, equal cells in the items' order. |
| `selection` | `"single"` (the default), `"multiple"` (Shift adds a range, Cmd or Ctrl one row) or `"none"`. |
| `value` | The selected id, or with `"multiple"` the list of them (`bind` keeps it in a state path); a change runs `onChange`. |
| `rowHeight` | Each row's height in pixels (default 24). |
| `font`, `textColor`, `selectedTextColor`, `selectedBackground`, `accentColor` | The rows' font and colours: their text, the selected rows' text and background, and the accent (the keyboard cursor, the drop line, a tree's guides). |
<!-- end generated -->

```json
{ "type": "tree", "id": "explorer", "items": "{{state.files}}", "expanded": "{{state.open}}", "bind": "state.chosen",
  "onExpand": [ { "set": "state.open", "to": "{{event.expanded ? concat(state.open, [event.id]) : filter(state.open, it != event.id)}}" } ] }
```

### `outlet`
Shows the current screen from `screens`, filling the outlet unless the screen places itself.

### Components
```json
"components": {
	"chip": { "params": { "label": "", "on": false },
	          "node": { "type": "label", "text": "{{label}}", "style": "{{on ? 'chipOn' : 'chip'}}" } }
}
...
{ "use": "chip", "with": { "label": "{{tag}}", "on": "{{tag == state.filter}}" }, "onClick": "..." }
```
The use's other properties override the component's node; `with` values are evaluated where it is used and become
variables of the node and its children. A child `{ "slot": true }` in the component is replaced by the use's
`children`. Changing a component changes every use: one edit restyles a whole list. Nodes built inside each other
(components and outlets included) nest at most 256 deep, so a component or a screen inside itself is an error.

## Values, expressions and state

Wherever a value is expected it can be computed:

| Form | Example | Meaning |
|---|---|---|
| Binding | `"{{state.threads[state.current].title}}"` | The value of an expression, keeping its JSON type. |
| Template | `"Clicked {{state.clicks}} times"` | Text with values inserted. |
| Operator | `{ "$clamp": ["{{event.x}}", 180, 520] }` | The same thing as JSON; expressions compile to these. |

**Expressions** are JS-like, compiled once and cached:
- literals: `12`, `1.5`, `'text'` or `"text"`, `true`, `false`, `null`, `@accent` (a colour name), `[1, 2]`,
  `{ key: value }`;
- paths: `state.items[state.current].title`, `card.tags.length`; a missing value is `null`; a name with `-` goes in
  brackets, `state['my-list']`, because `state.my-list` is `state.my - list`;
- `!`, `-`, `* / %`, `+ -` (`+` joins text when either side is text), `< <= > >=`, `== !=`, `&&`, `||`,
  `a ?? b` (b when a is null), `c ? a : b`; `&&` and `||` give an operand, as in JavaScript: `user && user.name` is the
  name (or `user` when it is false or null), `title || 'Untitled'` is the title unless it is empty;
- functions: `empty(value)`, `type(value)`, `round(number)`, `floor(number)`, `ceil(number)`, `abs(number)`,
  `min(a, b, ...)` (numbers or a list), `max(a, b, ...)` (numbers or a list), `clamp(value, low, high)`, `upper(text)`,
  `lower(text)`, `trim(text)`, `split(text, separator)`, `number(value)`, `text(value)`, `fixed(number, digits)`,
  `pad(value, width, fill?)` (width up to 10000), `plural(count, word, plural?)`, `parseJson(text)`, `json(value)`,
  `len(value)`, `concat(a, b, ...)`, `join(list, separator)`, `slice(list or text, from, to?)`,
  `contains(list or text or object, value)`, `indexOf(list or text, value)`, `keys(object)`, `values(object)`,
  `entries(object)`, `merge(a, b, ...)`, `reverse(list or text)`, `range(count)` (count up to 10000), `now()`,
  `time(milliseconds, 'ddd D MMM HH:mm')`;
- per item, with the item as `it` and its position as `index`: `filter(list, condition)`, `map(list, expression)`,
  `count(list, condition?)`, `sum(list, expression?)`, `find(list, condition)`, `some(list, condition)`,
  `every(list, condition)`, `sort(list, key?, 'desc'?)`.

Each function is also an operator: `len(x)` is `{"$length": x}`, `filter(l, c)` is `{"$filter": [l, c]}`. Texts
are UTF-8: `len`, `indexOf` and `slice` count bytes, `split(text, '')` and `reverse` keep characters whole.

**Variables:** `state`; `computed`; `event` (what triggered a handler); `window` (`width`, `height`); `theme`;
`screen` (the screen an outlet shows); `nodes` (`nodes.sidebar.width`: a built node's `x`, `y`, `width`, `height`,
`visible`, from the last layout); `revision` (the current commit: `number`, `id`, `short`, `author`, `committer`,
`message`, `time`); `history`; `proposals` (`id`, `author`, `message`, `head`, `short`, `commits`, `operations`,
`touches`, `changes`, `remember`, `reason`, `time`); `preview`; `mode`; and the names given by `repeat`, `let`,
`with`, `forEach`, a component's params, `it`/`index` inside list functions.

**`computed`** values are evaluated when first read after the state changed, then cached; they may use each other
(not in a circle, and at most 32 deep). Prefer them to actions that recount things by hand.

**Truthy** means anything except `false`, `null`, `0`, `""`, `[]` and `{}`.

**Work budget:** an event (a handler, a timer, `onLoad`, `watch`) or an update may run 100,000 steps, evaluate
10,000,000 expression operations (each item a list function visits counts) and write 64 MiB into the state. Past
one, what is running stops with an error that names its place (`actions.spin[0]: the event ran more than 100000
steps, its work budget: the action stopped`), and the screen goes on. An update that stops refuses the version that
caused it. A revision may have 100,000 operations and 16 MiB; a document may have 32 MiB, and so may its state
(SECURITY.md, "Limits").

### Typed state
`stateTypes` declares what a state key holds, one JSON Schema per key:

```json
"stateTypes": {
	"filter": { "enum": ["all", "bug", "feature"] },
	"volume": { "type": "number", "minimum": 0, "maximum": 100 },
	"cards": { "type": "array", "items": { "type": "object", "properties": { "title": { "type": "string" } } } }
}
```

- **Keywords:** `type` (`string`, `number`, `integer`, `boolean`, `object`, `array`, `null`, or a list of them),
  `enum`, `const`, `properties`, `required`, `additionalProperties` (`true`, `false` or a type), `items`, `minimum`,
  `maximum`, `minLength`, `maxLength` (characters), `minItems`, `maxItems`; notes: `description`, `title`, `default`,
  `examples`, `note`. `true` is any value, `false` none. Another keyword is an error that names its place.
- **The screen's own `state`** must match: a mismatch is an error when the screen is read
  (`state.volume: expected at most 100, got 120 (stateTypes.volume)`).
- **A revision** may not bring a state value that doesn't match, nor a type the state doesn't match; mismatches that
  were there before don't block it. A state kept on disk that no longer matches opens with a warning.
- **The checker** reports a `set` whose value can't match (a value written as it is is matched in full, enums and
  limits too; an expression by the kind of value it gives), a `toggle` of something that isn't a boolean, an
  `increment` or `animate` of something that isn't a number, a `push` onto something that isn't a list or of an item
  that doesn't match, a `bind` whose widget writes another kind of value (a toggle or checkbox writes true or false,
  a slider a number, a text area or editor text), and `a == b` or `a != b` comparing a state path that has an `enum`
  with a value it never takes. A declared key or property can be read even before the state holds it; an object's
  other keys are read only if its type says `"additionalProperties": true` or a type.
- **While the screen runs,** steps and widgets aren't stopped by the types: they are a contract for revisions and a
  description for agents and tools.

## Events

A handler is an action name, a step, or a list of steps. Pointer events get `event.x`/`y` (in the window),
`event.left`/`top` (in the handling node) and its `width`/`height`.

<!-- generated: events -->
| Handler | When |
|---|---|
| `onClick`, `onDoubleClick`, `onContextMenu` | Left click, two clicks on the same node, right click. |
| `onPress`, `onRelease` | Left button down, up. |
| `onDrag` | The pointer moves while pressed: `event.dx`, `event.dy` since the press. |
| `onDrop` | A dragged node was dropped here: `event.data` (on `tabs`, also `event.index`, where it goes; on a `list` or `tree`, `event.index`, `id` and `place`: `"before"`, `"inside"` or `"after"` that row). |
| `onHover`, `onLeave` | The pointer enters or leaves the node (or a child). |
| `onFocus`, `onBlur` | Keyboard focus. |
| `onScroll` | `event.dx`, `event.dy`. |
| `onKey` | A key on the focused node: `event.key`, `shortcut`, `shift`, `ctrl`, `alt`, `cmd`. |
| `onChange` | A bound or value control changed: `event.value`. |
| `onSubmit` | Enter in a text area: `event.text`. |
| `onShow`, `onHide` | The node was built, or removed (by `when`, `repeat` or a revision). |
| `onClose` | A tab's close button or Delete: `event.id`. |
| `onMove` | A tab dragged along its strip: `event.id`, `from`, `to`. |
| `onExpand` | A tree row opened or closed: `event.id`, `expanded`. |
| `onOpen` | Enter or a double click on a tree, list or table row: `event.id`. |
| `onSort` | A table header clicked: `event.column`, `descending`. |
| `onColumnResize` | A table column's edge dragged and let go: `event.column`, `width`. |
<!-- end generated -->

**Bubbling:** click, double click, context menu, press, release, drag, scroll and key events go to the node or, if it
has no handler, its nearest ancestor with one. A row with `onClick` needs no invisible overlay above its labels.

**Shortcuts:** `"keys"` on the document (anywhere) or on a node (while it or a child is focused): `"mod+k"`,
`"cmd+shift+p"`, `"escape"`, `"up"`, `"f5"`. `mod` is Cmd on macOS, Ctrl elsewhere. While a text area is focused, keys
without Cmd, Ctrl or Alt go to the text (Escape still works).

## Actions

An action is a list of steps. Each step is an object whose first key says what it does:

<!-- generated: steps -->
| Step | Example |
|---|---|
| `set` | `{ "set": "state.current", "to": "{{index}}" }` |
| `push` | `{ "push": "state.columns[c].cards", "value": "{{state.draft}}" }` |
| `remove` | `{ "remove": "state.items", "at": 0 }`, or without `at` to delete a key of an object |
| `toggle` | `{ "toggle": "state.dark" }` |
| `increment` | `{ "increment": "state.clicks", "by": 1 }` |
| `if` | `{ "if": "{{len(state.draft) == 0}}", "then": [...], "else": [...] }` |
| `forEach` | `{ "forEach": "state.items", "as": "item", "index": "i", "do": [...] }` |
| `while` | `{ "while": "{{state.n < 10}}", "do": [...] }` (at most 10,000 iterations) |
| `run` | `{ "run": "moveCard", "with": { "from": "{{c}}", "to": "{{c + 1}}" } }`: an action with variables; the name may be computed |
| `call` | `{ "call": "pickFile", "with": {...}, "into": "state.file", "onError": [...] }`: a native function (`Screen::onCall`); `error` in `onError`. A slow one (`Screen::onAsyncCall`) runs on a worker thread: `"loading": "state.busy"` is true while it runs, `"progress": "state.done"` goes from 0 to 1, partial results stream into `into`, `"timeout": 5000` ends it with the error `timeout`, `then` runs with `result` |
| `cancel` | `{ "cancel": "search" }`: stop the running slow calls of a function; they change nothing more |
| `emit` | `{ "emit": "saved", "data": {...} }`: an event for every connected peer |
| `send` | `{ "send": "host", "data": {...} }`: a message on a declared channel |
| `connect` | `{ "connect": "host" }`: connect a declared channel now |
| `patch` | `{ "patch": [JSON Patch ops], "message": "..." }`: the app revises its own document |
| `navigate` | `{ "navigate": "settings" }` or `"back"` |
| `focus`, `scrollTo` | `{ "focus": "#input" }`, `{ "scrollTo": "#row7" }` (after the update) |
| `after` | `{ "after": 2000, "do": [...] }`: later |
| `debounce` | `{ "debounce": 300, "do": [...] }`: later, unless run again in the meantime (search boxes) |
| `animate` | `{ "animate": "state.progress", "to": 100, "ms": 300, "easing": "out" }` (`linear`, `in`, `out`, `inOut`) |
| `copy` | `{ "copy": "{{state.link}}" }`: to the clipboard |
| `log`, `note`, `stop` | `{ "log": "n is {{state.n}}" }`, a comment, `{ "stop": true }` (end the action) |
| `preview`, `endPreview`, `accept`, `reject`, `restore`, `revert`, `undo` | Review and history, e.g. `{ "accept": "{{proposals[0].id}}", "head": "{{proposals[0].head}}" }` (`accept` names the head the user reviewed; refused if the proposal moved since) |
<!-- end generated -->

**Control steps** (`call`, `send`, `connect`, `patch` and the review and history steps) can be written in the screen
file, but no revision may add or change them: an agent can rearrange how the app uses the outside world and its own
history, never widen it. Nor may a revision move one to another trigger (a button's `onClick`, an `every` timer,
`keys`, `watch`, `onLoad`), or add a `run` or a handler that names an action with control steps, a native action, or
a computed action name.

Only paths under `state` can be changed. A step can't write inside a value that is neither an object nor a list, nor
more than 10,000 positions past a list's end (the positions between are filled with `null`). After an event's steps
finish, the UI is updated once. Panels are reused where
the structure didn't change, so focus, typing, scrolling and drags in progress are kept. Errors in actions are
reported (on stderr and to the peers) and never crash the app.

## Channels and persistence

```json
"channels": { "host": { "socket": "studio.sock", "reconnect": { "after": 2000 } } },
"actions": {
	"host:committed": [{ "push": "state.log", "value": "{{event.message}}" }],
	"host:closed": [{ "set": "state.connected", "to": false }],
	"refresh": [{ "send": "host", "data": { "type": "getHistory" } }]
}
```
A channel connects to a local socket (relative paths are in `$AFFRESCOUI_SOCKET_DIR`, else `/tmp/affrescoui-<uid>`, or
`%LOCALAPPDATA%\affrescoui\sockets` on Windows).
Each message of type `T` runs the action `name:T` with the message as `event`; `name:open`, `name:closed` and
`name:error` (a `send` while disconnected) run when that happens. `reconnect` (or `true`: 2 s) tries again after a
disconnect. The AffrescoUI Studio is built this way: its file is the whole client.

`"persist": ["state.habits"]` keeps those state values in `~/.affrescoui/<file name>.state.json` (on Windows,
`%APPDATA%\affrescoui\<file name>.state.json`), loaded at start and written when they change. A screen opened from
a file keeps its version-control history in the same folder, in `<file name>.history/` (VERSION_CONTROL.md,
"History on disk"). `AFFRESCOUI_DATA_DIR` (or the host's and the Studio's `--data`) puts both
in another folder.

## Live screens: revisions and the channel

### Revisions
A revision is a version-controlled change to a running screen. Write it as **compact edits**:

```json
{ "type": "revision", "author": "assistant", "message": "Dark status bar and a priority dot",
  "set":    { "#statusBar/color": "@topBar", "/state/board": "Q4 launch" },
  "add":    { "#card/children/-": { "id": "priority", "when": "{{card.points > 3}}", "width": 8, "height": 8 } },
  "remove": ["#oldBadge"],
  "move":   { "#filters": "#toolbar/children/0" } }
```

- `set` replaces a value, or adds it where nothing is yet; `add` inserts (`/-` appends to a list); `remove` takes paths;
  `move` maps from-paths to to-paths. They apply in that order: set, add, remove, move.
- Or as a standard [JSON Patch (RFC 6902)](https://datatracker.ietf.org/doc/html/rfc6902) in `patch` (applied before
  the compact edits), when the order of operations matters.
- Paths can start with `#id` to address a node by its id: `#header/children/-`, `#title/text`, or just `#badge`.
  An id inside a `repeat` addresses the template (all instances); instances change through the state.
- Operations on `/state/...` change the live state; all others change the document.
- Revisions are **atomic**. If anything fails, nothing changes and the error names the operation, the path and a
  hint: `revision.patch[0] (set #titel/color).path: no node with id 'titel' (did you mean '#title'?)`,
  `... (remove #title/colr): key 'colr' not found at /root/children/1/colr ('#title' has no 'colr' (it has: type, id,
  text))`.
- A node added without an `id` gets one (see `id` above), written into the commit, so history and diffs show it; a
  node that replaces another keeps that one's id.
- `parent` (optional): a revision number or commit id; if main has moved on, the revision is rejected as a conflict.
  `proposal` (optional) pushes onto the author's own open proposal.
- Every applied revision is a **commit** on `main` (SHA-256, numbered from `0`, the file). Compact edits are stored as
  plain JSON Patch, so history, diffs, reverts and change sets have one form.
- **Permissions** decide whether a revision is applied, proposed for review or rejected; see
  [VERSION_CONTROL.md](VERSION_CONTROL.md). Proposals list their changes as lines for the reviewer
  (`set #statusBar/color to @topBar`).

### Peers and the live protocol
The screen doesn't care where changes come from. Any number of **peers** read and change it with one JSON object per
line:

- `Screen::listen(executor, "/tmp/affrescoui-501/agent.sock", "agent")` accepts peers on a local socket;
  `Screen::connect(channel, "name")` adds one over any `affresco::Channel`.
- **The host decides who a peer is**, by the socket it came through: its revisions are authored as that name, followed
  by the name the peer gives itself (`agent/claude-code`).

| Direction | Message |
|---|---|
| → peer | `{"type": "hello", "format", "revision", "head", "document", "state", "mode", "you", "canReview"}` on connect |
| → peer | answers to its revisions: `applied` (`revision`, `commit`), `proposed` (`proposal`, `commit`) or `rejected` (`error`) |
| → all peers | `committed` (a commit landed on main), `review` (proposals, preview, mode, grants changed), a reviewer's decision, `event` (from `emit`), `error` |
| → peer | answers: `document` (with head, proposals, mode, grants), `history`, `diff`, `blame` (`target`, `commit`), `changes`, `catalog`, `image`, `done` |
| peer → | `revision` (compact edits or `patch`, `proposal`?), `revert` / `restore` (`revision`), `undo` |
| peer → | `getDocument`, `getHistory` (`limit`?), `diff` (`from`, `to` or `proposal`), `blame` (`target`: `#id` or a path), `exportChanges` (`from`, `name`), `getCatalog`, `render` (`proposal`?, `scale`?) |
| reviewer → | `accept` (`proposal`, `head`, `dontAskAgain`?), `reject`, `preview`, `endPreview`, `setMode`, `revoke` |
| peer → other peers | `request` (`id`, `text`) and `reply` (`request`, `text`), relayed with `from` |

Requests may carry an `"id"`; answers repeat it.

**Limits per peer** (`channel::PeerLimits`; the host may set others). A message over a limit is refused, and the first
refusal in a row is logged:

| Limit | Default | Answer |
|---|---|---|
| One message | 8 MiB | `error` "message too large", without `id` (it isn't read) |
| Messages of any kind | 100 a second, 200 at once | `error` "too many messages", without `id` |
| Changes (`revision`, `restore`, `revert`, `undo`) | 120 a minute, 30 at once | `rejected` "too many changes", with `id` |
| Pictures (`render`), apart from the changes | 120 a minute, 30 at once | `rejected` "too many pictures", with `id` |
| Sent to the peer and not yet read by it (a socket peer) | 64 MiB | the peer is disconnected |

On a socket, the host also asks the operating system which user the program on the other end runs as, and refuses a
program of another user.

### Pictures
`render` (MCP: `render_screen`) lets a peer see its work before a person reviews it. Any peer may ask: it is a read.

```json
{ "type": "render", "proposal": 3, "scale": 1, "id": 7 }
{ "type": "image", "format": "png", "data": "iVBORw0KGgo…", "width": 1280, "height": 800, "scale": 1,
  "proposal": 3, "revision": 12, "id": 7 }
```

- Without `proposal` it shows main; with one, that open proposal applied to main. `scale` is 1 (the default) or 2 (like a
  Retina display); the picture is the window's size times `scale`. `data` is the PNG file in base64.
- It is drawn offscreen, the way the rendering check draws (`tests/render`). The person's window never shows it: to draw
  a proposal the host previews it, draws, and puts back what was shown, within one step on the main thread. Peers hear
  no `review` messages for it, and while it is drawn `emit` events are dropped and `persist` doesn't save the state.
  Steps that run when panels appear or values change (`onShow`, `onHide`, `watch`) do run, as for a reviewer's
  `preview`. Those that putting the view back starts (the proposal's `onHide`, main's `onShow` and `watch`) run after
  the picture, on main's live state, and steps main had pending run inside the proposal's preview: known limits.
- `rejected` (`error`) answers a proposal that doesn't exist, a scale out of range, or a host without a window.

### Catalog
`getCatalog` (MCP: `get_catalog`) answers what this screen's host offers: node types and their properties, common
properties, events, steps, control steps, operators, variables and sections, plus the custom types, native actions and
native functions the application registered. Agents read it instead of guessing.

## Scenarios

A screen can ship its own checks: `<name>.scenarios.json` next to it, scripted input and expectations that
`affrescoui test` (and the MCP tool `run_scenarios`) plays, so a person or an agent proves a change with the screen's
own scenarios. Schema: [`schema/affrescoui-scenarios.schema.json`](schema/affrescoui-scenarios.schema.json).

```json
{
	"format": "affrescoui-scenarios",
	"version": 1,
	"screen": "kanban.screen.json",
	"scenarios": [
		{ "name": "add-a-card", "steps": [
			{ "click": "#newCard" },
			{ "type": "Write tests" },
			{ "key": "enter" },
			{ "expect": { "state": "columns[0].cards.length", "is": 5 } },
			{ "expect": { "node": "#cardTitle", "nth": 4, "text": "Write tests" } }
		] }
	]
}
```

Each scenario starts from the screen as loaded (no saved history or state) and plays alone: its channels find no host,
and the clock moves only when a step waits, so it plays the same way on any machine. It runs headless; only a picture
needs OpenGL. A scenario may name its own `screen`, and `screens` gives the folder they are in (default: the file's).
`scale` (1 to 4) sets the pixels per window unit of its pictures. `note` is allowed on the file, a scenario and a step.

| Step | Does |
|---|---|
| `{ "click": target }` | Moves the pointer there, presses and releases the left button. `move`, `press`, `release` do one part |
| `{ "drag": target, "to": target, "moves": 4 }` | Presses, moves there in steps, releases. `toNth` picks among the matches of `to` |
| `{ "scroll": target, "dx": 0, "dy": -3 }` | Scrolls there |
| `{ "type": "text" }` | Types into the focused node, a character at a time |
| `{ "key": "mod+z" }` | Presses a key: `enter escape tab backspace delete insert space left right up down home end pageup pagedown slash backslash minus equal comma period semicolon apostrophe leftbracket rightbracket`, `a`–`z`, `0`–`9`, `f1`–`f12`, after any of `shift+ ctrl+ alt+ cmd+ mod+` (`mod` as screens write it: Cmd on macOS, Ctrl elsewhere) |
| `{ "wait": 500 }` | Moves the clock on, running timers, animations and tooltips as they come due. What a handler leaves for the next frame (`focus`) runs at the next wait: `{ "wait": 0 }` |
| `{ "revision": { ... } }` | Applies a revision as the application would |
| `{ "action": "name", "event": { ... } }` | Runs an action; `<channel>:<type>` plays what a channel would send |
| `{ "resize": [960, 720] }` | Resizes the window |
| `{ "expect": ... }` | Checks something (below) |
| `{ "capture": "name" }` | Saves a picture as `<scenario>-<name>.png` (`affrescoui test --out`) |

A target is `"#id"`, the centre of the first shown node with that id (`"nth": 2` picks the third), or `[x, y]` in
window units. Expectations:

| Expectation | Holds when |
|---|---|
| `{ "state": "cards[0].title", "is": "Plan" }` | The live state has that value there (a path as expressions write it, without `state.`; `.length` counts a list or a text) |
| `{ "node": "#menu", "exists": false }` | A node with that id was built (or not), shown or not |
| `{ "node": "#title", "text": "Done" }` | Its text: a label's, a text area's or an editor's |
| `{ "node": "#box", "property": "color", "is": "#ffffff" }` | A property: `text textColor color borderColor borderWidth shown visible focused hovered tooltip checked value x y width height scrollX scrollY` (positions and sizes in window units, within 0.01) |
| `{ "image": "first-frame" }` | The window, drawn, is pixel for pixel the golden image `<scenario>-first-frame.png` (`affrescoui test --goldens DIR`) |

A failed expectation is reported and the scenario goes on; a step that can't be played (no such node, a refused
revision) ends it. The report is one line per file and one per failure:
`FAILED add-a-card, step 4: state columns[0].cards.length is 4, expected 5`. Limits: 1,000 scenarios a file, 10,000
steps a scenario, a `wait` of 10 minutes, 100,000 bytes typed at once.

## Errors

Loading stops at the first problem and names its place in the file:

```
AffrescoUI screen: root.children[0].anchors: unknown edge 'lefft' (expected top, bottom, left or right)
AffrescoUI screen: root.children[0].width: expected a length: a number of pixels, "12px" or "50%"
AffrescoUI screen: root.children[2].text: in {{a ? b}}: expected ':' (at column 6)
```

### The checker
Typos in names would otherwise show up only when that part runs. The checker reads the whole screen without running
it, the way the runtime builds it (components, styles, `repeat`, `let`, `with`, actions run with their callers'
variables), and finds what is referred to but not defined, each with its place and a hint:

```
AffrescoUI screen: root.children[2].text: 'card.titel' doesn't exist (did you mean 'card.title'?)
AffrescoUI screen: root.children[1].onSubmit: unknown action 'addCrad' (did you mean 'addCard'?)
AffrescoUI screen: root.children[3].with.bye: unknown parameter 'bye' of component 'moveButton' (did you mean 'by'?)
```

- **What it finds:** state paths, computed values and variables that expressions read but that don't exist (from
  `state`, `computed`, and the variables of `repeat`, `let`, `with`, `forEach`, `check`, components and events);
  actions (handlers, `run`, `keys`), styles, components, fonts, colours (`@name`) and screens (`navigate`) that
  aren't defined; component parameters the component doesn't declare; unknown steps; expressions that don't parse;
  an `id` that two nodes have.
- **What it learns:** a state key exists if the state has it or a step writes it (`set`, `push`, `toggle`,
  `increment`, `animate`, `call ... into`, `bind`, `caret`, an overlay that `dismiss`es). What a list holds comes
  from its items and what is pushed onto it.
- **What it leaves alone:** values it can't know, so it never reports a typo that isn't one: `event`, `nodes`, a
  native call's result, an empty object (a map, filled later), a value set to `null` or to values of another type,
  names an expression computes (`{"run": "{{name}}"}`, `"@{{card.tag}}Soft"`), and actions and components nothing
  uses (any caller's variables may reach them). Actions the application registers (`Screen::onAction`) are known.
- **On load,** findings are warnings (logged), and the screen opens.
- **For a revision,** findings it brings in reject it, with the messages; findings already there don't block it.
  Places move when nodes are added, so a finding counts as new only if its problem occurs more often than before.

A check visits each part of the document once (a repeat is not expanded), so it takes well under a frame: 0.05 to
0.2 ms for the templates (median, Release build, Apple M4 Pro). Past a million values visited, a screen is not checked.

## From code

```cpp
affresco::Application app;
affresco::Screen screen = affresco::Screen::fromFile("kanban.screen.json");
screen.registerType("chart", [] { return std::make_unique<MyChart>(); }); // a custom type, before createWindow
// A custom type has the properties of the built-in type it derives from, plus its own, given here with their handler.
// Registered under the name of a built-in type, it replaces that type.
screen.registerType("gauge", [] { return std::make_unique<MyGauge>(); }, {"needle"},
                    [&](affresco::Panel& panel, const std::string& propertiesJson) { /* {"needle": "@accent"} */ });
screen.onCall("pickFile", [](const std::string& argumentsJson) { return R"("/tmp/a.png")"; }); // {"call": "pickFile"}
screen.allowImageDirectory(photos);                    // image files from there too, not only the screen's folder
screen.setImage("chart", width, height, rgbaPixels);    // {"type": "image", "source": "chart"}; again to change it
affresco::Window& window = screen.createWindow(app); // also connects the file's channels
screen.listen(app, "/tmp/affrescoui-501/agent.sock", "agent"); // peers can change it from now on
app.run();
```

The document decides how a built panel looks. An update applies a node again only when something it shows changed:
its values, the theme, the colours, the fonts, or a new version of the document. So a change code makes to a built
panel directly (`screen.find("logo")->setColor(...)`) stays until one of those changes, then the document's values
replace it. To keep a change, make it in the document (state, or a revision), or in a registered type's apply function.

See the templates in `examples/assets/screens/`: `kanban` (the one `affrescoui_host` opens: rows, components, drag and
drop, a popover), `smart-home` (themes, computed values, switches and sliders, animations), `music-player` (a playback
timer, seeking), `code-editor` (an application type built on `editor`, tabs, a terminal, a Cmd+P palette), `habit-tracker` (validation-free forms, `watch`,
`persist`), `sign-in` (screens, navigation, `check`), and `studio` (the Studio client: its whole UI, behaviour and
connection).
