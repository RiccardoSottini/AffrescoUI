# AffrescoUI Screen Format

A JSON format for describing a user interface that AffrescoUI builds and shows at runtime: **what is on screen**, plus the
app's **state, bindings and behaviour**, so a whole app can live in the file. Application code can add more (native
functions, custom types), and the catalog tells agents what it added.

Screens are **live**: the UI follows the state, and **revisions** (compact edits or JSON Patch) can change the document,
layout and behaviour while the app runs. A revision can come from the app itself or from a program connected over a
channel, for example an AI agent through the MCP server.

**Current version: 2.** Every file declares `"version"`. The schema keeps each version's definition separately
(`$defs/v2`; `$defs/v1` only says to migrate) and the loader checks the version first. A file of version 1 still
loads: it is migrated in memory, with one notice ("Migrating from version 1").

- File extension: `.screen.json`. JSON Schema (editor completion and validation):
  [`schema/affrescoui-screen.schema.json`](schema/affrescoui-screen.schema.json), referenced with `"$schema"`.
- Try any file without writing code: `./build/affrescoui_launcher path/to/file.screen.json`

```json
{
	"$schema": "../schema/affrescoui-screen.schema.json",
	"format": "affrescoui-screen",
	"version": 2,
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
| `version` | Required. `2` (a file of version 1 is read by migrating it: "Migrating from version 1"). |
| `root` | Required. The window's root node. It always fills the window, so layout properties aren't allowed on it. |
| `format` | `"affrescoui-screen"`, for tools. |
| `note` | A comment. |
| `window` | `title`, `width`, `height`, `minWidth`, `minHeight` (integers); `menuBar`: the native menu bar, a list of `{ "menu": "file", "title": "File" }` naming menus of the `menus` section (macOS, Windows; elsewhere none, and `window.nativeMenuBar` is false so the screen can draw its own); `focusRing`: the ring around the node the keyboard focused: a colour, `{ "color": "#2563EB", "width": 2, "offset": 2 }` (the defaults), or `false` (none). |
| `fonts` | `name → { "file", "size", "fallback", "bold", "italic" }`, paths relative to the screen file (TrueType or OpenType, a collection's first font). `default` is used by text without a `font`. Text is UTF-8, shaped per script (kerning, ligatures, Arabic, Indic; right-to-left runs), colour emoji from COLR fonts. `"fallback": ["symbols"]` names other fonts asked in order for a character this one lacks; a character none has shows as `?` (macOS menu key labels use ⌘ ⇧ ⌥ ⌃, which JetBrains Mono has). `"bold": true` and `"italic": true` thicken or slant the outlines, for headings from a file with one weight: `"heading": { "file": "Roboto-Regular.ttf", "size": 20, "bold": true }`; a label's `runs` keep its font's style unless they set their own. |
| `languages` | `name → language`: the code languages an editor's `language` names (by name, alias or file name). Each has `title`, `aliases`, `files` (`["*.py", "CMakeLists.txt"]`), `comments` (`{ "line": "//", "block": ["/*", "*/"] }`), `brackets` and `pairs` (`[["(", ")"], ...]`: Enter between brackets indents, typing an opener of a pair adds its closer), `indent` (`{ "increase": regex, "decrease": regex }`), `wordPattern` (regex), and TextMate-style `patterns` with a `repository`: `{ "match": regex, "scope": "keyword" }`, `{ "begin": regex, "end": regex, "scope", "contentScope", "patterns" }` (an `end` may use the begin's groups, `\1`), `captures` / `beginCaptures` / `endCaptures` (`{ "1": { "scope": "type" } }`) and `{ "include": "#name" }` or `"$self"`. A scope is coloured by `@syntax.<scope>`, or its parent's (`keyword.control` by `@syntax.keyword`). Regular expressions are PCRE2, each find limited in steps and each line in time; a language has at most 2,048 rules. Ready-made languages are data files to `include` (`examples/shared/languages/`). |
| `colors` | `name → "#rrggbb"`, or per theme `name → { "light": "#fff", "dark": "#111827" }`. Used as `"@name"`. |
| `tokens` | `name → value`: a number (spacing, a radius, a type size), a colour (`"#rrggbb"`, `"transparent"`), an alias (`"@other"`, of a token or of `colors`), or per theme `{ "light": ..., "dark": ... }` of those. Used as `"@name"` wherever a number or a colour fits (`"padding": "@space.m"`, a font's `"size"`). Switched by `theme` like `colors`; a name in both is an error. Aliases go at most 16 deep. |
| `theme` | Which variant of the colors and tokens is shown, usually bound: `"{{state.theme}}"`. Without a match, the first variant. |
| `strings` | `locale → { key → message }`, with `"default"` (the locale a key falls back to) and `"locale"` (the locale shown, usually bound: `"{{state.locale}}"`; else the host's, from the system). `t("key", {params})` looks a key up in the locale, then its parents (`fr-CA`, then `fr`), then the default; a key found nowhere shows as itself. See Strings and locales. |
| `styles` | `name → properties`, applied with `"style": "name"` or `["a", "b"]`. A style may itself have a `style`. |
| `components` | `name → { "params": {defaults}, "node": node }`: reusable nodes with parameters (see Components). |
| `screens` | `name → node`: the screens an `outlet` shows; `{"navigate": "name"}` switches. The first is shown first. |
| `templates` | `name → node`, built by code on demand (`Screen::instantiate`). |
| `overlays` | `name → node`: laid over the window, above the root and its own overlays, e.g. from an `include`d add-on. Each is an `overlay` placed in the window by its own anchors unless it says otherwise; it may `use` a component, and isn't built while its `when` doesn't hold. |
| `state` | The app's data, any JSON object. |
| `assets` | `name → {type, size, sha256}`: files revisions brought, kept with the history, read as `"asset:<name>"` (see Assets). |
| `stateTypes` | `key → type`: what each state key holds, written as JSON Schema (see Typed state). |
| `models` | `collection → { "version", "fields", "seed", "migrations" }`: the records' models (see Data). |
| `queries` | `name → { "from", "where", "sort", "limit" }`: records read as `data.<name>`. |
| `computed` | `name → expression`: values derived from the state, read as `computed.name` (see Values). |
| `watch` | `"state.path" → handler`: runs after an update in which that value changed (`event.value`, `event.previous`). |
| `actions` | `name → steps`, run by handlers, `run` steps, shortcuts and channels; or a command `{ "title", "category", "icon", "enablement", "description", "do" }`, listed by the `commands` variable (see Commands and keybindings). |
| `keys` | Screen-wide shortcuts: `"mod+k" → handler` (see Events). |
| `menus` | `menu → { key → { "run" or "submenu", "group", "order", "when", "title", "icon" } }`, listed by the `menus` variable (see Menus and settings). |
| `settings` | `"editor.fontSize" → { "type", "default", "title", "description", "minimum", ... }`: the user's settings, stored in `state.settings` (see Menus and settings). |
| `onLoad` | A handler run once after the first update. |
| `include` | `["parts/sidebar.json", ...]`: files merged in at load, section by section (this file's entries win; `menus` menu by menu, so several files add to one menu; `persist` lists join). |
<!-- end generated -->

### Capabilities and features

The sections the host grants and revisions can't widen, and the installed features.

<!-- generated: capabilities -->
| Key | Meaning |
|---|---|
| `channels` | Programs the screen talks to (see Channels). A capability: revisions can't change it. |
| `services` | `{"files": {"provider": "fs", "root": "workspace"}}`: services that `invoke` and `subscribe` steps talk to, read by the application's providers (`jsonrpc`, `process`, `fs`); their status is the `services` variable. A capability: revisions can't change it. |
| `connections` | `{"api": {"url": "https://api.example.com/v1", "operations": {"getProject": {"path": "/projects/{id}"}}}}`: servers that `request` and `subscribe` steps talk to, each request built from its declared operation (method, `path` with `{name}` segments from `with`, `query`, `body`, `headers`, `effect`, `stream`, `result`, `timeout`, `mock`); a `ws://`/`wss://` connection has `messages` and runs the actions `name:open`, `name:<type>`, `name:error`, `name:closed`. The host allows the origins (`network.allow`). `connections.<name>` is `{online, pending, state, error}`. `auth` signs its requests in: an object declares an OAuth 2 provider (`flow` `browser` or `device`, `signInUrl` or `deviceUrl`, `tokenUrl`, `clientId`, `scopes`), a string names another connection's; the `signIn` step signs in, the host adds the token, and `auth.<name>` is `{state, account, expires, error}`, never a token. A capability: revisions use its operations, never add them. |
| `persist` | `["state.habits", ...]`: state paths kept on disk across restarts. A capability: revisions can't change it. |
| `permissions` | Who may change the running screen (see [Version control](VERSION_CONTROL.md)). Revisions can't change it. |
| `features` | `{"wordCount": {"release": "1.0.0", "enabled": true, "order": 10, "requires": {...}, "provides": {...}}}`: installed features, each kept whole in its record (with `title`, `source`, `digest` and `publisher`). The runtime merges the enabled ones in `order`, under their names (`state.wordCount`, `wordCount.toggle`); views join the nodes whose `point` they name. Install, enable, disable and remove are revisions on `/features/<name>`. A feature can't bring a capability or a control step. |
| `exports` | `{"editor.text": {"type": "string", "value": "{{state.text}}"}}`: values features read as `app.editor.text`. |
| `imports` | `{"ui": "workbench@^1.0.0"}`: `"use": "ui.panel"` builds the feature's `workbench.panel`, if its release is in the range. |
| `images` | `{"canvas": {"width": 800, "height": 600, "into": "state.canvas", "tools": {"brush": {"gesture": "paint", "size": 12}}}}`: the images the `image` step edits; `tools` names the library's gestures with their settings, and `limits` only lowers the library's (see Images). Revisions can't change it. |
| `audio` | `{"song": {"file": "sounds/song.mp3", "into": "state.song", "loop": true}}`: the sounds the `audio` step plays, each from one source: a `file` (WAV, FLAC or MP3) in the screen's folder or one the host allows, a `url` the host's network allows (streamed), a `source` the host names (`Screen::setAudioSource`), or a `tone` (`frequency`, `seconds`, `amplitude`); with `volume`, `pan`, `rate`, `loop`, `updates` (ms between position updates) and a URL's `maxBytes`. `into` gets `{status, playing, position, duration, progress, volume, pan, rate, loop, level, underruns, error}`; the actions `name:started`, `name:ended`, `name:underrun` and `name:error` run (see Audio). A `file` may be `asset:<name>`, also a sound description. A revision may give a sound a `file` or a `tone` (never a `url` or a `source`), and only the person accepts it (Assets). |
| `points` | `{"panels.dock": {"slot": "dock", "receives": ["doc.layers", "layers.move"]}}`: where a feature from another app may put its views (the node whose `point` has the name) and the exports it receives; the action exports a point receives are the host actions such features may run. Revisions can't change it. |
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
| `type` | `panel` (default), `label`, `textarea`, `editor`, `diff`, `stack`, `row`, `column`, `grid`, `toggle`, `checkbox`, `slider`, `image`, `surface`, `raster`, `split`, `tabs`, `list`, `tree`, `table`, `path`, `colorPicker`, `gradient`, `curves`, `canvas`, `graph`, `timeline`, `calendar`, `codeInput`, `carousel`, `scene`, `outlet`, or a type the application registers. |
| `id` | A name to address the node (`#id` in edits, `nodes.id`, anchors, `focus`). Every node should have one, and no two nodes the same (the checker reports a duplicate). `affrescoui ids` gives readable ones to a file's nodes (`label-blocked`, `panel-top-bar`), and a node a revision adds without one gets one in the commit. A `use` keeps its own id over its component's. |
| `style` | Style name or list of names; may be computed: `"{{on ? 'chipOn' : 'chip'}}"`. |
| `children` | Array of nodes. |
| `x`, `y` | Position inside the parent (used when the matching edge isn't anchored). |
| `width`, `height`, `minWidth`, `minHeight` | Size. The minimum wins over a smaller size. |
| `maxWidth`, `maxHeight` | The largest size (a length, or `"none"`, the default). The minimum wins over the maximum, as in CSS. |
| `aspectRatio` | Width divided by height, `1.5` or `"16/9"`: the height follows the width, unless only the height is set (a non-zero `height`, opposite anchors, or a container's stretch), then the width follows the height. With both set, it is ignored, as in CSS. |
| `anchors` | `{ "top", "bottom", "left", "right" }`: keep that edge this far from the parent's same edge. Anchoring opposite edges stretches the node. A value `"#id.edge + 8"` puts the edge at another node's edge (`"top": "#header.bottom + 12"`, `"bottom": "#footer.top - 8"`). The same as text: edge names, each at 0 or at a length after a colon, `"left right:12 top:8"` is `{ "left": 0, "right": 12, "top": 8 }`. |
| `fill` | Anchor all four edges this far in (`0` or `true` to cover the parent). |
| `grow` | In a `row`/`column`: the share of the spare room it takes, like CSS flex-grow. |
| `shrink` | In a `row`/`column`: how much of an overflow it gives up, in proportion to `shrink` times its size, down to its minimum (CSS flex-shrink). Default `0`: it keeps its size and the children overflow, as they always have (CSS's default is 1). |
| `basis` | In a `row`/`column`: its size along it before growing or shrinking (CSS flex-basis), a length; `"auto"` (the default) for its own `width` or `height`. |
| `alignSelf` | In a `row`/`column`/`grid`: where it sits across (`start`, `center`, `end`, `stretch`), over the container's `alignItems`. |
| `justifySelf` | In a `grid`: where it sits along its cell's row (`start`, `center`, `end`, `stretch`), over the grid's `justifyItems`. |
| `cell` | In a `grid`: `{ "column": 2, "row": 1, "columnSpan": 2, "rowSpan": 1 }`, counted from 1 (each key optional). Without `column` and `row`, it takes the next free cell, row after row. |
| `dir` | Which way its children and text run, inherited: `"ltr"`, `"rtl"`, `"auto"` (the locale's: Arabic and Hebrew are right to left) or `"inherit"` (the default; the root is left to right). Right to left mirrors row order, grid columns, left and right anchors, `x`, padding and scrolling, swaps a label's `align` start and end, and draws an icon marked `mirror` mirrored. |
<!-- end generated -->

### Every type: look
<!-- generated: look -->
| Key | Meaning |
|---|---|
| `color` | Background. Default: light grey for panels, transparent for labels, stacks and controls. |
| `radius` | Rounds the corners, in pixels: one number for all four (`8`), or `{ "topLeft", "topRight", "bottomRight", "bottomLeft" }` (corners left out are square). A radius larger than half the shorter side is clamped to it, so `999` makes a pill or a circle. Edges are anti-aliased. Children are still clipped to the square bounds, and the corners still take clicks. Default: `0`. |
| `borderWidth`, `borderColor` | A border drawn inside the node's edges (it doesn't change the layout), over the background and under the children, following `radius`. Default: no border; `borderColor` defaults to black. |
| `shadow` | `{ "x": 0, "y": 4, "blur": 12, "color": "#00000040" }`, like CSS box-shadow: the node's shape, moved by `x` and `y` and blurred by `blur` (a Gaussian with a standard deviation of `blur / 2`), drawn under the node. Every key is optional; `color` defaults to `#00000040`. A shadow can fall outside the node, but it is clipped by the parent, like the node itself. |
| `gradient` | The background as a gradient, painted over `color` within the corners, like CSS `linear-gradient()` and `radial-gradient()`: `["#1e3a5f", "#0b1220"]` (colours spread evenly from top to bottom), or `{ "kind": "linear", "angle": 90, "stops": ["@accent", { "at": 0.6, "color": "#00000000" }] }`. `angle` is in degrees (0 up, 90 right, 180 down, the default); the line reaches the corners, as in CSS. A `radial` gradient spreads from the centre to the farthest corner. A stop is a colour, spread evenly, or `{ "at": 0..1, "color" }`; two stops at one place change colour sharply. At most 32 stops. It can be set in `hover` and the other states. |
| `visible` | `false` hides the node and its children. |
| `cache` | `true` keeps the node and its children drawn in a texture, drawn as one quad each frame until something in them changes. Worth it for a large part of a screen that rarely changes (a sidebar, a chart); it looks the same (a pixel of its own translucent edge, such as a rounded corner, may be one level of 255 off). Default: `false`. |
| `overflow` | `"visible"` lets the children draw and be hit outside the node, clipped to what clips the node instead, as CSS `overflow: visible`: a line whose arrowhead passes its box, text longer than its frame. A cached or scrolling node still clips. Default: `"clip"`. |
| `snapshot` | `"thumb"` or `{ "image": "thumb", "scale": 0.25 }`: the node and its children are drawn into the image of that name instead of on the screen, again each time something in them changes, and an `image` node with `"source": "thumb"` shows it (a thumbnail, a preview, a slide sorter). Out of the layout: laid out at its own `width` and `height` from the window's top left, and never hit, focused or read. `scale` is the image's pixels per layout pixel (above 0, at most 4; default `1`). Overlays inside it are left out. Not on the root or with `overlay`; an empty name draws the node in place. |
| `rotate` | Draws the node and its children turned, in degrees clockwise, about `transformOrigin`: `"rotate": 8`. Like every transform key it only changes how the node is drawn and hit: its place in the layout, and its siblings', stay. Bind it to a number in the state and an `animate` step turns it. Default: `0`. |
| `scale` | Draws the node scaled about `transformOrigin`: one factor (`0.9`) or `[x, y]`. Default: `1`. |
| `skew` | Draws the node skewed, `[x, y]` in degrees, as CSS `skew()`: `[-12, 0]` slants it. Default: `[0, 0]`. |
| `translate` | Draws the node moved by `[x, y]` pixels after layout. The keys compose as CSS does: `translate`, `rotate`, `scale`, then `skew`. Default: `[0, 0]`. |
| `transformOrigin` | The point `rotate`, `scale` and `skew` turn about: `"center"` (the default), an edge or a corner (`"top"`, `"bottomRight"`), or `[x%, y%]` of the node's size (`[0, 100]` is the bottom-left corner). |
| `cursor` | `arrow`, `ibeam`, `hand`, `resize-horizontal`, `resize-vertical`. |
| `tooltip` | Text shown after the pointer rests on the node. |
| `disabled` | `true`: no events (hover styling still applies). |
| `scroll` | `"vertical"`, `"horizontal"`, `"both"` or `"none"`: the wheel or trackpad scrolls the children; a thin bar shows where. |
| `overlay` | `{ "anchor": "#id", "placement": "below", "open": "state.menuOpen", "dismiss": "outside" }`: drawn above everything, unclipped, next to its anchor (default: its parent). Placements: `below`, `below-end`, `above`, `above-end`, `right`, `left`, `center` (in the window), `window` (placed by its own x, y, anchors in the window). `open` is a state path (or a value); with `"dismiss": "outside"`, a click outside it or Escape sets it to false. `"modal": true` traps the focus inside while it is open (Tab cycles in it, the first focusable node inside gets the focus unless one inside already has it) and gives it back when it closes. `"flip": true` places it on the other side when only that side has room, its children mirrored on that axis. |
| `hover`, `focused`, `dragging`, `dragOver`, `pressed` | Properties shown in that state (later ones win: pressed over drag over over dragging over focused over hover). Any visual property: `{ "color", "textColor", "borderColor", "shadow", "style", ... }`. A node counts as hovered and pressed while one of its children is. |
| `transition` | `{ "color": 150, "textColor": 150, "borderColor": 150 }`: changes of these colours animate over that many milliseconds. |
<!-- end generated -->

### Every type: focus and accessibility
<!-- generated: access -->
| Key | Meaning |
|---|---|
| `focusable` | `true`: Tab reaches the node, and Enter or Space on it runs its `onClick` (text inputs are always focusable). |
| `role` | What the node is to assistive tools and to scenarios (ARIA's names: `button`, `checkbox`, `textbox`, `dialog`, `list`, `listitem`, `menu`, `menuitem`, `tab`, ...). Default: `textbox` for inputs and editors, `text` for labels, `checkbox` for toggles, `slider` for sliders, `img` for images, `button` for a focusable node with an `onClick`; other nodes have none and are skipped in the accessibility tree. |
| `label` | The node's accessible name. Default: its text, else the texts of the labels inside it, else its `tooltip` (an input's or a slider's text is its value, not its name). |
| `description` | A longer accessible description, read after the name. |
| `isOpen` | Whether the node is expanded (a tree item, a disclosure, a menu button: `expanded` to assistive tools); leave it out for one that doesn't expand. Screen readers say it, and offer to expand or collapse it (a click). |
| `selected` | Whether the node is selected (a tab, a list item, a menu's check); leave it out, or bind it to `null`, for one that can't be. |
| `live` | A live region: changes to its text are read without moving the focus, `polite` when the person is idle (a toast), `assertive` at once (an error). Default: `off`. |
| `tabIndex` | The tab order: positive ones first, in increasing order, then the others in tree order; `-1` leaves it out. |
| `arrows` | The arrow keys, Home and End move the focus between the focusable nodes inside (a list, a menu, a toolbar), wrapping; Tab moves past them in one step. A letter key clicks the node inside whose `mnemonic` it is. |
| `mnemonic` | A letter: Alt with it clicks the node (the first one, in the open modal overlay if any); inside an `arrows` node, the letter alone does. |
<!-- end generated -->

The window hands these to the platform's accessibility API (VoiceOver, Narrator, Orca; see `ACCESSIBILITY.md`): every
node with a role becomes an element with its name, description, value (a slider's number and range, a toggle's
checked state, a text field's text, caret and selection), states (`isOpen`, `selected`, `disabled`) and bounds in
screen points. Assistive tools can press a node (its `onClick`), step a slider, expand or collapse an `isOpen` node,
and move the focus; the action runs on the UI thread in the next frame. Changes go out after the frame that shows them,
only for the elements that changed.

### Every type: behaviour
<!-- generated: behaviour -->
| Key | Meaning |
|---|---|
| `repeat` | `{ "for": "card", "in": "state.cards", "index": "i", "key": "{{card.id}}" }`: one node per list item. With `key`, a node follows its item when the list is reordered (keeping focus and scroll). |
| `when` | Build the node only if the value is truthy. |
| `let` | `{ "on": "{{tag == state.filter}}" }`: named values for the node and its children, evaluated in order. |
| `use`, `with` | Build a component: `{ "use": "chip", "with": { "label": "{{tag}}" } }` (see Components). |
| `point` | `"point": "statusBar.right"`: features' views for that point become the node's last children. |
| `check` | `{ "rule": "{{length(value) >= 3}}", "message": "Too short", "value": ... }`: `value` defaults to the node's bound value; gives the node and its children `valid` and `error`. |
| `every` | `{ "ms": 1000, "do": steps }`: runs while the node exists. |
| `drag` | `{ "data": {...} }`: the node can be dragged; a node with `onDrop` receives `event.data`. |
| `context` | `{ "editorFocus": true }`: keys added to the `context` variable while the node or a child has the focus (an inner node wins); `when` clauses read them. |
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
| `align` | One value for both axes, two for horizontal then vertical (`"start center"`), or `{ "horizontal", "vertical" }`. |
| `wrap` | Break lines that don't fit the width. |
| `fitText` | `true` or `{ "maxWidth" }`: the label sizes itself to its text (plus padding). |
| `icon` | An icon before the text, by name: `"search"`, `"arrow-left"`. A glyph of that name in the label's font or its fallbacks, drawn in `textColor` with a space before the text (alone if there is no text). A name no font has shows nothing. Give the text font an icon font as a fallback (`"fallback": ["icons"]`) and icons are drawn at the text's size, in the same batch; or give an icon-only label the icon font itself, at its own size. The examples ship Lucide (`examples/shared/fonts/Lucide.ttf`, 1,890 icons: https://lucide.dev/icons). Not on text areas. |
| `mirror` | Right to left (`dir`), draw the icon mirrored: `left` and `right` swap in its name (`"chevron-left"` draws `"chevron-right"`), when the font has that one. For arrows and chevrons that point the reading way. |
| `runs` | Rich text instead of `text`: a list of parts, each text or `{ "text", "bold", "italic", "size", "font", "color", "link", "icon" }`. `font` names a font of `fonts` (default: the label's); `bold`, `italic` and `size` (4 to 256 pixels) make a variant of it (at most 32 per document). A part with `link` is underlined and a press on it runs `onLink`; `icon` draws a named glyph inline, as `icon` does. Lines wrap at spaces across parts. At most 256 parts and 65,536 characters. Not on text areas. |
| `selectable` | The user can select the text with the mouse and copy it with Cmd/Ctrl+C (Cmd/Ctrl+A selects it all); icons are left out of the copy. |
| `selectionColor` | The selection's colour (default `#b4d5fe`). |
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
`examples/shared/CodeEditor.cpp`, the Forge template's `code` type.
Forge's `code` type adds keys for features at the caret, such as a language server's (`features/lsp/`):
`onHover` (event `{ over, line, column, x, y, top }` when the code point under the resting mouse changes), `onType`
(event `{ text, line, column, word, x, y, top }` after each typed character or deletion), `popup` and `onPopupKey`
(while `popup` is true, Up, Down, Page Up/Down, Enter, Tab and Escape go to the action as `{ key }`), `insert`
(`{ text, before, request }`: replaces the `before` characters before the caret, once per request), and `tokens`
(`[{ line, column, length, style }]`, from 0, coloured by `@syntax.<style>` over the grammar's colours).

<!-- generated: editor -->
| Key | Meaning |
|---|---|
| `bind` | A state path holding the text (lines joined by `"\n"`), kept in sync both ways. Without it, `text` sets the text. |
| `caret` | A state path the editor writes its caret to: `{ "line", "column", "selected" }`, counted from 1. |
| `font`, `textColor` | Font and text colour. |
| `lineNumbers` | `true` shows line numbers in a gutter. |
| `lineNumberColor`, `activeLineNumberColor` | Line numbers, and the caret's line number. |
| `lineColor`, `selectionColor`, `caretColor` | The caret's line background, the selection, the caret. |
| `autofocus`, `onChange` | Focus when built, and when a modal overlay holding it opens; `event.value` is the new text. |
| `wrap` | `true` wraps lines at the width (at spaces, else anywhere); the gutter numbers each line once. |
| `folds` | Folded ranges, `[{ "line", "endLine" }]` from 0: the lines after `line` up to `endLine` are hidden behind it. The user folds and unfolds with the gutter or Cmd+Alt+[ and ]; the document's folds replace theirs only when they change, and leaving the key out keeps them. At most 10,000. |
| `decorations` | `[{ "line", "endLine", "column", "endColumn", "kind", "color", "icon", "text" }]`, lines and columns from 0: `background` tints the lines, `squiggle` underlines the columns (or the whole lines) with a wave, `gutter` draws `icon` (its first character) beside the line, `inline` writes `text` after the line's end. At most 10,000 and 8 on a line; inline text is cut at 256 characters. |
| `find` | `{ "text", "regex", "wholeWord", "caseSensitive" }` or `null` (none): highlights the matches of `text` (plain, or a PCRE2 pattern with `regex`, matched line by line within match and depth limits); Cmd+G and Cmd+Shift+G select the next and previous. A pattern that doesn't compile highlights nothing. `inSelection` limits it to the selection as it is when turned on. A find bar asks for a step with `step` (`first` from the selection's start, `next`, `previous`, `replace` or `replaceAll`, with `replace` as the replacement) and a `request` number from 1, run once per new number. While a pattern is set, `caret` also gets `matches`, the count. |
| `brackets` | `true` highlights the bracket that pairs with the one at the caret. |
| `bracketStyle` | How the paired brackets are shown, in `bracketColor`: `fill` (the default) behind them, or `underline`. |
| `indentGuides` | `true` draws a line at each indentation level. |
| `rulers` | Columns, counted from 0 in spaces, to draw a thin vertical line at, in `guideColor` (e.g. `[80]`). |
| `minimap` | `true` shows a minimap at the right: an overview of the rows, with the visible ones marked; pressing or dragging it scrolls. |
| `minimapStyle` | How the minimap draws a row: `bars` (the default), one bar from its first to its last character, or `text`, each character a pixel in its syntax colour. |
| `minimapViewport` | When the minimap marks the visible rows: `always` (the default), or `hover`, while the pointer is over it. |
| `matchStyle` | How find's matches are shown, in `matchColor`: `fill` (the default) behind them, or `outline`. |
| `composition` | Text an input method is composing, shown underlined at the caret without editing (empty: none). |
| `readOnly` | `true`: the user can select, find and fold, but not edit. |
| `guideColor`, `matchColor`, `bracketColor` | The indentation guides, find's matches and the paired brackets. |
| `grammar` | The language parsed for syntax colours, folds and bracket pairs: `c`, `cpp`, `json`, `python`, `javascript`, `typescript`, `markdown` or an alias (`c++`, `js`, `ts`, `py`, `md`), or a file name whose extension says it. Each capture is coloured by the theme's `@syntax.<capture>` token (e.g. `@syntax.keyword`; `keyword.return` falls back to `@syntax.keyword`). Left out, plain text. |
| `commands` | Keys bound to code commands, each one undo step: `{ "mod+/": "toggleComment", "alt+up": "moveLinesUp" }` (`mod` is cmd on macOS, ctrl elsewhere). `toggleComment` (the language's line comment), `moveLinesUp`, `moveLinesDown`, `deleteLines`, `copyLine` and `cutLine` (the caret's line, when nothing is selected), `indentLines`, `outdentLines`, `insertLineAfter`, `insertLineBefore`. A bound key isn't typed. |
| `popup` | `true` while the document shows a popup over the editor (completions): up, down, page up, page down, enter, tab and escape without modifiers go to `onPopupKey` instead of the text. |
| `goTo` | `{ "line", "column", "request" }` (from 0): moves the caret there, once per `request` number (a new number moves it again). |
| `insert` | `{ "text", "before", "request" }`: replaces the selection and the `before` characters just before it with `text` (a completion), once per `request` number; one undo step. |
| `tokens` | `[{ "line", "column", "length", "style" }]` (from 0): runs coloured by `@syntax.<style>` over the colours of `language` or `grammar`, e.g. a language server's semantic tokens. They move with edits until the next list. |
<!-- end generated -->

### `diff`
Two texts compared line by line (Myers' algorithm, within a cost limit: past it, a longer but still correct script),
then character by character on the lines changed, in read-only editors that number each text's lines. `font` and
`textColor` are the editor's.

<!-- generated: diff -->
| Key | Meaning |
|---|---|
| `original` | The text compared from (lines split at `\n`, a `\r` before it dropped). |
| `modified` | The text compared to. |
| `mode` | `split` (default): the original on the left, the modified text on the right, rows aligned with blank rows and scrolled together; `inline`: one editor, the deleted lines above the lines inserted for them. |
| `language` | A language id given to the application's editor factory (syntax colours); left out, or without one, plain text. On an `editor`: a language of the `languages` section, by name, alias or file name (`main.py`). It colours the text unless `grammar` does, and its rules apply to typing: an opener of `pairs` adds its closer (typed over, or wrapping the selection), Backspace between an empty pair deletes both, Enter between `brackets` or after an `indent.increase` line indents a level, and a line that comes to match `indent.decrease` goes back one. Unknown, plain text. |
| `deletedColor`, `insertedColor` | Behind deleted and inserted lines (the changed characters in the same colour, twice as strong). |
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
| `alignItems` | Where the children sit across: `start`, `center`, `end` or `stretch` (as tall as a row, as wide as a column, within their maximum). Left out, they keep their own anchors across. |
| `fitContent` | `true`, `"width"` or `"height"`: that size comes from the children instead of `width` and `height` (within the minimum and maximum), as a CSS `fit-content` box: along the direction their sizes and the gaps (one line, even when it wraps), across it the largest. A toolbar of buttons, a tab with its underline, a card that grows with its text. A child that grows or is stretched by its anchors adds only its own size. |
| `direction` | On a `stack`: `"row"` or `"column"`. |
| `wrap` | Continue on a new line (in a column, a new column) when a child doesn't fit; each line grows, shrinks and is spread on its own, and sits at the start of its line unless aligned. |
<!-- end generated -->

**Direction.** A node's `dir` (`"ltr"`, `"rtl"`, `"auto"` for the locale's, or `"inherit"`, the default) says which
way its children and text run. Right to left, a `row` places its first child at the right, a `grid` its first column
at the right, and `left` and `right` anchors, `x`, padding and scrolling are mirrored; a label's `align` `start` and
`end` swap, and an icon marked `mirror` is drawn mirrored. Text is shaped by its own direction either way (Arabic and
Hebrew runs right to left, numbers left to right). `"dir": "auto"` on the root makes a whole screen follow `locale`.

`grid` places its children in columns and rows, like a CSS grid: each in its `cell`, or in the next free cell, row
after row. A child is as wide as its cell and keeps its own height at the top of it, unless `justifyItems`,
`alignItems`, or its own `justifySelf` and `alignSelf` say otherwise.

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

### `colorPicker`, `gradient`, `curves`
Creative controls, bound like `slider`: `bind` keeps the value in a state path; without it, `value` shows one and
`onChange` (`event.value`) reacts. They keep their own clicks, work from the keyboard alone (with `"focusable": true`)
and draw from cached bitmaps.

`colorPicker`: a saturation-value square and a hue strip, or a hue wheel and a value strip (`mode`), an alpha strip,
fields for hex, RGB, HSL, HSV and OKLCH, swatches and the last colours picked. The arrows move the marker of the part
the keyboard is on, Tab goes to the next part, typing in a field and Enter applies it. Pressing on the preview and
letting go elsewhere in the window picks the colour there (an eyedropper limited to the window).

<!-- generated: colorPicker -->
| Key | Meaning |
|---|---|
| `value` | The colour, `"#rrggbbaa"` (`bind` keeps it in a state path); picking one runs `onChange` with it. |
| `mode` | `"square"` (the default: saturation across, value down, a hue strip) or `"wheel"` (hue around, saturation out, a value strip). |
| `alpha` | Whether the alpha strip shows (default true); hidden, the colour is opaque. |
| `fields` | Whether the typed fields show (default true): hex, RGB, HSL, HSV and OKLCH. |
| `swatches` | Colours to pick from: hex colours or design tokens (`"@accent"`), at most 32. The last 8 colours picked follow them. |
| `textColor`, `accentColor` | The fields' text and the accent of the part the keyboard is on. |
<!-- end generated -->

`gradient`: colour stops on a bar over a preview. Click the bar to add a stop, drag one to move it (off the bar to
remove it), click one to edit its colour in a picker. Keys: Left and Right select a stop, Shift with the arrows moves
it, Insert or `=` adds one, Delete removes it, Enter edits its colour (Escape closes), K switches linear and radial,
`[` and `]` turn the angle.

<!-- generated: gradient -->
| Key | Meaning |
|---|---|
| `value` | `{ "kind": "linear" or "radial", "angle": 90, "stops": [{ "at": 0, "color": "#000000ff" }] }`: the angle in degrees (0 up, 90 right, as in CSS), the stops' positions 0 to 1, at most 32 stops. `bind` keeps it in a state path; a change runs `onChange` with it. `gradientColor(value, at)` gives its colour at a position. |
| `accentColor` | The selected stop's outline, and the accent of the colour popover. |
<!-- end generated -->

`curves`: control points on a 0 to 1 square, over an optional histogram. Click to add a point, drag one to move it
(out of the square to remove it). Keys: Left and Right select a point, Shift+Left and Shift+Right move it across, Up
and Down move it (Shift for larger steps), Insert or `=` adds one, Delete removes it, C switches the channel.

<!-- generated: curves -->
| Key | Meaning |
|---|---|
| `value` | `{ "points": [[0, 0], [1, 1]] }`: the control points, 0 to 1, at most 32; with `channels`, also `"red"`, `"green"` and `"blue"` lists. `bind` keeps it in a state path; a change runs `onChange` with it. `curve(value, x)` gives the curve's output at `x`. |
| `interpolation` | `"monotone"` (the default: smooth through the points, never overshooting), `"linear"` or `"bezier"` (the points as a Bézier curve's control polygon). |
| `channels` | Whether the RGB, R, G and B tabs show (default false: the RGB curve only). |
| `histogram` | Counts from dark to light drawn behind the curve, e.g. `"{{state.histogram}}"`; at most 1,024. |
| `accentColor` | The selected point and tab. |
<!-- end generated -->

### `image`
A PNG or JPEG picture, fitted to the node. Transparent until it arrives: files are decoded on a worker thread, and the
picture appears when it is ready (a picture of the screen, `render`, waits for every one).

<!-- generated: image -->
| Key | Meaning |
|---|---|
| `file` | A PNG, JPEG or SVG file, relative to the screen file. It must be inside the screen file's folder (or the folders inside it), or a folder the host allows (`Screen::allowImageDirectory`): a revision naming any other file is refused. At most 64 MiB, 8,192 pixels a side and 4,096 × 4,096 pixels in all. A file that can't be read or decoded shows nothing (and is logged). `"asset:<name>"` shows an asset of the screen instead (Assets), which only the person accepts. |
| `source` | Instead of `file`: a named image, which a node draws (its `snapshot`: a thumbnail of it) or the host sets (`Screen::setImage("chart", …)`, e.g. a picture it made); the node shows each new one. |
| `fit` | `"contain"` (the default: all of it, as large as fits, centred), `"cover"` (the whole node, the image's edges cut off) or `"fill"` (stretched), like CSS `object-fit`. |
<!-- end generated -->

```json
{ "type": "image", "id": "avatar", "file": "images/avatar.png", "width": 48, "height": 48, "fit": "cover" }
```

Small images (up to 256 pixels a side) share textures, so a list of thumbnails draws in one batch with its text.

### `surface`
Native GPU drawing the host registered with `Screen::registerSurface`: a shader, a 3D view, a video. The host's code
draws into a framebuffer of the node's size in pixels, with its own OpenGL state, and the node shows the result like an
image: clipped, transformed and redrawn only when it changes. The pointer's moves, presses and scrolls on the node reach
the surface in its own pixels. A surface that raises an OpenGL error is disabled (and logged once); the rest of the
window draws as before.

<!-- generated: surface -->
| Key | Meaning |
|---|---|
| `surface` | The name of a GPU surface the host registered (`Screen::registerSurface("cube", …)`). A document can only name one: it never supplies shader source. A name that isn't registered is an error at the node. |
| `params` | An object the surface reads each time it draws, e.g. `{ "speed": 2 }`: bind its values to the state and it draws again when they change. |
| `continuous` | `true` draws it every frame (an animation), with the time since the screen started. Default: `false`, drawn only when `params`, its size or its input change. |
<!-- end generated -->

```json
{ "type": "surface", "surface": "cube", "params": { "speed": "{{state.speed}}" }, "continuous": true,
  "width": 240, "height": 240, "rotate": 6 }
```

A document can't supply shader source: it only names what the host registered (see `SECURITY.md` for the limits on
a surface's passes and memory).

### `raster`
An image the screen edits (see Images), shown at its view: its layers flattened over a checkerboard where it is
transparent, on the node's `color`. Over it, the node draws what the tools are doing: the selection's marching ants,
the quick mask's tint, the crop being dragged (its rule-of-thirds lines, the rest shaded), the marquee or lasso being
drawn, and the brush's outline under the pointer (painting, erasing, cloning). The first raster node to show an image
fits it to the node.

The pointer on the node plays the image's current tool exactly as the `pointer` operation does: presses, moves,
releases and the wheel, with their modifier keys. While the node has the focus, the space bar held pans whatever the
tool; the wheel zooms about the pointer. Every node showing an image draws again when it changes, whoever changed it
(a node, a step, an agent's scenario), and the image's snapshot is published to its `into` path after each press,
release, pan and zoom (a drag's moves only draw). To assistive technology the node is an image named after its image,
unless the node has its own `label`.

<!-- generated: raster -->
| Key | Meaning |
|---|---|
| `image` | The name of an image the `images` section declares, made the first time a node or an `image` step names it. A name the section doesn't declare is an error at the node. |
| `checker` | A checkerboard where the image is transparent. Default: `true`. |
| `overlayColor` | The outlines of what the tools are doing (the marquee, the lasso, the crop and its thirds, the brush). Default: white, over a dark line. |
| `shadeColor` | Over what the crop being dragged leaves out. Default: black at half opacity. |
<!-- end generated -->

A profile-picture cropper: the person drags the crop, and a button keeps the square it shows.

```json
"images": { "avatar": { "width": 512, "height": 512, "into": "state.avatar",
                        "tools": { "crop": { "gesture": "crop" }, "hand": { "gesture": "pan" } } } },
"root": { "type": "column", "children": [
  { "type": "raster", "image": "avatar", "grow": 1, "color": "#1f2937", "shadeColor": "#000000aa" },
  { "type": "row", "children": [
    { "text": "Move", "onClick": [{ "image": "setTool", "with": { "tool": "hand" } }] },
    { "text": "Crop", "onClick": [{ "image": "setTool", "with": { "tool": "crop" } }] },
    { "text": "Undo", "disabled": "{{!state.avatar.canUndo}}", "onClick": [{ "image": "undo" }] } ] } ] }
```

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
| `accentEdge` | The edge of the active tab its accent line is on: `bottom` (default) or `top`. |
| `markerColor` | The markers' colour (default `accentColor`). |
| `keepMarker` | A marked tab keeps its marker while active, and shows its close button only while hovered (default false). |
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
| `items` | The rows, in order (or a binding such as `"{{state.files}}"`): a string (its id and label), or `{ "id", "label", "icon", "iconColor" }` (an icon before the label, by name, as a label's `icon`, in a hex colour or the text's); in a `tree`, also `"children"` (a list of items) and `"expandable"` (a closed node whose children aren't loaded yet: `onExpand` can load them); in a `table`, `"cells"`: the texts of the row's columns, in order (at most 256). Only the rows in view are drawn, so a list scrolls through a million items as fast as through ten. At most 1,000,000 items, nested at most 256 deep. |
| `expanded` | On a `tree`: the ids of the open nodes. Opening or closing one runs `onExpand`; keep the ids in state there to keep them over a rebuild. A state path (`"expanded": "state.files.open"`) does that itself: the ids are read from there, and kept there before `onExpand` runs. |
| `stickyRows` | On a `tree`: how many open folders stay pinned at the top while their children scroll by, nearest the root first (default 0, at most 16). A click on a pinned folder moves the cursor to it. |
| `guides` | On a `tree`: a line per level left of the nested rows (default true). |
| `sort` | On a `table`: the rows' order, `{ "column": id, "descending": false }` (or null for the items' order). A click on a header sorts by its column, ascending then descending, and runs `onSort`: numbers by value before text, equal cells in the items' order. A state path (`"sort": "state.files.sort"`) keeps the order there. |
| `selection` | `"single"` (the default), `"multiple"` (Shift adds a range, Cmd or Ctrl one row) or `"none"`. |
| `value` | The selected id, or with `"multiple"` the list of them (`bind` keeps it in a state path); a change runs `onChange`. |
| `rowHeight` | Each row's height in pixels (default 24). |
| `font`, `textColor`, `selectedTextColor`, `selectedBackground`, `accentColor` | The rows' font and colours: their text, the selected rows' text and background, and the accent (the keyboard cursor, the drop line, a tree's guides). |
<!-- end generated -->

```json
{ "type": "tree", "id": "explorer", "items": "{{state.files}}", "expanded": "{{state.open}}", "bind": "state.chosen",
  "onExpand": [ { "set": "state.open", "to": "{{event.expanded ? concat(state.open, [event.id]) : filter(state.open, it != event.id)}}" } ] }
```

### `graph`
A node editor: boxes with input and output ports, joined by curves, over a grid, with a minimap. The nodes and links
come from the node (`nodes`, `links`, usually bound to state) and the graph changes nothing itself: what the user does
runs the node's handlers, whose steps change the state. Dragging from a port draws a curve to the pointer and rings the
ports it may join (the same `kind`, or `"any"`; not a link already there, nor past a port's `links`; a cycle only with
`"cycles": true`): letting go on one runs `onConnect`, on empty space `onLinkDrop`. Dragging a linked input picks its
last link up: `onDisconnect` runs, then `onConnect` if it is dropped on another port. Nodes drag, with Shift for more
and a marquee from empty space (`onSelect`, `onMove`); Delete runs `onDelete`. The keyboard does it all: Page Up and
Page Down pick a node, the arrows move the selection, `[` and `]` pick a port, Enter on two ports links them, Shift+
Delete removes a port's links, + and - zoom, Home puts the view back. Off-screen nodes aren't drawn or laid out, so a
graph of thousands drags as fast as one of ten. The colours are `nodeColor`, `headerColor`, `textColor`,
`accentColor` (the selection) and `kindColors`, the font `font`.

<!-- generated: graph -->
| Key | Meaning |
|---|---|
| `nodes` | The nodes, usually from state: `{ id, title, x, y, width, bodyHeight, inputs, outputs }` (at most 10,000, ids unique). A port is an id, or `{ id, label, kind, links }`: links join ports of the same `kind` (or either `"any"`, the default), and `links` is how many it takes (0, the default, for any number); at most 64 inputs and 64 outputs. The node's children, made with `repeat` like the nodes, are their bodies: child i sits under node i's ports, `bodyHeight` tall (0, the default, for none), hidden when the node is off screen or the zoom below 0.5. |
| `links` | The links, `{ from, output, to, input }`: a node's output to a node's input (at most 50,000). The graph changes nothing itself: its handlers change the state. |
| `cycles` | Whether a link may close a cycle (a node linked to itself included); `false`, the default, refuses it. |
| `kindColors` | Each kind's colour, `{ "float": "#a3e635" }` (at most 64): its ports and the links from them. |
| `nodeColor`, `headerColor` | The nodes' box and header; their text and the selection's colours are `textColor` and `accentColor`. |
| `minimap` | Whether the minimap shows, at the bottom right (the default): a press or a drag there moves the view. |
| `zoom`, `pan` | The camera: the zoom (0.1 to 4, the default 1) and where the graph's origin is in the view, `[x, y]`. They are applied when they change, so the user's own zoom (+ and -) and pan (the wheel, or a drag with the right or middle button) stay until then. |
| `selectedIds` | The selected nodes' ids (applied when it changes); a click, Shift and a click, or a marquee changes the selection and runs `onSelect`. |
<!-- end generated -->

```json
{ "type": "graph", "id": "material", "nodes": "{{state.nodes}}", "links": "{{state.links}}",
  "kindColors": { "color": "#f472b6", "float": "#a3e635" },
  "onConnect": [ { "set": "state.links", "to": "{{concat(state.links, [{ from: event.from, output: event.output, to: event.to, input: event.input }])}}" } ],
  "onDisconnect": [ { "set": "state.links", "to": "{{filter(state.links, index != event.index)}}" } ],
  "onMove": [ { "set": "state.nodes", "to": "{{map(state.nodes, contains(event.ids, it.id) ? merge(it, { x: it.x + event.dx, y: it.y + event.dy }) : it)}}" } ] }
```

### `timeline`
Tracks of keyframes and clips under a time ruler, with a playhead: an animation or a video editor's timeline. Like
`graph`, it changes nothing itself: scrubbing runs `onScrub`, dragging a keyframe `onKeyframeMove`, moving or trimming
a clip (by its edges) `onClipChange`, and their steps change the state. The ruler's ticks follow the zoom, in frames,
seconds or timecode; the play range is shaded and markers cross the lanes. Drags snap to frames and, within a few
pixels, to keyframes, clip edges, markers and the playhead (Alt held at the press leaves it off). A clip dragged up or
down moves to another track. Keyframes show their easing: a square for step, a diamond for linear, a circle for
Bézier. Items select with a click, Shift and a click, or a marquee from an empty lane (`onSelect`). Groups of tracks
open and close; a track's M and L buttons mute and lock it. The keyboard does it all: Left and Right move the
playhead a frame (Shift: a second), Comma and Period jump to the previous or next edit, Up and Down pick a track,
Page Up and Page Down an item on it, Alt and the arrows move the selection, Enter opens a group, M and L mute and lock,
+ and - zoom. Only the rows on screen are laid out and drawn. `keyframeValue(track, time)` reads a track's value at a
time, as it is drawn.

<!-- generated: timeline -->
| Key | Meaning |
|---|---|
| `tracks` | The tracks, usually from state: `{ id, label, keyframes, clips, tracks, expanded, muted, locked }` (at most 10,000 in all, ids unique). A track with `tracks` is a group (at most 16 deep) that its chevron or Enter opens and closes (`onExpand`); its M and L buttons, or the keys, mute and lock it (`onChange`, `event.type` `"track"`): a locked track's items don't move. A keyframe is `{ time, value, easing }`, `easing` being `"step"` (drawn as a square), `"linear"` (a diamond, the default) or `"bezier"` (a circle; CSS's ease-in-out, or `[x1, y1, x2, y2]` for others). A clip is `{ id, label, start, duration, color }` (ids unique across tracks). At most 10,000 keyframes and 10,000 clips a track, 100,000 in all. The node's children, made with `repeat`, are the headers: child i is the i-th track (a group's tracks following it), placed in its row and hidden while the row is scrolled away or closed. |
| `duration`, `fps` | Its length, in seconds (default 10), and its frames per second (`fps`, 1 to 1000, default 30): times snap to frames. |
| `time` | The playhead, in seconds, usually `{{state.time}}` (applied when it changes). A press or a drag on the ruler, Left and Right (a frame; a second with Shift), Comma and Period (the previous or next keyframe, clip edge or marker), Home and End (the play range's ends) move it and run `onScrub`. |
| `timeFormat` | The ruler's labels: `"frames"` (the default), `"seconds"` or `"timecode"` (`HH:MM:SS:FF`). Its ticks follow the zoom. |
| `playRange` | `[start, end]`, shaded on the ruler, the rest of the lanes dimmed (without it, the whole duration). |
| `markers` | Times, or `{ time, label }`, marked on the ruler and across the lanes (at most 1,000). |
| `snap` | Whether drags snap (the default): to frames, and within a few pixels to keyframes, clip edges, markers, the play range and the playhead. Alt held at the press leaves it off for that drag. |
| `pixelsPerSecond`, `offset` | The camera: window units per second (1 to 10,000, the default 100) and the time at the lanes' left edge, `offset`. They are applied when they change, so the user's own zoom (+ and -) and scroll (the wheel sideways, or a drag with the right or middle button) stay until then. |
| `selectedKeys` | The selected items' keys (applied when it changes): a clip's id, or a track's id, `:` and a keyframe's index (`"pos:2"`). A click, Shift and a click, a marquee, or Page Up and Page Down on the keyboard's track changes it and runs `onSelect`; dragging the selection (or Alt and the arrows) runs `onKeyframeMove` and `onClipChange` for each item. |
| `headerWidth`, `trackHeight` | The headers' column (default 160) and a row's height (`trackHeight`, 12 to 400, default 28). |
| `headerColor`, `clipColor`, `textColor`, `accentColor`, `font` | The ruler and headers, the clips (a clip's own `color` first), the text and the selection. |
<!-- end generated -->

```json
{ "type": "timeline", "id": "anim", "tracks": "{{state.tracks}}", "time": "{{state.time}}", "duration": 4, "fps": 24,
  "timeFormat": "seconds", "markers": [ { "time": 2, "label": "beat" } ],
  "onScrub": [ { "set": "state.time", "to": "{{event.time}}" } ],
  "onKeyframeMove": [ { "set": "state.tracks", "to": "{{map(state.tracks, it.id == event.track ? merge(it, { keyframes: map(it.keyframes, index == event.index ? merge(it, { time: event.time }) : it) }) : it)}}" } ] }
```

### `calendar`
A month of days to pick a date from. A header shows the month and year between a previous and a next button, a row
names the days of the week, and six weeks of days follow; days outside the range show disabled, today is outlined and
marked days carry a dot. The keyboard does it all: the arrows move a day or a week (mirrored right to left), Page Up
and Page Down a month (with Shift, a year), Home and End to the week's ends, and Enter or Space picks the day. Its role
is `grid`. Dates are ISO 8601 texts, `"YYYY-MM-DD"` (see Dates).

- `value` (or `bind`): the picked date; a pick runs `onChange` with `event.value`.
- `min`, `max`: the first and last days that can be picked; `today`: the day outlined (the document gives it, so a
  screenshot doesn't change from day to day; `today()` is the system's).
- `marked`: the days with a dot. `firstDayOfWeek`: 0 for Sunday to 6 for Saturday (default 1, Monday).
- `monthNames` (twelve, January first) and `dayNames` (seven, short, Sunday first): the locale's words, usually from
  `t()`; missing ones stay English.
- `font`, `textColor`, `accentColor` (the picked day, the keyboard's ring and the dots), `pickedTextColor` (the picked day's number; a name starting with `on` would read as an event).

```json
{ "type": "calendar", "bind": "state.due", "today": "2026-10-22", "min": "2026-01-01", "focusable": true,
  "dayNames": "{{split(t('calendar.days'), ',')}}", "onChange": [ { "log": "due {{event.value}}" } ] }
```

### `path`
A vector shape from SVG path data, filled and stroked, fitted to the node (like an icon or a chart line). It is
drawn anti-aliased in the same batch as the panels and text around it, and its meshes are built again only when its
data or its size changes.

<!-- generated: path -->
| Key | Meaning |
|---|---|
| `d` | SVG path data: `M L H V C S Q T A Z`, upper case absolute and lower case relative, e.g. `"M 0 0 L 10 0 L 5 8 Z"`. At most 64 KiB and 16,384 commands, coordinates within a million: a revision with data that can't be read is refused, naming the place of the problem. |
| `viewBox` | The area of the data's coordinates fitted to the node, `"x y width height"` like SVG's. Without it the data is in the node's own pixels, from its top-left corner. |
| `fit` | How the view box fills the node: `"contain"` (the default), `"cover"` or `"fill"`, as for images. |
| `fillColor` | The inside's colour; without it the path isn't filled. |
| `fillRule` | Which points are inside: `"nonzero"` (the default) or `"evenodd"` (a shape inside another is a hole). |
| `fillGradient` | The inside as a gradient across the node's box, in place of `fillColor`: the `gradient` node's value, `{ "kind": "linear" or "radial", "angle": 90, "stops": [{ "at": 0, "color": "@accent" }] }` (1 to 32 stops; a radial one is centred, reaching the corners). |
| `stroke` | The outline's colour; without it there is no outline. |
| `strokeWidth` | The outline's width (default 1), in the view box's units (pixels when stretched by `"fill"`). |
| `strokeJoin` | Corners: `"miter"` (the default, bevelled past 4 widths), `"round"` or `"bevel"`. |
| `strokeCap` | Ends: `"butt"` (the default), `"round"` or `"square"`. |
| `strokeDash` | Dash and gap lengths, repeated, e.g. `[6, 3]` (at most 64); without it the outline is solid. |
<!-- end generated -->

```json
{ "type": "path", "id": "check", "d": "M 4 12 L 9 17 L 20 6", "viewBox": "0 0 24 24", "width": 24, "height": 24,
  "stroke": "@accent", "strokeWidth": 2.5, "strokeCap": "round", "strokeJoin": "round" }
```

An `image` can also show an SVG file (`.svg`, at most 1 MiB): paths and basic shapes with solid fills and strokes,
rasterised at twice the size it declares. Gradients, text, masks, filters and rotations aren't drawn.
`examples/shared/components/charts.json` has line, bar and area charts built on `path`, to `include`.

### `canvas`
An infinite canvas, as in whiteboards and design tools: its children and its `items` are placed in world units and
seen through a camera. Drag the background (or hold Space, or use the middle button) and scroll to pan; pinch, or Ctrl
or Cmd with the wheel, zooms at the pointer, as do `+` and `-` (`0` resets the zoom, Shift+1 fits everything, Shift+2
the selection). Click, Shift-click and a marquee select; the handles move, resize (eight of them) and rotate; arrow
keys nudge (Shift: 10). Moves and resizes snap to the grid, to other items' edges and centres and to guides, with
smart guides drawn while dragging. Rulers along the top and left follow the zoom; dragging out of one makes a guide,
dragging a guide back onto it removes it. The canvas doesn't move anything itself: it runs `onTransform` with where
the items should go, and the document applies it. `items` are drawn by the canvas, only those in view, so thousands
cost what is on screen; an application with more (up to a million) gives them through a model. The selection is
announced as a state of the canvas ("3 selected"). With `scaleChildren` the children are drawn at the camera's zoom,
text included, like a slide or a page in a drawing app, and a single selected child can be rotated by its handle.

<!-- generated: canvas -->
| Key | Meaning |
|---|---|
| `view` | The view (`bind` keeps it in a state path, both ways): `{ "camera": { "x", "y", "zoom" }, "selection": [ids], "guides": { "vertical": [x], "horizontal": [y] } }`, each part only if given. The camera's x and y are the world point at the top-left; the zoom (screen units per world unit) stays within 0.01 and 256. Panning, zooming, selecting and dragging guides out of the rulers run `onChange`; `animate` on `state.<bind>.camera.zoom` (or x, y) moves the camera smoothly. |
| `items` | Items the canvas draws itself, in world units, in drawing order (at most 10,000; an application gives more through a model): `{ "id", "x", "y", "width", "height", "rotate", "color", "borderColor", "borderWidth", "radius", "text", "textSize", "textColor" }`. Only those in view are drawn; text too small to read draws as a bar. Children are placed in world units too, by their `x`, `y`, `width` and `height`. |
| `lod` | Levels of detail: `[{ "for": "card", "below": 0.25, "use": "cardSmall" }]`: below that zoom the child `use` (a lighter sibling, by id) is drawn in place of the child `for`. |
| `grid` | World units between grid lines (default 0: none); drawn when the lines are far enough apart, and snapped to. |
| `snap` | Moves and resizes snap to the grid, other items' edges and centres, and guides (default true). |
| `rulers` | Rulers along the top and left, in world units; dragging out of one makes a guide (default true). |
| `scaleChildren` | Children are drawn at the camera's zoom, text and all, like a page in a drawing app, and can be rotated by their handle (default false: their rectangles scale while their text keeps its size). |
| `font` | The items' text font (default `default`), in a few sizes. |
| `accentColor` | The selection, the marquee, the handles and the guides. |
<!-- end generated -->

```json
{ "type": "canvas", "id": "board", "bind": "state.board", "grid": 20,
  "items": "{{state.cards}}", "onTransform": { "run": "moveCards", "with": { "moved": "{{event.items}}" } },
  "lod": [{ "for": "chart", "below": 0.25, "use": "chartSmall" }] }
```

`{ "animate": "state.board.camera.zoom", "to": 2, "ms": 300 }` moves the camera smoothly. Until panels can be
transformed, a child's text keeps its size while its box, images and paths scale with the zoom.

### `scene`
A 3D scene: primitive meshes (`cube`, `sphere`, `cylinder`, `cone`, `torus`, `plane`) and glTF models, placed by
transforms, lit by point, sun and spot lights and drawn with PBR materials, over a ground grid and axes. The middle
button orbits, Shift with it pans and the wheel zooms at the pointer (`controls`); a click picks, a drag over nothing
draws a box, and the selection is outlined. A `gizmo` moves, rotates and scales the selection by its handles; the
scene doesn't move the nodes itself, the document applies `onTransform`. glTF models (with their buffers and
textures) are checked and bounded (64 MiB a file, 4,096 nodes and meshes, two million vertices); a model that fails
to load is left out and named in the log.

<!-- generated: scene -->
| Key | Meaning |
|---|---|
| `bind` | A state path keeping the view (camera and selection), both ways. |
| `view` | The view, if there is no `bind`: `{ "camera": { "target": [x, y, z], "yaw", "pitch", "distance", "fieldOfView", "orthographic" }, "selection": [ids] }`, each part only if given. Angles are in radians; the pitch stays within ±90°, the distance within 0.01 and 100,000, the field of view within 0.05 and 3. Orbiting, panning, zooming and selecting write it back and run `onChange`. |
| `scene` | The scene, usually `{{state.scene}}`: `{ "nodes": [...], "materials": [...] }`. A node: `{ "id", "name", "parent", "translation": [x, y, z], "rotation": [x, y, z, w] (or `"rotationDegrees": [x, y, z]`), "scale", "mesh", "material", "light", "camera", "visible", "asset" }`; `mesh` is `"cube"`, `"sphere"`, `"cylinder"`, `"cone"`, `"torus"`, `"plane"` or a mesh of an asset; `light` is `{ "type": "directional" | "point" | "spot", "color", "intensity", "range", "innerCone", "outerCone" }`. A material (PBR): `{ "id", "name", "baseColor": [r, g, b, a], "metallic", "roughness", "emissive", "alphaMode", "alphaCutoff", "doubleSided" }`. Read when it changes, checked and bounded. |
| `assets` | glTF models (`.gltf` or `.glb`) by name: `{ "robot": "models/robot.glb" }`, at most 64, each read with the `reader.imageFile` permission and checked. A node with `"asset": "robot"` holds the model's nodes (ids `"<its id>/<id>"`); a click on them selects that node. |
| `animations` | Animation clips: `[{ "name", "tracks": [{ "node", "path": "translation" | "rotation" | "scale", "interpolation": "linear" | "step" | "cubic", "times", "values" }] }]`, played at `time`. |
| `time` | The animations' time, in seconds (default 0). |
| `shading` | `"material"` (the default: lit, with the lights and materials), `"solid"` (one grey, a studio light) or `"wireframe"`. |
| `grid` | A ground grid (default true). |
| `axes` | The X (red) and Z (blue) axes on the floor (default true); with `"up": "z"`, X and Y (green). |
| `glyphs` | Lights and cameras drawn as a modeller draws them (default false: a small cross): a camera's frustum with its up triangle, a light's circle with a line down to the floor (a sun's rays, a spot's cone); the selected ones in `outlineColor`. |
| `up` | Which way is up in the scene's frame: `"y"` (the default, as glTF) or `"z"` (as CAD and modelling tools): then the nodes' transforms, the world gizmo's axes, the picked `point`, the `cursor3d` and the pivot are Z up, and the floor is the XY plane. |
| `cursor3d` | A 3D cursor drawn at `[x, y, z]` (none without it): a ring of red and white dashes facing the eye; e.g. set from a pick's `point`, and used as the gizmo's `pivot` or where new nodes go. |
| `background` | Behind the scene (default the theme's). |
| `gridColor` | The grid's lines. |
| `outlineColor` | The selection's outline (default orange). |
| `exposure` | The light's multiplier, before tone mapping (default 1). |
| `gizmo` | A gizmo on the selection (none without it): `{ "visible", "mode": "translate" | "rotate" | "scale", "space": "world" | "local", "pixels" (its size), "pivot": [x, y, z] (where it turns and scales about, if not the selection's centre), "snap": { "translate", "rotate" (degrees), "scale" } }`. Dragging a handle runs `onTransform`. |
| `controls` | The pointer's gestures: `{ "orbit": "middle", "pan": "shift+middle", "select": "left", "zoom": true, "selects": true, "minimumDistance", "maximumDistance" }` (these are the defaults); a binding is `"left"`, `"middle"` or `"right"` after any of `"shift+"`, `"control+"` and `"alt+"`, or `"none"`. A click picks, a drag on nothing draws a box; `selects` false leaves the selection to the handlers. |
| `onSelect` | A click (`event.type` `"pick"`: `id`, or null on nothing, `button`, `shift`, `x`, `y`, the world `point`) or a box (`"boxSelect"`: `ids`, `shift`, `box`), with the `selection` after it. |
| `onTransform` | A gizmo drag: `event.phase` (`"preview"`, `"end"` or `"cancel"`), `mode`, `axis`, and `nodes`: `[{ "id", "translation", "rotation", "rotationDegrees", "scale" }]`, and `text` (e.g. `"Move 1, 0, 0"`). |
<!-- end generated -->

```json
{ "type": "scene", "id": "viewport", "bind": "state.view", "scene": "{{state.scene}}",
  "assets": { "robot": "models/robot.glb" }, "shading": "{{state.shading}}",
  "gizmo": { "mode": "translate", "snap": { "translate": 0.5 } },
  "onSelect": { "set": "state.view.selection", "to": "{{event.selection}}" },
  "onTransform": { "run": "moveNodes", "with": { "nodes": "{{event.nodes}}" } } }
```

`{ "camera": "#viewport", "view": "top", "frame": "selection" }` snaps the camera to a view (front, side, top) and
frames the selection; `"projection": "toggle"` switches between perspective and orthographic, `"orbit": [15, 0]`
turns it as a drag would and `"zoom": 1` comes one wheel step closer. Scenarios and `affrescoui render` capture a
scene like any other node.

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
`children`; with several slots, each gets them, and they see the variables where they are placed (a slot inside a
`repeat` shows them once per item). Changing a component changes every use: one edit restyles a whole list. Nodes built inside each other
(components and outlets included) nest at most 256 deep, so a component or a screen inside itself is an error.

### Built-in components
These need no definition (a screen's own component of the same name wins). They are written in the
format, take their colours and sizes as parameters whose defaults are the kit's tokens (see The kit's tokens), and
keep their open state where `open` says (a state path):
- `menu` (below its parent), `submenu` (to its right) and `contextMenu` (where `x`, `y` say): `items` of
  `{ "title", "keyLabel", "mnemonic", "enabled", "checked", "icon", "separator", "submenu", "run", "with" }`. The
  arrows, Home, End and the items' first letters move and choose; Enter or a click runs the item's action and closes
  the menus; a click outside or Escape closes them. A `checked` item shows a check (and is selected for screen
  readers), and with `"icons": true` the items show their `icon`. A submenu's state is `open + 'Sub'`, one key per
  item (`.m0`, `.m1`, ...). Open a context menu from `onContextMenu`, which a right click and, on the focused node,
  Shift+F10 or the Menu key send.
- `menuBar`: `menus` of `{ "name", "title", "items" }`, or of `{ "menu", "title" }` naming a menu of `source` (the
  `menus` variable); by default `window.menuBar`, so one list feeds the native bar and the drawn one. A menu's `width`
  sizes its heading (by default, from the title's length). Alt with a menu's first letter opens it, and `open` is an
  object with one flag per menu name.
- `toolbar`: a row of `items` (`{ "title", "icon", "keyLabel", "tooltip", "checked", "enabled", "run", "with" }`, or
  `{ "separator": true }`) as wide as `width`; the arrows move between them, a `checked` one is a pressed toggle, the
  tooltip names the shortcut, and those that don't fit go into a "more" menu (open at `open + '.more'`).
- `statusBar`: a row of `items` (`{ "text", "icon", "tooltip", "color", "run", "with", "side" }`), `"side": "end"`
  putting one at the end; an item with a `run` is a button.
- `dialog`: a modal `title`, `message` and `buttons` (`{ "title", "run", "with", "keepOpen", "default" }`) over a
  backdrop; the focus is trapped in it, the `"default": true` button is drawn as primary (else the first) and
  takes the focus, Enter choosing the focused button, Alt with a letter a button, Escape cancels.
  A `"keepOpen": true` button doesn't close it (its action does, setting `open` false). Its `children` go between the
  message and the buttons.
- `toast`: a `message` in the bottom right corner, a `progress` bar (0 to 1, or null) and `actions`; its × is a
  button named `closeLabel`.
- `toasts`: a stack of notifications in the bottom corner from the list at `state[name]` (`name` is `toasts`), each
  `{ "message", "progress", "color", "actions" }`: the last `limit` show, the newest at the bottom, and screen
  readers announce them. A toast's × or action removes it from the list (an action then runs its `run`).
- `hoverCard`: shows a card with `title` and `text` while the pointer is over its `children`.
- `popover`: a card with `title`, `text` and its `children`, `width` by `height`, below its parent (or `anchor`;
  `placement` `below-end` lines it up with the end) with an arrow `arrowAt` from its start pointing at it; it flips
  above when only that side has room, and a click outside or Escape closes it.
- `settingsPage`: the document's settings with a search field (see Menus and settings).
- `quickPick`: a search field over `providers` of `{ "prefix", "items", "key", "placeholder", "run", "filter" }` (the
  one whose `prefix` starts the query, else the one without), matched with `fuzzySort` on each item's `key` (`title`)
  and the matching letters highlighted; a provider with `"filter": false` shows its items as given (already matched
  by the document, such as a search inside the file the query names). Its state is `state[name]` (`name` is
  `quickPick`): `open`, `query`, `index`, `top` and `recent` (the default provider's last `recentLimit` choices, shown
  first). Only the visible `rows` are built; the arrows move, Enter or a click runs the item's `run` (with its
  `with`), or else the provider's `run` with the item as `item`. Open it with `{ "set": "state.quickPick.open", "to": true }`, and a prefix with `query`.
  With `"stacked": true`, each row's `detail` goes on a second line under its title (in `detailFont`, if given):
  give it a `rowHeight` of about 44. An item's `badge` (a letter or two, e.g. the kind of a symbol) shows in a small box
  before its title, coloured `badgeColor`.

```json
{ "use": "dialog", "with": { "open": "state.confirm", "title": "Save changes?",
  "buttons": [{ "title": "Save", "run": "save" }, { "title": "Cancel" }] } }
{ "use": "popover", "with": { "open": "state.share", "title": "Share", "text": "Anyone with the link can view." } }
{ "use": "toasts", "with": { "name": "notes" } }
{ "push": "state.notes", "value": { "message": "Export finished", "actions": [{ "title": "Open", "run": "open" }] } }
{ "label": "Document", "focusable": true,
  "onContextMenu": { "set": "state.ctx", "to": { "open": true, "x": "{{event.x}}", "y": "{{event.y}}" } },
  "children": [{ "use": "contextMenu", "with": { "open": "state.ctx.open", "x": "{{state.ctx.x}}",
    "y": "{{state.ctx.y}}", "items": [{ "title": "Word wrap", "checked": "{{state.wrap}}", "run": "toggleWrap" }] } }] }
{ "use": "toolbar", "with": { "items": [{ "title": "Undo", "icon": "undo", "keyLabel": "Ctrl+Z", "run": "undo" },
  { "separator": true }, { "title": "Grid", "icon": "layout-grid", "checked": "{{state.grid}}", "run": "toggleGrid" }] } }
```

### Built-in controls
Controls of a desktop app's window, built in the same way as the built-in components (see Built-in components):
- `segmentedControl`: a row of `items`, each `{ "id", "title", "icon", "tooltip", "enabled", "width", "run",
  "with" }`; the chosen `id` is `state[name]`, a click or Space picks one (then runs its `run`), and the arrows move.
  Screen readers hear a group of radio buttons.
- `searchField`: a text box over `state[name]` with a search icon and a × that clears it (as Escape does); Enter runs
  `run` with the text as `query`. Its border lights while it has focus, and it stays left to right.
- `breadcrumbs`: a path of `items`, each `{ "title", "icon", "run", "with" }`, with chevrons between them; each but the
  last is a link that runs its `run`.
- `tabStrip`: the tabs at `state[name]` as `{ "tabs", "active" }`, each tab `{ "id", "label", "marker", "closable" }`
  (`marker` draws a dot, for unsaved changes). A click activates one, its × closes it (running `onClose` with its
  `id`, or else removing it and activating its neighbour), and a drag or Ctrl+Shift+Page Up/Down moves it. The tabs
  that don't fit in `width` are listed under an "All tabs" button (`more` in its state).

```json
{ "use": "tabStrip", "with": { "name": "files", "onClose": "closeFile" } }
{ "push": "state.files.tabs", "value": { "id": "notes", "label": "notes.txt", "closable": true } }
{ "use": "breadcrumbs", "with": { "items": [{ "title": "Projects", "icon": "folder", "run": "openProjects" },
  { "title": "main.cpp" }] } }
{ "use": "segmentedControl", "with": { "name": "view", "label": "View",
  "items": [{ "id": "list", "title": "List" }, { "id": "grid", "title": "Grid", "icon": "layout-grid" }] } }
{ "use": "searchField", "with": { "name": "find", "run": "search" } }
```

### Built-in panels
The larger parts of a desktop app's window, built in the same way (see Built-in components):
- `dock`: a window's panels in tabbed areas: `left`, `right` and `bottom` around the `center`. `panels` names them
  (`{ "explorer": { "title": "Explorer", "icon": "folder" } }`) and `state[name]` places them: each area
  `{ "tabs", "active", "collapsed" }`, plus `sizes`, `maximized` (an area or null) and what is being done (`resizing`,
  `menu`). The use's children are shown in every area, with `area` and `panel` (its active panel, `""` in a centre
  without tabs), so each says which panel it is with `when`. A tab dragged to another area's tabs or body moves there,
  as from an area's "Move to" menu (the keyboard's way); a side area collapses to a strip of its panels' icons, the
  bottom one to its tabs, and one maximises over the others. An area without panels goes. An area resizes by dragging
  its inner edge, or from it with the arrows (`dir`: the dock's writing direction, for which way they go; `sizes`,
  `minSize`, `maxSize`). The areas mirror right to left.

```json
{ "fill": true, "use": "dock", "with": { "dir": "{{state.dir}}", "panels": {
    "explorer": { "title": "Explorer", "icon": "folder" }, "terminal": { "title": "Terminal", "icon": "terminal" } } },
  "children": [
    { "when": "{{panel == ''}}", "fill": true, "type": "editor", "bind": "state.text" },
    { "when": "{{panel == 'explorer'}}", "fill": true, "type": "tree", "items": "{{state.files}}" },
    { "when": "{{panel == 'terminal'}}", "fill": true, "type": "label", "text": "{{state.output}}" }
  ] }
{ "set": "state.dock", "to": { "left": { "tabs": ["explorer"] }, "bottom": { "tabs": ["terminal"] },
  "center": { "tabs": [] } } }
```

- `inspector`: a property sheet editing an object in the state, in collapsible `groups`
  (`[{ "id", "title", "rows" }]`). A row is `{ "key", "label", "type", "unit", "min", "max", "step", "decimals",
  "options", "axes", "enabled", "tooltip" }` and edits `values[key]` at `bind + "." + key`: a `number` (the default)
  changes by dragging its label, typing or the arrows, rounded to `decimals` (from `step` when not given) and kept
  within `min` and `max`, a text that isn't one changing nothing; a `slider`, a `color` (a swatch opening a picker), a
  `select` (`options`: ids or `{ "id", "label" }`), a `toggle`, a `checkbox`, a `text`, or a `vector`, a list of
  numbers with a field for each of `axes` (default `["x", "y", "z"]`). Every change runs the action `onChange` names
  with `{ key, value }`. `state[name]` keeps the closed groups (`closed`) and the open picker or list (`open`).

```json
{ "use": "inspector", "with": { "bind": "state.shape", "values": "{{state.shape}}", "onChange": "shapeChanged",
  "groups": [{ "id": "transform", "title": "Transform", "rows": [
    { "key": "width", "label": "Width", "unit": "px", "min": 0 },
    { "key": "position", "label": "Position", "type": "vector", "step": 0.5 },
    { "key": "fill", "label": "Fill", "type": "color" },
    { "key": "blend", "label": "Blend", "type": "select", "options": ["normal", "multiply"] } ] }] } }
```

- `ribbon`: tabs of groups of buttons across a window's top. `tabs` is `[{ "id", "title", "groups" }]`, a group
  `{ "id", "title", "icon", "items" }` and an item a toolbar's (`title`, `icon`, `tooltip`, `keyLabel`, `checked`,
  `enabled`, `run`, `with`) plus `size`: `"large"` (an icon over its title) or small (an icon beside it, three to a
  column). Groups that don't fit its `width` fold, from the last, into a button each that opens a menu of their items.
  A tab's double click or the end button collapses it to its tabs, and a tab opens it again. `largeFont` names a
  bigger icon font for the large buttons. `state[name]` keeps `tab`, `collapsed` and the open menus (`menus`).

```json
{ "use": "ribbon", "with": { "largeFont": "largeIcons", "tabs": [{ "id": "home", "title": "Home", "groups": [
  { "id": "clipboard", "title": "Clipboard", "items": [
    { "title": "Paste", "icon": "clipboard", "size": "large", "run": "paste" },
    { "title": "Cut", "icon": "scissors", "run": "cut" }, { "title": "Copy", "icon": "copy", "run": "copy" } ] }] }] } }
```

### Chat components
The parts of a conversation with an assistant (see Asking the agents) are built-in components too. They run the actions they are given with
  `with`: `chatBubble` (`text`, `from`, `mine`, `time`, `error`; `padding` and `radius`, so a `color` of
  `transparent` draws a reply as plain text), `typingIndicator` (`label`; its active dot is
  `phase % 3`), `proposalCard` (`title`, `summary`, up to three `lines` such as a proposal's `changes`, in the font
  `lineFont`, a `status`, and `preview`, `accept`, `reject` buttons until the status is `accepted`, `rejected` or
  `closed`), `chatComposer` (a card: an editor bound to `bind`, showing `value`, above a bar with the use's `children`
  at its start, such as an attach button or a model picker, and a round send button at its end. Enter runs `send`
  unless the draft is blank, Shift+Enter breaks the line, Escape runs `close`; while `working` is true, a `stop`
  action turns the send button into a stop button. It lays out left to right, as the editor starts its lines at the
  left) and `chatLauncher` (a round button that toggles `open`, or runs `run`, with an `unread`
  badge).

### The kit's tokens
The built-in components draw with these design tokens, used as `"@kit.surface"`. A screen restyles every
built-in at once by setting them in `tokens` or `colors`. The colours are the dark ones unless the screen names a
`kit.` token itself (`"tokens": { "kit.radius": 4 }` is enough): then each has a light variant too, chosen by `theme`
like the screen's own (`"theme": "{{window.colorScheme}}"` follows the system).

<!-- generated: kitTokens -->
| Token | Dark | Light | For |
|---|---|---|---|
| `kit.background` | `#1e1e1e` | `#ffffff` | Pages and the settings page. |
| `kit.surface` | `#252526` | `#ffffff` | Menus, dialogs, toasts, cards and popovers. |
| `kit.bar` | `#3c3c3c` | `#f3f4f6` | Menu bars, toolbars and status bars. |
| `kit.barHover` | `#505050` | `#e5e7eb` | A bar's item under the pointer, focused or open. |
| `kit.field` | `#3c3c3c` | `#ffffff` | Text fields. |
| `kit.text` | `#cccccc` | `#1f2328` | Text. |
| `kit.dim` | `#8b8b8b` | `#6e7781` | Secondary text: shortcuts, hints, disabled items. |
| `kit.body` | `#a0a0a0` | `#57606a` | A dialog's message and a card's text. |
| `kit.selection` | `#04395e` | `#ddf4ff` | The highlighted item of a menu or a list. |
| `kit.border` | `#454545` | `#d0d7de` | Borders and separators. |
| `kit.primary` | `#0e639c` | `#0969da` | The main button and progress. |
| `kit.onPrimary` | `#ffffff` | `#ffffff` | Text on `kit.primary`. |
| `kit.secondary` | `#3a3d41` | `#eaeef2` | Other buttons, tracks, a pressed or checked item. |
| `kit.onSecondary` | `#ffffff` | `#1f2328` | Text on `kit.secondary`. |
| `kit.accent` | `#2aaaff` | `#0969da` | Matched letters, links and the focused item's mark. |
| `kit.focus` | `#ffffff` | `#0969da` | The ring of the focused button. |
| `kit.backdrop` | `#00000088` | `#00000040` | Behind a modal dialog. |
| `kit.info` | `#3794ff` | `#0969da` | Information. |
| `kit.warning` | `#cca700` | `#9a6700` | Warnings. |
| `kit.error` | `#f14c4c` | `#cf222e` | Errors. |
| `kit.success` | `#89d185` | `#1a7f37` | Success. |
| `kit.radius` | 4 | 4 | The corners of menus, buttons and fields. |
| `kit.cardRadius` | 6 | 6 | The corners of dialogs, toasts, cards and popovers. |
| `kit.borderWidth` | 1 | 1 | Borders. |
<!-- end generated -->

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
  `min(a, b, ...)` (numbers or a list), `max(a, b, ...)` (numbers or a list), `clamp(value, low, high)`, `sin(radians)`,
  `cos(radians)`, `tan(radians)`, `asin(number)`, `acos(number)`, `atan(number)`, `atan2(y, x)`, `radians(degrees)`,
  `degrees(radians)`, `pi()`, `sqrt(number)`, `pow(base, exponent)`, `exp(number)`, `log(number, base?)`,
  `hypot(a, b, ...)` (numbers or a list), `sign(number)`, `upper(text)`, `lower(text)`, `trim(text)`,
  `split(text, separator)`, `number(value)`, `text(value)`, `fixed(number, digits)`,
  `pad(value, width, fill?)` (width up to 10000), `plural(count, word, plural?)`, `parseJson(text)`, `json(value)`,
  `length(value)`, `concat(a, b, ...)`, `flat(list)`, `join(list, separator)`, `slice(list or text, from, to?)`,
  `contains(list or text or object, value)`, `indexOf(list or text, value)`, `keys(object)`, `values(object)`,
  `entries(object)`, `fromEntries(list)`, `merge(a, b, ...)`, `reverse(list or text)`, `range(count)` (count up to 10000), `now()`,
  `time(milliseconds, 'ddd D MMM HH:mm')`, `fuzzy(query, text)`, `fuzzySort(list, query, key?, limit?)`,
  `curve(points, x, interpolation?)`, `gradientColor(gradient, at)`, `keyframeValue(track, time)`, `t('key', {params})`,
  `formatNumber(n, fewest?, most?)`, `formatPercent(ratio, decimals?)`, `formatCurrency(amount, 'EUR')`,
  `formatDate(milliseconds, style?, zone?)`, `formatTime(milliseconds, style?, zone?)`, `formatRelative(value, 'day')`,
  `formatList(list, 'and' | 'or')`, `compareText(a, b)`, `sortText(list)`, `today()`, `addDays(date, days)`,
  `addMonths(date, months)`, `daysBetween(from, to)`, `weekday(date)`, `daysInMonth(date)`, `dateOf(milliseconds)`,
  `dateMs(date)`, `isDate(text)`, `passwordStrength(text)`, `isEmail(text)`, `pathTree(list, key?)`;
- per item, with the item as `it` and its position as `index`: `filter(list, condition)`, `map(list, expression)`,
  `count(list, condition?)`, `sum(list, expression?)`, `find(list, condition)`, `some(list, condition)`,
  `every(list, condition)`, `sort(list, key?, 'desc'?)`.
- per item, with the item as `it` and its position as `index`: `filter(list, condition)`, `map(list, expression)`,
  `count(list, condition?)`, `sum(list, expression?)`, `find(list, condition)`, `some(list, condition)`,
  `every(list, condition)`, `sort(list, key?, 'desc'?)`, `reduce(list, expression, initial?)`,
  `scan(list, expression, initial?)`.

`reduce` and `scan` fold a list: the expression also sees the value so far as `acc` (first `initial`, else null),
and its result is the next one; `reduce` gives the last, `scan` every one (`scan(days, it ? acc + 1 : 0, 0)` gives the
running streak). Each function is also an operator: `length(x)` is `{"$length": x}`, `filter(l, c)` is `{"$filter": [l, c]}`. Texts
are UTF-8: `length`, `indexOf` and `slice` count bytes, `split(text, '')` and `reverse` keep characters whole.
`fuzzy` matches the query's characters in order, in any case, and gives null or `{ score, ranges, parts }`: `ranges`
are the matched `[start, end]` character spans, `parts` the text cut into `{ text, match }` for highlighting.
`fuzzySort` keeps a list's matches (on each item's `key`, or the item itself) as `{ item, score, ranges, parts, index }`,
the best first (an empty query keeps the order), at most `limit` of them.

**Variables:** `state`; `computed`; `event` (what triggered a handler); `window` (`width`, `height`, `scale`: the
display's pixels per unit, `colorScheme`: the system's `"light"` or `"dark"`, `menuBar`: the screen's `window.menuBar` list
(or `[]`), `nativeMenuBar`: whether it is shown natively); `theme`; `screen` (the screen an outlet shows); `nodes` (`nodes.sidebar.width`: a built node's `x`, `y`, `width`, `height`,
`visible`, from the last layout); `revision` (the current commit: `number`, `id`, `short`, `author`, `committer`,
`message`, `time`); `history`; `proposals` (`id`, `author`, `message`, `head`, `short`, `commits`, `operations`,
`touches`, `changes`, `remember`, `reason`, `time`); `preview`; `mode`; `peers` (`author`, `role`);
`requests` (the `ask` requests, see "Asking the agents"); and the names given by `repeat`, `let`,
`with`, `forEach`, a component's params, `it`/`index` inside list functions.

**`computed`** values are evaluated when first read after the state changed, then cached; they may use each other
(not in a circle, and at most 32 deep). Prefer them to actions that recount things by hand.

**Truthy** means anything except `false`, `null`, `0`, `""`, `[]` and `{}`.

### Work budget
An event (a handler, a timer, `onLoad`, `watch`) or an update may run 100,000 steps, evaluate
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
  were there before don't block it. A value kept on disk (`persist`) that no longer matches opens as the document's
  own, and the file's is kept in `<file name>.state.rejected.json`, beside it.
- **The checker** reports a `set` whose value can't match (a value written as it is is matched in full, enums and
  limits too; an expression by the kind of value it gives), a `toggle` of something that isn't a boolean, an
  `increment` or `animate` of something that isn't a number, a `push` onto something that isn't a list or of an item
  that doesn't match, a `bind` whose widget writes another kind of value (a toggle or checkbox writes true or false,
  a slider a number, a text area or editor text), and `a == b` or `a != b` comparing a state path that has an `enum`
  with a value it never takes. A declared key or property can be read even before the state holds it; an object's
  other keys are read only if its type says `"additionalProperties": true` or a type.
- **While the screen runs,** steps and widgets aren't stopped by the types: they are a contract for revisions and a
  description for agents and tools.

### Strings and locales

A screen's text can come in several languages. The `strings` section holds, per locale, keys and their messages;
`t("key", {params})` gives the message of the shown locale with its parameters filled:

```json
"strings": {
  "default": "en",
  "locale": "{{state.locale}}",
  "en": { "files": "{n, plural, =0 {No files} one {# file} other {# files}}", "hello": "Hello, {name}" },
  "it": { "files": "{n, plural, =0 {Nessun file} one {# file} other {# file}}", "hello": "Ciao, {name}" }
},
"text": "{{t('files', {n: state.count})}}"
```

- **The locale** is `strings.locale` (usually bound to the state, so a setting can change it), else the host's, from
  the system (`Screen::setLocale`), else `default`, else `"en"`. Expressions read it as `locale` (`"fr-CA"`).
- **Fallback:** a key is looked up in the locale (`fr-CA`), then its parents (`fr`), then `default`, then `en`; a key
  found nowhere shows as itself, so a missing translation is visible but never blank. `strings` may come from an
  `include`d file, whose locales and keys merge into the screen's.
- **Messages** are a subset of ICU MessageFormat: `{name}` (a number in the locale's way), `{n, number}` (also
  `integer` or `percent`), `{t, date}` and `{t, time}` (milliseconds; `short`, `medium`, `long` or `full`),
  `{n, plural, =0 {...} one {...} other {...}}` with the locale's categories (`zero`, `one`, `two`, `few`, `many`,
  `other`), `offset:1` and `#` for the number, and `{kind, select, a {...} other {...}}`. `other` is required;
  `'{'` writes a brace and `''` a quote. Plurals and selects nest at most 8 deep; a message has at most 4,096 bytes.
- **Formatting functions** use the locale's CLDR data: `formatNumber(n, fewest, most)`, `formatPercent(ratio)`,
  `formatCurrency(amount, "EUR")`, `formatDate(ms, style)`, `formatTime(ms, style)` (a style or a CLDR pattern
  such as `"EEEE d MMMM"`, in local time or `"utc"`), `formatRelative(-1, "day")` ("yesterday"), `formatList(list)`
  ("a, b and c"), and `compareText(a, b)` and `sortText(list)`, which order text for people (letters, then accents,
  then case). The data covers en, it, fr, de, es, pt, ja, zh, ar and he; another language formats as English.
- **Direction:** Arabic and Hebrew are right to left; nodes with `"dir": "auto"` follow it.
- **The checker** reports a `t()` key no locale has and parameters a message doesn't read or that aren't given.

### Dates

Calendar dates are ISO 8601 texts, `"YYYY-MM-DD"`, from year 1 to 9999, so they sort as text, compare with `<` and
are stored in the state as they are shown to a `calendar`. The functions give null for a text that isn't a date:
- `today()`: the local date. Screens that must render the same every day give "today" in the state instead.
- `addDays(date, n)` and `addMonths(date, n)` (the day kept within the month: January 31 plus one month is the last
  day of February); `daysBetween(from, to)`, negative if `to` is earlier.
- `weekday(date)` (0 for Sunday to 6 for Saturday) and `daysInMonth(date)`.
- `dateOf(ms)` (the UTC date a time falls on) and `dateMs(date)` (its midnight, UTC), to use `formatDate(ms, style,
  'utc')`; `isDate(text)`.

## Events

A handler is an action name, a step, or a list of steps. Pointer events get `event.x`/`y` (in the window),
`event.left`/`top` (in the handling node) and its `width`/`height`.

<!-- generated: events -->
| Handler | When |
|---|---|
| `onClick`, `onDoubleClick`, `onContextMenu` | Left click, two clicks on the same node, right click (or Shift+F10 or the Menu key on the focused node). |
| `onPress`, `onRelease` | Left button down, up. |
| `onDrag` | The pointer moves while pressed: `event.dx`, `event.dy` since the press. |
| `onDrop` | A dragged node was dropped here: `event.data` (on `tabs`, also `event.index`, where it goes; on a `list` or `tree`, `event.index`, `id` and `place`: `"before"`, `"inside"` or `"after"` that row); or files from the OS: `event.files` (their paths). |
| `onHover`, `onLeave` | The pointer enters or leaves the node (or a child). |
| `onFocus`, `onBlur` | Keyboard focus. |
| `onScroll` | `event.dx`, `event.dy`. |
| `onKey` | A key on the focused node: `event.key`, `shortcut`, `shift`, `ctrl`, `alt`, `cmd`. |
| `onChange` | A bound or value control changed: `event.value`; a timeline's track muted or locked: `event.type` `"track"`, `id`, `muted`, `locked`. |
| `onSubmit` | Enter in a text area: `event.text`. |
| `onLink` | A press on a part of `runs` with a link: `event.link`. |
| `onShow`, `onHide` | The node was built, or removed (by `when`, `repeat` or a revision). |
| `onClose` | A tab's close button or Delete: `event.id`. |
| `onMove` | A tab dragged along its strip: `event.id`, `from`, `to`; a graph's nodes dragged or moved with the arrows: `event.ids`, `dx`, `dy` (in graph units) and `nodes`, each `{ id, index, x, y }`. |
| `onExpand` | A tree row or a timeline's group opened or closed: `event.id`, `expanded`. |
| `onOpen` | Enter or a double click on a tree, list or table row: `event.id`. |
| `onSort` | A table header clicked: `event.column`, `descending`. |
| `onColumnResize` | A table column's edge dragged and let go: `event.column`, `width`. |
| `onTransform` | Canvas items moved, resized, rotated or nudged: `event.items`, each `{ "id", "x", "y", "width", "height", "rotate" }`, where they should go; the document applies them. |
| `onConnect` | A graph's link made, by a drag from a port or Enter on two ports: `event.from`, `output`, `to`, `input`. |
| `onDisconnect` | A graph's link removed (Shift+Delete on its port) or its end picked up: `event.index`, `from`, `output`, `to`, `input`. Moving an end runs `onDisconnect` then `onConnect`. |
| `onLinkDrop` | A graph's link dropped on empty space: `event.node`, `port`, `output` (whether it started at an output), `x`, `y` (in graph units), e.g. to add a node there. |
| `onSelect` | A graph's or a timeline's selection changed: `event.ids` (a timeline's item keys). |
| `onDelete` | Delete or Backspace on a graph: `event.ids`, the selected nodes. |
| `onScrub` | A timeline's playhead moved by the user: `event.time` (seconds), `frame`. |
| `onKeyframeMove` | A timeline's keyframe dragged or moved with Alt and the arrows: `event.track` (its id), `index`, `time`, `from`, `value`. |
| `onClipChange` | A timeline's clip moved or trimmed: `event.id`, `track` (where it is now), `fromTrack`, `index` (in `fromTrack`), `start`, `duration`, `from` (the start before) and `edge` (`"move"`, `"start"` or `"end"`). |
| `onGutterClick` | A press in an editor's gutter: `event.line` (from 0), `shift`, `alt`, `control`, `super`. While a node has one, the press doesn't move the caret. |
| `onWordHover` | The pointer moved onto a word of an editor, or off one: `event.word`, `line`, `column` (where it starts, from 0); off a word, `word` is empty and `line` null. |
| `onType` | Typing in an editor that has a `language` or not: `event.text` (what was typed; `""` for a deletion), `word` (the word that now ends at the caret, by the language's `wordPattern`), `line`, `column` (the caret, from 0), and `x`, `y`, `top` (the caret's box, for a popup). |
| `onTextHover` | The pointer moved over an editor's text: `event.over` (`false` when it left the text), `line`, `column` (from 0), and `x`, `y`, `top` (the place's box). For hover cards, e.g. a language server's. |
| `onPopupKey` | A popup key while an editor's `popup` is `true`: `event.key` is `up`, `down`, `pageUp`, `pageDown`, `enter`, `tab` or `escape`. |
<!-- end generated -->

**Bubbling:** click, double click, context menu, press, release, drag, scroll and key events go to the node or, if it
has no handler, its nearest ancestor with one. A row with `onClick` needs no invisible overlay above its labels.

**Shortcuts:** `"keys"` on the document (anywhere) or on a node (while it or a child is focused): `"mod+k"`,
`"cmd+shift+p"`, `"escape"`, `"up"`, `"f5"`. `mod` is Cmd on macOS, Ctrl elsewhere. Besides letters, digits and
`f1` to `f25`, the keys are `enter`, `escape`, `tab`, `backspace`, `delete`, `space`, the arrows, `home`, `end`,
`pageup`, `pagedown`, `minus`, `equal`, `comma`, `period`, `slash`, `backslash`, `semicolon`, `apostrophe`,
`leftbracket`, `rightbracket`, `grave`, and the keypad's `numpad0` to `numpad9`, `numpadperiod`, `numpadplus`,
`numpadminus` and `numpadmultiply` (its Enter is `enter`). While a text area is focused, keys
without Cmd, Ctrl or Alt go to the text (Escape still works).

### Commands and keybindings

**Commands:** an action may be an object that describes itself: `{ "title": "Save", "category": "File", "icon":
"save", "enablement": "{{state.dirty}}", "description": "...", "do": "write" }`. `do` is an action name or steps. An
object with both step keys and command keys is an error. `enablement` is checked only when a key, a menu or a palette
runs the command; a `run` step and the application's code ignore it.

**Chords and bindings:** a key may be a chord of up to 4 presses separated by spaces: `"mod+k mod+s"`. A key's value
is a handler, a binding `{ "when": "{{...}}", "do": "save", "with": { "tab": 2 } }` (`with` gives the steps local
variables), or a list of bindings, where the first that applies runs.

**Resolution:** one walk per press.
1. A focused text area keeps plain keys, except Escape, the function keys (F1 to F25) and the keys of the focused
   node and its ancestors.
2. During a chord, a press that doesn't continue it ends it and is dropped; Escape ends it.
3. The scopes go from the focused node to the root, then to the document's `keys`.
4. A binding is skipped when its `when` is false or its command is disabled.
5. A press that starts a chord waits for the next one.
6. If nothing matched, `onKey` bubbles.

**Variables:**
- `context`: `focus` (the focused node's id), `focusPath` (the ids from the root), `textInputFocus`, `overlay` (one is
  open), `chord` (the presses so far, or null), `platform` (`"mac"`, `"windows"` or `"linux"`), and the keys of the
  node `context` of the focused node and its ancestors (an inner node wins).
- `commands`: `{ name, title, category, icon, description, enabled, key, keyLabel }`, sorted by category, then title.
- `keybindings`: `{ key, keyLabel, command, title, when, scope }`, the document's first (`"screen"`), then by node
  (`"#id"`) and key.

The checker reports a key bound twice in a scope with the same `when`, a single press that a chord starting with it
shadows, an unknown action in `do`, and a `when` it can't read.

### Menus and settings

**Menus:** `menus` names menus, each a map of keyed items. An item runs an action (`"run"`, with `"with"`) or opens
another menu (`"submenu"`), and may have a `group`, an `order` within it, a `when`, a `title` and an `icon`:
```json
"menus": { "file": { "save": { "run": "save", "group": "2_save" },
                     "new": { "run": "add", "title": "New", "group": "1_new", "order": 1 } } }
```
The `menus` variable gives each menu as a list ready for the `menu` and `menuBar` components: items sorted by group,
then order, then key, a `{ "separator": true }` between groups, items whose `when` is false left out. An item that runs
a command takes its title, icon, `enabled` and `key`/`keyLabel` from the command, and a submenu is a nested `submenu`
list. Extensions add items to a menu by key, so a key is the item's identity.

**The native menu bar:** `"window": { "menuBar": [{ "menu": "file", "title": "File" }] }` shows those menus in the
system's menu bar, kept up to date as the `menus` variable changes; choosing an item runs it like the drawn menus.
macOS shows them at the top of the screen (after the app's own menu; shortcuts are shown by the drawn menus only, and
run through the keybindings), Windows in the window with their key labels. Elsewhere there is no native bar:
`window.nativeMenuBar` is false, so a screen can draw its own with `"when": "!window.nativeMenuBar"`.

**The desktop:** files dropped on the window from the OS reach the node under the pointer with `onDrop` (or its
nearest ancestor with one) as `event.files`, their paths. `window.colorScheme` follows the system's light or dark mode
(macOS and Windows; `"light"` elsewhere) and `window.scale` the display the window is on; both update while the app
runs. Several windows are several screens, one per window.

**Settings:** `settings` maps a key such as `"editor.fontSize"` to a schema (the subset of Typed state) with a
required `default`, and `title` and `description` for people. Values the user changed are stored flat in
`state.settings` (`{ "editor.fontSize": 18 }`), so the defaults stay in the document. The variables:
- `settings`: every value, nested by the dots (`settings.editor.fontSize`), the default where unchanged;
- `settingsList`: `{ key, title, description, type, value, default, modified, enum, minimum, maximum }` by key.

Steps write `settings.editor.fontSize` (`set`, `toggle`, `bind`, ...); a value that doesn't match the schema is an
error and isn't stored. `remove` resets a setting to its default. The checker knows the settings' types and reports
writes to unknown settings. The built-in `settingsPage` component lists them with a search field (`search`, a
state path for its text in `query`), a control per type and a "Reset" on modified ones.

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
| `if` | `{ "if": "{{length(state.draft) == 0}}", "then": [...], "else": [...] }` |
| `forEach` | `{ "forEach": "state.items", "as": "item", "index": "i", "do": [...] }` |
| `while` | `{ "while": "{{state.n < 10}}", "do": [...] }` (at most 10,000 iterations) |
| `run` | `{ "run": "moveCard", "with": { "from": "{{c}}", "to": "{{c + 1}}" } }`: an action with variables; the name may be computed, and `with` too (`"{{item.with}}"`, an object or null) |
| `call` | `{ "call": "pickFile", "with": {...}, "into": "state.file", "onError": [...] }`: a native function (`Screen::onCall`); `error` in `onError`, `result` in `then`. A slow one (`Screen::onAsyncCall`) runs on a worker thread: `"loading": "state.busy"` is true while it runs, `"progress": "state.done"` goes from 0 to 1, partial results stream into `into`, `"timeout": 5000` ends it with the error `timeout`, `then` runs with `result` |
| `cancel` | `{ "cancel": "search" }`: stop the running slow calls of a function; they change nothing more |
| `invoke` | `{ "invoke": "files.read", "with": {"path": "a.txt"}, "into": "state.text" }`: a method of a declared service, answered later; `loading`, `progress`, `timeout` (ms, default 30000), `id` (a new call with the same id cancels it), `onDone` with `result`, `onError` with `error` (`{code, message, data}`); a stream's events go to `append` (the last `keep`) or run `onEvent` with `events` |
| `request` | `{ "request": "api.getProject", "with": {"id": "{{state.id}}"}, "into": "state.project", "loading": "state.busy" }`: an operation (or a WebSocket message) of a declared connection, built from its declaration; the keys of `invoke`, with `error` as `{code, message, status, body}`. In a preview only a read without a `mock` reaches the server: the others answer with their mock, or fail with `preview` |
| `subscribe` | `{ "subscribe": "build.output", "append": "state.log", "keep": 500, "id": "build" }`: a stream of a declared service or connection until `cancel` or its node goes; `do` runs with `events` |
| `respond` | `{ "respond": "{{state.answer}}" }` or `{ "respond": null, "error": "no" }`: answers the service request that ran the action `service:method` (its `request`); a control step |
| `signIn` | `{ "signIn": "api", "onDone": [...], "onError": [...] }`: signs in to the provider a connection's `auth` declares (OAuth 2: the browser with PKCE, or a device code), then its requests carry the token. The token never reaches the document or the state: the `auth` variable shows `{state, account, expires}`, and `error` in `onError` is `{code, message}`. A control step |
| `signOut` | `{ "signOut": "api" }`: forgets the sign-in and its kept token; a control step |
| `chooseFolder` | `{ "chooseFolder": "workspace", "onDone": [...] }`: after the event, the person chooses the folder a root names in the OS's dialog (if the host lets them); its services start again on it. `result` is `{name}` (never where it is), `error` `{code, message}`, a cancel runs nothing. A control step |
| `emit` | `{ "emit": "saved", "data": {...} }`: an event for every connected peer |
| `send` | `{ "send": "host", "data": {...} }`: a message on a declared channel |
| `connect` | `{ "connect": "host" }`: connect a declared channel now |
| `ask` | `{ "ask": "{{state.draft}}", "into": "state.asked" }`: asks the agents in plain words, with `options` (an object) passed on to them; replies run `ask:reply`, never as the person's decision (Asking the agents). A control step |
| `cancelAsk` | `{ "cancelAsk": "{{state.asked}}" }`: stops requests of `ask` still in progress (an id or a list): they become `cancelled`, the agents are told, and their later replies are dropped. A control step |
| `attach` | `{ "attach": { "accept": ["audio/*"] }, "into": "state.files" }`: the person's files as attachments (Assets). A control step |
| `patch` | `{ "patch": [JSON Patch ops], "message": "..." }`: the app revises its own document |
| `navigate` | `{ "navigate": "settings" }` or `"back"` |
| `focus`, `scrollTo` | `{ "focus": "#input" }` (a container gives it to its first focusable node), `{ "scrollTo": "#row7" }` (after the update, waiting up to two frames for a node that is still being laid out) |
| `edit` | `{ "edit": "sortLines", "in": "#code" }`: a code editor's command (Editor steps) |
| `dismiss` | `{ "dismiss": true }`: closes the open overlays, e.g. a menu once an item is chosen |
| `after` | `{ "after": 2000, "do": [...] }`: later |
| `debounce` | `{ "debounce": 300, "do": [...] }`: later, unless run again in the meantime (search boxes) |
| `animate` | `{ "animate": "state.progress", "to": 100, "ms": 300, "easing": "out" }` (`linear`, `in`, `out`, `inOut`) |
| `copy` | `{ "copy": "{{state.link}}" }`: to the clipboard |
| `log`, `note`, `stop` | `{ "log": "n is {{state.n}}" }`, a comment, `{ "stop": true }` (end the action) |
| `preview`, `endPreview`, `accept`, `reject`, `restore`, `revert`, `undo` | Review and history, e.g. `{ "accept": "{{proposals[0].id}}", "head": "{{proposals[0].head}}" }` (`accept` names the head the user reviewed; refused if the proposal moved since) |
| `image` | `{ "image": "undo", "of": "canvas" }`: runs an image operation (see Images) |
| `audio` | `{ "audio": "play", "of": "song" }`: see Audio |
| `insert` | `{ "insert": "songs", "value": { "title": "{{state.draft}}" }, "into": "state.added" }`: stores a record, checked against its model; `result` in `then` is the record, `error` in `onError` is `{code, message}` (see Data: models and records) |
| `update` | `{ "update": "songs", "id": "{{song.id}}", "fields": { "rating": 5 } }`: changes fields of records (an `id`, a list of them, or a `where`) |
| `delete` | `{ "delete": "songs", "id": "{{song.id}}" }`: deletes records (an `id`, a list, or a `where`) |
| `undoData`, `redoData` | `{ "undoData": true }`, `{ "redoData": true }`: undoes or redoes the last write step's records |
| `importCsv` | `{ "importCsv": "songs", "file": "songs.csv", "columns": { "title": "Title" }, "into": "state.report" }`: imports a CSV's rows as records (`file` in the screen's folder, or `text`); `result` is `{imported, failed, failures}` |
| `camera` | `{ "camera": "#viewport", "view": "front" }`: moves a scene's camera (`view`, `frame`, `projection`, `orbit`, `zoom`: see `scene`) |
| `transform` | `{ "transform": "#viewport", "mode": "translate" }`: a scene's modal move of its selection, after the pointer until a click; `axis` locks it, `value` types its amount (units, degrees or a factor; null follows the pointer again), `end` (confirm, cancel) ends it |
<!-- end generated -->

### Control steps and paths

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

## Images

A screen can edit raster images: paint and erase, select, crop, work in layers, apply filters, undo and redo. The
`images` section declares them; `image` steps run operations on them; a `raster` node shows one and plays the pointer
through its current tool. The section is a capability: revisions can't change it, and its `limits` can only lower the
library's.

| Key of an image | Meaning |
|---|---|
| `width`, `height` | Required. Its size in pixels, from 1 to the `side` limit (and at most `pixels` in all). |
| `format` | `"rgba8"` (the default) or `"rgba16f"`. |
| `background` | Its first layer: `"#rrggbb"` (the default: white) or `"transparent"`. |
| `into` | A literal state path, e.g. `"state.drawing"`: where its snapshot is published after each change. |
| `tools` | `name → { "gesture": "paint", ...options }`, at most 64: the tools a screen offers, each a gesture of the library with its options (those of `setOptions`). Without it, every gesture is a tool of its own name. |
| `tool` | The tool chosen first. Default: the first painting tool. |
| `limits` | `name → whole number`: lowers a library limit for this image (see the names below). |

An image is made the first time a step or a node names it. The `image` step runs one operation on it, then publishes
its snapshot to `into` and tells the raster nodes that show it:

```json
{ "image": "pointer", "of": "board", "with": { "phase": "press", "x": 10, "y": 20 }, "onError": [...] }
```

`of` names the image, and may be left out when the screen declares only one. A failed operation runs `onError` with
`error` (its message); without `onError` the step fails at its place.

### Image operations

The operations, and the keys of `with` each reads:

<!-- generated: imageOperations -->
| Operation | Reads from `with` | What it does |
|---|---|---|
| `newImage` | `width`, `height`, `format`, `background` | Replaces the image with a new one of one layer. |
| `open` | `projectExtension`, `asset` | Opens a project, PNG or JPEG file the person chooses, or an asset (PNG, JPEG or SVG) the screen has. |
| `save` | `format`, `extension` | Saves the image where the person chooses, and keeps it for revert. |
| `revert` | — | Opens again what was last saved or opened. |
| `undo` | — | Takes back the last step. |
| `redo` | — | Does again the step last taken back. |
| `history` | `position` | Goes to a place in the history (0 is before the first step). |
| `clearHistory` | — | Forgets every step of the history. |
| `addLayer` | `kind`, `name` | Adds a pixel layer, a group or an adjustment layer. |
| `deleteLayer` | `id` | Deletes a layer (a group with its layers). |
| `duplicateLayer` | `id` | Duplicates a layer. |
| `selectLayer` | `id` | Makes a layer the active one. |
| `addMask` | `id` | Adds a mask to a layer. |
| `deleteMask` | `id` | Deletes a layer's mask. |
| `moveLayer` | `id`, `onto`, `by` | Moves a layer onto another's place, or a step up or down. |
| `setLayer` | `id`, `name`, `visible`, `opacity`, `blend`, `lockPixels`, `lockPosition`, `lockAll`, `clipped`, `maskEnabled`, `adjustment` | Changes a layer's settings. |
| `select` | `kind`, `mode`, `from`, `to`, `points`, `x`, `y`, `color`, `fuzziness`, `allLayers` | Selects all, none, the inverse, a rectangle, an ellipse, a polygon, by magic wand or by colour range. |
| `feather` | `radius` | Softens the selection's edge. |
| `fillSelection` | `color`, `mask` | Fills the selection of the active layer with a colour. |
| `crop` | `x`, `y`, `width`, `height` | Keeps a rectangle of the image, and forgets the history. |
| `previewFilter` | `kind`, `radius`, `amount`, `threshold`, `angle`, `distance`, `monochrome`, `seed`, `name`, `mask` | Previews a filter on the active layer until it is applied or cancelled. |
| `applyFilter` | `kind`, `radius`, `amount`, `threshold`, `angle`, `distance`, `monochrome`, `seed`, `name`, `mask` | Applies the filter previewed, or the one given, as one step. |
| `cancelFilter` | — | Drops the filter's preview. |
| `snapshot` | — | Changes nothing; publishes the image's snapshot. |
| `fit` | — | Zooms and centres the view on the whole image. |
| `zoom` | `by`, `to`, `x`, `y` | Multiplies or sets the zoom, and centres the view on a point. |
| `setTool` | `tool` | Chooses one of the document's tools, with its options. |
| `setOptions` | `preset`, `size`, `sizeStep`, `hardness`, `spacing`, `opacity`, `flow`, `mode`, `tolerance`, `contiguous`, `allLayers`, `fillOpacity`, `shape`, `gradientPreset`, `gradient`, `foreground`, `background`, `swap`, `reset`, `quickMask`, `cloneSource` | Sets the tools' options and the colours. |
| `setProfile` | `working`, `display`, `proof` | Sets the working, display or proof colour profile. |
| `pointer` | `phase`, `x`, `y`, `button`, `pressure`, `scroll`, `modifiers`, `points` | Plays pointer events (document pixels) through the current tool. |
<!-- end generated -->

Points are in document pixels, `{ "x": 10, "y": 20 }`. `select` takes a `kind` (`all`, `none`, `invert`, `rectangle`
and `ellipse` from `from` to `to`, `polygon` through `points`, `wand` at `x`, `y`, `colorRange` of `color` within
`fuzziness`) and a `mode` (`replace`, `add`, `subtract`, `intersect`). `moveLayer` moves `id` onto the place of `onto`,
or `by` places up (positive) or down. Filters and `fillSelection` change the active layer's pixels, or its mask with
`"mask": true`. `zoom` multiplies the zoom `by` a factor or sets it `to` one, centred on `x`, `y`. `setProfile` takes
`srgb`, `displayP3`, `wideRgb` or `gray`. `pointer` takes one event (`phase`: `press`, `move`, `release` or `scroll`;
`x`, `y`; `button`: `left` or `right`; `pressure` from 0 to 1; `scroll`; `modifiers`: a list of `shift`, `control`,
`alt`, `super` and `space`, which pans whatever the tool) or `points`, a list of up to 4096 such events. `open`, `save`
and `revert` go through the file dialogs the person sees: a document never names a file.

### Image values and the snapshot

The names the operations and tool options take, and the library's limits:

<!-- generated: imageNames -->
| Value | Names |
|---|---|
| A tool's `gesture` | `move`, `select-rectangle`, `select-ellipse`, `lasso`, `polygon-lasso`, `magic-wand`, `crop`, `paint`, `erase`, `clone`, `gradient`, `fill`, `sample`, `pan`, `zoom` |
| A layer's `blend` | `normal`, `multiply`, `screen`, `overlay`, `soft-light`, `darken`, `lighten`, `difference`, `hue`, `saturation`, `color`, `luminosity` |
| `addLayer`'s `kind`, besides `pixel` and `group` | `levels`, `curves`, `hue-saturation`, `color-balance`, `brightness-contrast`, `invert`, `threshold`, `gradient-map` |
| A filter's `kind` (`previewFilter`, `applyFilter`) | `gaussian-blur`, `sharpen`, `unsharp-mask`, `add-noise`, `motion-blur` |
| The `shape` tool option (gradients) | `linear`, `radial`, `angle`, `reflected`, `diamond` |
| `limits`, with the library's values | `side` 32768, `pixels` 1073741824, `layers` 256, `groupDepth` 2, `cacheBytes` 536870912, `gpuBytes` 268435456, `undoBytes` 536870912, `undoSteps` 200, `filterRadius` 250, `brushSize` 5000, `imageFileBytes` 268435456, `imageFileSide` 30000, `projectFileBytes` 4294967296, `manifestBytes` 16777216, `profileBytes` 4194304, `floodPixels` 67108864, `polygonPoints` 100000 |
<!-- end generated -->

The snapshot has `width`, `height`, `format`, `profile`, `display`, `proof`, `revision`, `active` (the active layer's
id), `layers` (each with `id`, `name`, `label`, `depth`, `kind`, `visible`, `opacity`, `blend`, `lockPixels`,
`lockPosition`, `lockAll`, `clipped`, `mask`, `adjustment` and `thumbnail`, an image name such as
`"board/layer/3"` that an `image` node shows), `history` (`id`, `label`), `position`, `dropped`, `canUndo`,
`canRedo`, `memory`, `selection`, `bounds`, `foreground`, `background`, `tool`, `gesture`, `options`, `view`
(`zoom`, `x`, `y`), `message` and `previewing`; `"<image>/navigator"` is a thumbnail of the whole image.

### An image example

A whiteboard with a marker, an eraser and undo:

```json
"images": { "board": { "width": 1600, "height": 900, "into": "state.board",
                       "tools": { "marker": { "gesture": "paint", "size": 6, "hardness": 100, "foreground": "#1d4ed8" },
                                  "eraser": { "gesture": "erase", "size": 40 } } } },
"root": { "type": "column", "children": [
  { "type": "row", "children": [
    { "text": "Marker", "onClick": [{ "image": "setTool", "with": { "tool": "marker" } }] },
    { "text": "Eraser", "onClick": [{ "image": "setTool", "with": { "tool": "eraser" } }] },
    { "text": "Undo", "disabled": "{{!state.board.canUndo}}", "onClick": [{ "image": "undo" }] },
    { "text": "Clear", "onClick": [{ "image": "select", "with": { "kind": "all" } },
                                   { "image": "fillSelection", "with": { "color": "#ffffff" } },
                                   { "image": "select", "with": { "kind": "none" } }] } ] },
  { "type": "raster", "image": "board", "grow": 1 } ] }
```

## Audio

A screen can play sound: music, effects, a podcast, speech, a stream from a server. The `audio` section declares the
sounds, each from one source; `audio` steps play and change them; each sound's state is written to its `into` path,
and what happens to it runs actions. An agent can play, pause or seek what the screen declares. A revision may also
give a sound a `file` (one of the screen's, or an asset: `"asset:beat.wav"`) or a `tone`, never a `url` or a host's
`source`, and only the person accepts that change (Assets).

| Key of a sound | Meaning |
|---|---|
| `file` | A WAV, FLAC or MP3 file, relative to the screen's folder: inside it, or inside a folder the host allows (`Screen::allowAudioDirectory`). Decoded as it plays, never read whole. A sound description (`.sound.json`, below) too, and `"asset:<name>"`: an asset of the screen. |
| `url` | An `https://` URL (`http://` to the loopback only) the host's network allows (`network.allow`), streamed as it downloads; `maxBytes` (1 KiB to 64 MiB, 8 MiB by default) bounds what is buffered. |
| `source` | A source the host gives by name (`Screen::setAudioSource`): a stream the application feeds (speech, a call, its own decoder), or anything else it makes. |
| `tone` | `{ "frequency": 440, "seconds": 1, "amplitude": 0.5 }`: a sine, with short ramps at its ends. |
| `into` | A literal state path, e.g. `"state.song"`: where the sound's state is written. |
| `volume`, `pan`, `rate`, `loop` | How it starts: a gain from 0 to 4, a balance from -1 to 1, a playback rate from 0.25 to 4 (speed and pitch), and whether it starts again at its end. |
| `updates` | Milliseconds between position updates while it plays: 50 to 5,000, 250 by default. |

A sound has exactly one of `file`, `url`, `source` and `tone`. Its state is `{status, playing, position, duration,
progress, volume, pan, rate, loop, level, underruns, error}`: `status` is `stopped`, `playing`, `paused` or `error`;
`position` and `duration` are seconds (`duration` is null for a live stream), `progress` goes from 0 to 1, and `level`
is the sound's peak over the last block mixed (0 to 1), for meters. The step:

```json
{ "audio": "play", "of": "song", "from": 30, "onError": [{ "set": "state.problem", "to": "{{error}}" }] }
```

| Operation | Reads | What it does |
|---|---|---|
| `play` | `from` (seconds) | Plays, from where it is (from the start after its end). A stream that ended starts again from its source. |
| `pause` | — | Holds it where it is. |
| `toggle` | — | Plays, or pauses if it plays. |
| `stop` | `ms` | Stops it and goes back to the start; with `ms`, fades out first. |
| `seek` | `to` (seconds) | Moves it (a file, bytes or a tone; not a live stream). |
| `volume` | `to` (0 to 4), `ms` | Sets its gain, over `ms` if given. |
| `pan`, `rate`, `loop` | `to` | Sets its balance, its playback rate, whether it loops. |
| `master` | `to` (0 to 4) | Sets the gain of every sound (no `of`). |

`of` names the sound, and may be left out when the screen declares only one; the operation may be computed
(`"{{state.on ? 'play' : 'pause'}}"`). The actions `name:started`, `name:ended` (at its end, not after `stop`),
`name:underrun` (a stream ran dry) and `name:error` run with `event` (`position`, `message`). What can't play (the host
gives the screen no audio, a file outside the allowed folders, a URL the network doesn't allow) shows in the sound's
`error`, runs the step's `onError` with `error`, and is logged otherwise: the action goes on, as a channel's does
without its host. A host chooses to give a screen sound (`Screen::audio()`); the rendering check, scenarios played
headless and pictures of a screen get none, so they stay silent and the same every time.

A music player's controls:

```json
"audio": { "song": { "file": "music/track.mp3", "into": "state.song" } },
"actions": { "song:ended": [{ "run": "next" }] },
"root": { "type": "row", "children": [
  { "text": "{{state.song.playing ? 'Pause' : 'Play'}}", "onClick": [{ "audio": "toggle", "of": "song" }] },
  { "type": "slider", "max": "{{state.song.duration ?? 0}}", "value": "{{state.song.position}}",
    "onChange": [{ "audio": "seek", "of": "song", "to": "{{event.value}}" }] } ] }
```

### Sound descriptions
An agent can't write a recording's bytes, but it can write text: a **sound description** is to a sound what SVG is to a
picture. It is JSON (`application/vnd.affrescoui.sound+json`, a name ending `.sound.json`), an asset or a file of the
screen's folder, and the audio module renders it as it plays, the same samples every time:

```json
{ "tempo": 96, "tracks": [
  { "instrument": "kick",  "steps": "x...x...x...x..." },
  { "instrument": "hat",   "steps": "..x...x...x...x.", "volume": 0.5, "pan": 0.3 },
  { "instrument": "snare", "steps": "....x.......x..." },
  { "instrument": "triangle", "release": 0.3, "notes": [
    { "at": 0, "note": "C4", "length": 2 }, { "at": 2, "note": "E4", "length": 2 }, { "at": 4, "note": 67 } ] } ] }
```

- Times are in beats of `tempo` (20 to 300 a minute; 120 by default). `length` (beats) is how long it lasts; by
  default, until its last event has faded. A sound's `loop` plays it again.
- `instrument`: `sine`, `square`, `saw`, `triangle` and `noise`, shaped by an envelope (`attack`, `decay`, `release`
  in seconds, 0 to 5; `sustain` 0 to 1), or the drums `kick`, `snare`, `hat` and `clap`. `volume` (0 to 1, 0.8) and
  `pan` (-1 to 1) per track.
- `notes`: `{ "at", "note", "length", "velocity" }`: `at` and `length` in beats (`length` 1 by default), `note` a name
  (`"C4"`, `"F#3"`, `"Bb2"`) or a MIDI number (0 to 127), `velocity` 0 to 1. `steps`: a pattern, one character a step
  (`x` a hit, `X` an accented one, `.` or `-` a rest), each `step` beats long (0.25 by default), from `at`; `note`
  gives its pitch.
- Bounds: 1 MiB of text, 16 tracks, 4,096 notes and hits, 32 sounding at once, 10 minutes. One beyond them, or
  malformed, is refused when a revision brings it, with its place (`tracks[1].notes[3].note: ...`).

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
disconnect.
A `"socket"` that starts with `ws://` or `wss://` is a remote peer instead (a `PeerServer`, `modules/net/README.md`):
`wss://` anywhere, `ws://` on the loopback only. The token is never in the file: the program that shows the screen
gives it (`Screen::connectChannel()`), and
without one, the server refuses the connection and `name:closed` runs. In a browser this is the only kind of channel.

`"persist": ["state.habits"]` keeps those state values in `~/.affrescoui/<file name>.state.json` (on Windows,
`%APPDATA%\affrescoui\<file name>.state.json`), loaded at start and written when they change. A folder's
`index.screen.json` is named after its folder (`flow/index.screen.json` keeps `flow.state.json`). A screen opened from
a file keeps its version-control history in the same folder, in `<file name>.history/` (VERSION_CONTROL.md,
"History on disk"), and its records in `<file name>.data.sqlite` (Data: models and records). `AFFRESCOUI_DATA_DIR`
(or the host's `--data`) puts them all
in another folder.

`"services"` reach the folders and programs the host grants ([`modules/services/README.md`](modules/services/README.md)
has each provider's methods): `{"apps": {"provider": "fs", "root": "apps", "copyFrom": ["examples"]}}` reads,
writes, lists and copies under the folder `apps`, and copies from the folder `examples` too
(`{"invoke": "apps.copy", "with": {"from": "examples", "path": "flow", "to": "board", "exclude": ["*.cpp"]}}`, with
`merge` to bring a folder up to date); a folder the host grants read-only takes only `"readOnly": true`. A `process`
service runs the program its grant names, with the arguments the host's rule allows; one the host grants detached is
started and left running, its result `{pid}` (the Launcher opens apps so).

## Data: models and records

An app is four layers, kept apart so that a change of one says what it does to the others (the idea of MVVM over a
database with migrations, as Rails and Django keep their schema):

| Layer | In a screen | Changed by | Lives in |
|---|---|---|---|
| **View** | the layout: nodes, styles, `{{…}}` bindings | revisions | the document |
| **Logic** (the view-model) | actions, `computed`, `queries`, rules, the short-lived `state` (a selection, a panel open) | revisions | the document |
| **Model** (the schema) | `models`: collections, typed fields, references, a `version` | revisions, with a **migration** when the records must change | the document |
| **Data** (the records) | each collection's records | the app's write steps (or a reviewed step an agent proposes), never a view or logic revision | a store of its own: SQLite, `<data folder>/<screen name>.data.sqlite`, with a journal |

So a new view never erases what was stored: a revision can't write records at all, a field the model stops declaring
keeps its values in the store (unread), and reverting a commit leaves the records alone, except that a commit that
migrated them is undone with them (VERSION_CONTROL.md, "Impact per layer"). A screen from text (the rendering check,
scenarios) keeps its records in memory.

### Models
```json
"models": {
  "notes": {
    "version": 2,
    "fields": {
      "title": { "type": "string", "required": true, "maxLength": 200 },
      "kind": { "type": "enum", "values": ["idea", "task"], "default": "idea" },
      "folder": { "type": "reference", "to": "folders", "onDelete": "nullify" },
      "tags": { "type": "list", "of": { "type": "string" }, "maxItems": 16 },
      "due": { "type": "date" }, "done": { "type": "boolean", "default": false }
    },
    "seed": [{ "id": "n1", "title": "First note" }],
    "importFrom": "state.notes",
    "migrations": [{ "from": 1, "to": 2, "steps": [{ "rename": "text", "to": "title" }] }]
  }
}
```
- **Types:** `string` (`minLength`, `maxLength` in characters), `number` and `integer` (`minimum`, `maximum`),
  `boolean`, `date` (`"YYYY-MM-DD"`, as Dates), `dateTime` (ms since 1970, as `now()`), `enum` (`values`),
  `reference` (`to` a collection: a record's id; `onDelete` `restrict`, the default, refuses to delete what is
  referred to, `nullify` clears the field), `list` (`of` a type; `minItems`, `maxItems`), `object` (`fields`), `map`
  (any keys, each value `of` a type; `maxItems`). Any field takes `required`, `default`, `title`, `description`;
  a top-level single value may be `unique`. Every record also has `id` (given, or `"1"`, `"2"`, ...), `created` and
  `updated` (ms).
- **Every write is checked** against the model: a record with a missing required field, a value of another type, an
  unknown field, a dangling reference or a duplicate unique value never gets in, and the write's `onError` gets why.
- **`seed`**: the records a collection starts with, once, when the store first creates it. A seed is written for the
  model's first version and migrated like stored records, so a migration never needs it rewritten.
- **`importFrom`**: a state path (or `{"path", "with": "{{merge(it, {position: index})}}"}`): when the store first
  creates the collection and the screen's old `persist` file holds that path, its items come in as records once
  (instead of the seed), after the file is copied to `<state file>.before-data.json`; items that don't fit stay there.
- **`queries`**: named reads of a collection, run by the store: `{"open": {"from": "notes", "where": {"done": false,
  "due": {"<=": "{{state.today}}"}}, "sort": ["due", {"field": "title", "descending": true}], "limit": 50}}`.
  `where` compares fields with `==` (the default), `!=`, `<`, `<=`, `>`, `>=`, `in` (a list), `contains` (a text, or a
  list field's item) and `startsWith`; `"any": [{...}, {...}]` holds when one does; `"count": true` gives a number.
  Values may be bindings, so a query follows the state.
- **Reading:** `data.<collection>` is a collection's records in the order they were stored, `data.<query>` a query's
  results; at most 10,000 records each. A list or table shows only the rows in view. `data` changes after every write,
  and only what reads it is evaluated again.

### Write steps
| Step | Does |
|---|---|
| `{ "insert": "notes", "value": {...}, "into": "state.added" }` | Stores a record (`values`: a list of them, all or none); `result` in `then` is the record |
| `{ "update": "notes", "id": "{{note.id}}", "fields": { "done": true } }` | Changes fields (null clears one); `id` may be a list, or `where` a filter |
| `{ "delete": "notes", "id": "{{note.id}}" }` | Deletes records (an `id`, a list, or a `where`) |
| `{ "undoData": true }`, `{ "redoData": true }` | Undoes or redoes the last event's writes (the person's only) |
| `{ "importCsv": "notes", "file": "notes.csv", "columns": { "title": "Title", "due": { "column": "Due", "with": "{{value}}" } } }` | Imports a CSV's rows (`file` in the screen's folder or one the host allows, or `text`): columns match fields by header, or by `columns`; cells convert to the field's type; `atomic` imports all or none; `result` is `{imported, failed, failures}` |

A refused write runs `onError` with `error` (`{code, message}`: `invalid`, `unique`, `reference`, `notFound`,
`model`, `limit`, `denied`); without one, it is reported. The writes of one event are one step of the store's undo, journaled
with who and when, before and after. The steps aren't control steps, but a revision that adds or changes one is always
reviewed, and an author other than the person (the host's `Runtime::setDataAuthor`, for an agent's reviewed step)
writes only what `permissions.data` lets it write at `/data/<collection>`.

### Migrations
A change to a model is **compatible** (a new optional field or one with a default, a new collection, a widened enum,
bound or type), **transforming** (a rename, a new type, a newly required field: the records must change) or
**destructive** (a field or a collection removed, a type narrowed, a drop). A transforming change needs the model's
`version` raised and a migration from the old version to the new one in its `migrations`, in the same revision:
```json
{ "from": 3, "to": 4, "steps": [
  { "rename": "artist", "to": "by" },
  { "convert": "duration", "with": "{{value * 1000}}", "inverse": "{{value / 1000}}" },
  { "fill": "rating", "with": 0 },
  { "split": "name", "into": ["first", "last"], "with": ["{{split(value, ' ')[0]}}", "{{split(value, ' ')[1]}}"],
    "inverse": "{{record.first + ' ' + record.last}}" },
  { "merge": ["street", "city"], "into": "address", "with": "{{record.street + ', ' + record.city}}" },
  { "drop": "legacy" } ] }
```
Expressions see `value` (the field's) and `record`. Before a change is accepted it dry-runs on a copy of the store,
and the review says what it does ("notes: 312 of 312 records migrate (3 to 4)"); if a record doesn't fit, the change
is refused, naming it ("id 17: notes[17].duration: expected an integer"). On accept the store archives every record
the migration changes (its values before, restorable), migrates them in one transaction, checks each against the new
model, and commits, or changes nothing. A destructive change is always reviewed: accepting it confirms it. Migrations
already applied are history: a revision can't change or remove them. Reverting a commit that migrated runs the
inverse of its steps (a rename back, the `inverse` expressions, a drop's values from the archive); without an inverse
it puts the archived records back and archives what was written since that the older model can't hold.

`get_models` (the MCP server) gives each collection's model and count, never a record. Limits: SECURITY.md, "Limits".

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
- `assets` (optional) brings files, and `run` (optional) steps run once when it is accepted; only the person accepts
  a revision with either (Assets).
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
| peer → | `revision` (compact edits or `patch`, `proposal`?, `assets`?, `run`?), `revert` / `restore` (`revision`), `undo` |
| peer → | `getDocument`, `getHistory` (`limit`?), `diff` (`from`, `to` or `proposal`), `blame` (`target`: `#id` or a path), `exportChanges` (`from`, `name`), `getCatalog`, `render` (`proposal`?, `scale`?) |
| reviewer → | `accept` (`proposal`, `head`, `dontAskAgain`?), `reject`, `preview`, `endPreview`, `setMode`, `revoke` |
| peer → other peers | `request` (`id`, `text`) and `reply` (`request`, `text`, `done`?, `error`?), relayed with `from` |
| → all peers | `request` (`id`, `text`, `from`: `"user"`, `source`: `"screen"`, `options`?, `attachments`?) from the screen's own `ask` step, and `cancel` (`request`, `from`, `source`) from its `cancelAsk`; a `reply` to its `id` goes to the screen (`ask:reply`), not to the other peers |

Requests may carry an `"id"`; answers repeat it.

**Limits per peer** (`channel::PeerLimits`; the host may set others). A message over a limit is refused, and the first
refusal in a row is logged:

| Limit | Default | Answer |
|---|---|---|
| One message | 88 MiB (a revision of 64 MiB of assets in base64) | `error` "message too large", without `id` (it isn't read) |
| Messages of any kind | 100 a second, 200 at once | `error` "too many messages", without `id` |
| Changes (`revision`, `restore`, `revert`, `undo`) | 120 a minute, 30 at once | `rejected` "too many changes", with `id` |
| Pictures (`render`), apart from the changes | 120 a minute, 30 at once | `rejected` "too many pictures", with `id` |
| Sent to the peer and not yet read by it (a socket peer) | 64 MiB | the peer is disconnected |

On a socket, the host also asks the operating system which user the program on the other end runs as, and refuses a
program of another user.

### Asking the agents
A screen can ask the agents connected to it in plain words, e.g. from a chat (the assistant add-on in
`examples/shared/addons/assistant/`):

```json
"actions": {
	"send": [{ "ask": "{{state.draft}}", "into": "state.asked" }, { "set": "state.draft", "to": "" }],
	"ask:reply": [{ "push": "state.notes", "value": "{{event.text}}" }]
}
```

- `ask` sends the request to every peer after the event, from the author `user` (`request` in the table above), and
  writes its id (`ask-<milliseconds>-<n>`) `into` a state path. It is a control step. With no peer connected it
  fails at once: `ask:reply` runs with the error `no agent is connected`. `"attachments": "{{state.attached}}"` sends
  what the person attached (an `attach` step's list, or ids) as `[{id, name, type, size, duration?, width?,
  height?}]`, never the bytes.
- `options` (an object, at most 4 KiB as JSON) goes with the request as it is, for the agents to read, e.g.
  `{ "ask": "{{text}}", "options": { "model": "{{state.model}}" } }`: the model the person chose. The screen gives it
  no meaning; an agent ignores what it doesn't know (the examples' agent uses `model` if its provider lists it).
- Each reply runs `ask:reply` with `event` `{request, from, text, done, error, proposals, time}`. `done: false` is a
  progress note (an agent at work); one with no text only marks the request `working`, with no reply. `proposals`
  lists the ids of the proposals the replier (any author of the same channel: `agent/...`) opened or changed since the
  request, each once per head, so the screen can show them for review with `preview`, `accept` and `reject`.
- `{ "cancelAsk": "{{state.asked}}" }` (an id or a list) stops requests still `sent` or `working`: they become
  `cancelled`, the peers get `cancel` (`request`, `from`: `"user"`, `source`: `"screen"`), and later replies to them
  are dropped. A control step.
- `requests` lists the requests, oldest first (at most 64, the oldest finished one goes first):
  `{id, text, time, state, replies, proposals}`, with `time` in milliseconds since 1970, `state` `sent`, `working`,
  `answered`, `failed` or `cancelled`, and `replies` (at most 256 each) `{from, text, time, done, error}`. `peers` lists who is
  connected (`author`, `role`).
- **A reply is a reply, never a decision.** While `ask:reply` runs, `accept`, `reject`, `restore`, `revert` and
  `undo` are refused: only the person's own events review. An agent's changes keep their permissions: a proposal
  waits for review as any other.
- Limits: a request is at most 16 KiB; a reply's `text` and `error` are cut to 64 KiB, `from` to 256 bytes; a reply
  to an id the screen didn't send is for the other peers, and fields of the wrong type are ignored.

### Assets
A revision can bring files: a picture an agent drew (an SVG it writes), a sound it described (a `.sound.json`), or a
song or a photo the person attached. They are **assets**, kept with the screen's history, never in its folder:

```json
{ "type": "revision", "message": "A night sea to paint on",
  "assets": [{ "name": "moon-sea.svg", "type": "image/svg+xml", "text": "<svg ...>" },
             { "name": "intro.mp3", "attachment": "att-3-5e1f0c2ab97d" }],
  "set": { "#cover/file": "asset:moon-sea.svg", "/audio/intro": { "file": "asset:intro.mp3", "into": "state.intro" } },
  "run": [{ "image": "open", "of": "canvas", "with": { "asset": "moon-sea.svg" } }] }
```

- Each asset is `{name, type, text}` (SVG, JSON, text, a sound description), `{name, type, data}` (base64) or
  `{name, attachment}`. Names: `[a-z0-9][a-z0-9._-]{0,99}`, no folders, the extension of the type. Types: `image/png`,
  `image/jpeg`, `image/svg+xml`, `audio/wav`, `audio/mpeg`, `audio/flac`, `application/json`, `text/plain`,
  `application/vnd.affrescoui.sound+json`. At most 32 MiB each, 64 MiB a revision, 1,000 a screen. The bytes must be
  what the type says (pictures decode, sounds decode, JSON parses), or the revision is refused before review.
- The screen reads one as `"asset:<name>"`: an `image` node's `file`, a sound's `file`, and the `image` step's
  `open` with `{"asset": name}` (a PNG, JPEG or SVG). The top-level `assets` section names them (`{type, size,
  sha256}`): version control writes it, and the checker reports an `asset:` the screen doesn't have.
- `run`: steps like an action's, run once on the live screen when the proposal is merged, after the update; never in
  its preview, never again on a restore or a revert. They may not be control steps, nor run an action that has one
  (such as one calling the host's native functions): open an asset with the `image` step itself.
- **Only the person accepts** a revision that brings, changes or removes assets, carries `run`, changes the `audio`
  section or makes a node show an asset: whatever the rules, the mode or "Always allow" say, it waits for review, and
  only the screen's own review steps (the person, `user`) may accept it; the proposal lists them (`assets`, `run`,
  `person`).
- **Attachments**: `{ "attach": { "accept": ["audio/*"], "multiple": true }, "into": "state.attached" }` asks the
  person for files in the OS's file dialog (`"from": "{{event.files}}"` in an `onDrop` takes the files they dropped);
  each becomes an attachment, `{id, name, type, size, duration?, width?, height?}`, which `ask` sends to the agents
  (`attachments`) and a revision names. A control step: only the person's input attaches. Attachments wait for this
  run only (32 and 128 MiB at most, the oldest going first).

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
	"version": 2,
	"screen": "index.screen.json",
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
| `{ "key": "mod+z" }` | Presses a key: `enter escape tab backspace delete insert space left right up down home end pageup pagedown slash backslash minus equal comma period semicolon apostrophe leftbracket rightbracket backquote`, `a`–`z`, `0`–`9`, `f1`–`f12`, after any of `shift+ ctrl+ alt+ cmd+ mod+` (`mod` as screens write it: Cmd on macOS, Ctrl elsewhere) |
| `{ "wait": 500 }` | Moves the clock on, running timers, animations and tooltips as they come due. What a handler leaves for the next frame (`focus`) runs at the next wait: `{ "wait": 0 }` |
| `{ "revision": { ... } }` | Applies a revision as the application would |
| `{ "action": "name", "event": { ... } }` | Runs an action; `<channel>:<type>` plays what a channel would send |
| `{ "resize": [960, 720] }` | Resizes the window |
| `{ "expect": ... }` | Checks something (below) |
| `{ "capture": "name" }` | Saves a picture as `<scenario>-<name>.png` (`affrescoui test --out`) |

A target is `"#id"`, the centre of the first shown node with that id (`"nth": 2` picks the third),
`{ "role": "button", "name": "Save" }`, the first shown node with that role and accessible name (`name` is optional;
see `role` and `label`), as a person or a screen reader finds it, or `[x, y]` in window units. Expectations:

| Expectation | Holds when |
|---|---|
| `{ "state": "cards[0].title", "is": "Plan" }` | The live state has that value there (a path as expressions write it, without `state.`; `.length` counts a list or a text) |
| `{ "data": "notes[0].title", "is": "Plan" }` | The records have that value there, as `data.` reads them (a collection or a declared query; a field without a value is null) |
| `{ "node": "#menu", "exists": false }` | A node with that id was built (or not), shown or not |
| `{ "node": "#title", "text": "Done" }` | Its text: a label's, a text area's or an editor's |
| `{ "node": "#box", "property": "color", "is": "#ffffff" }` | A property: `text textColor color borderColor borderWidth shown visible focused hovered tooltip checked value x y width height scrollX scrollY role name description` (positions and sizes in window units, within 0.01; `name` is the accessible name) |
| `{ "node": { "role": "checkbox", "name": "Dark mode" }, "property": "checked", "is": true }` | The same, for the first shown node with that role and name |
| `{ "image": "first-frame" }` | The window, drawn, is pixel for pixel the golden image `<scenario>-first-frame.png` (`affrescoui test --goldens DIR`) |

A failed expectation is reported and the scenario goes on; a step that can't be played (no such node, a refused
revision) ends it. The report is one line per file and one per failure:
`FAILED add-a-card, step 4: state columns[0].cards.length is 4, expected 5`. Limits: 1,000 scenarios a file, 10,000
steps a scenario, a `wait` of 10 minutes, 100,000 bytes typed at once.

## Features from another app: needs and adapters

A feature written for one app can be installed into another. Its manifest declares what it **needs** from whatever
app it is installed into, instead of naming that app's state and actions, and the host declares what it offers: its
`exports` (each with a `kind` and, for a list, an `item` type) and its `points` (a slot, `dock`, `panel`, `menu` or
`toolbar`, and the exports it hands to a feature placed there). `affrescoui contributions <screen>` prints both,
with the installable parts the app ships in `features/*/feature.json`.

```json
"needs": {
  "pages":   { "kind": "list", "item": { "id": "string", "name": "string", "image": "image" } },
  "current": { "kind": "state", "type": "string", "optional": true },
  "reorder": { "kind": "action", "arguments": { "id": "string", "onto": "string" } },
  "dock":    { "kind": "slot", "slot": "dock" }
}
```

A need's `kind` is `value`, `state`, `list`, `action`, `model`, `surface`, `image`, `slot` or `capability`. The
feature reads a bound need as `app.<need>` and runs a bound action as `app.<need>`; its views go in the point bound to
its slot need. A need binds directly to the host's export of the same name and kind, or to its only point of that
slot. Otherwise an **adapter** binds it:

```json
{ "format": "affrescoui-adapter", "version": 1, "feature": "slideSorter", "host": "app:tempera",
  "bind": {
    "pages":   { "value": "{{map(filter(app.doc.layers, it.depth == 0), {id: it.id, name: it.name, image: it.thumbnail})}}" },
    "current": { "export": "doc.active" },
    "reorder": { "action": "layers.move", "with": { "args": { "id": "{{id}}", "onto": "{{onto}}" } } }
  } }
```

A binding is one of `{"export": name}`, `{"value": "{{expression}}"}` (reading only the host's exports as
`app.<export>`), `{"action": export, "with": {...}}` (an action export a point receives, its arguments made from the
need's), or `{"point": name}`. An adapter is expressions only: no steps, at most 64 bindings, 2,048 characters each,
8 levels deep and 64 KiB. It can never give a capability: a `capability` need binds only when the host declares it.

`affrescoui check --compose <screen> <feature> [--adapter <file>]` proves the composition: it binds every need, checks
kinds, slots, ids and imports, refuses a feature that reaches past what it is given or widens permissions or
capabilities, checks the composed document, then plays the feature's scenarios and the host's on it. The proof
(`"format": "affrescoui-proof"`) records the bindings, each check, the scenarios and the base commit; a failed one
names each unmet need and why. An install keeps the record whole on `/features/<name>`, with its `needs`, `bindings`
and `proof` ([Version control](VERSION_CONTROL.md), "Installing across apps").

## Other formats

A screen file may instead hold a document in one of two other formats; the screen loader detects it and converts it
when it loads, and `affrescoui import --from a2ui|adaptive-cards <source> [-o out.screen.json] [--font FILE]
[--title TITLE] [--size WIDTHxHEIGHT] [--revisions FILE]` writes the screen it converts to, to read or edit. Each
adapter follows one recorded version of its format:

| Format | Version | Source |
|---|---|---|
| A2UI | 0.8 | A list of messages, or JSON Lines, one message a line: `surfaceUpdate`, `dataModelUpdate`, `beginRendering`, `deleteSurface` |
| Adaptive Cards | 1.6 | One card, `{"type": "AdaptiveCard", ...}`, with its `$data` |

What maps, and to what:

| Theirs | Ours |
|---|---|
| Layout: `Column`, `Row`, `Card`, `List`, `Divider`, `Tabs` (A2UI); `Container`, `ColumnSet`, `FactSet`, `ImageSet`, `ActionSet` (cards) | `column`, `row`, framed panels, `repeat` over a list, lines; tabs as one section after another |
| `Text`, `TextBlock`, `RichTextBlock` | `label`, sized by `usageHint` or `size` |
| `Image` | `image`: a relative file, or an `https` URL as `source`, which loads only from a source the host allows |
| `TextField`, `Input.Text`, `Input.Number`, `Input.Date`, `Input.Time` | `textarea` bound to the state (dates, times and numbers as text) |
| `CheckBox`, `Input.Toggle` | `checkbox` bound to the state |
| `MultipleChoice`, `Input.ChoiceSet` | `list` with single or multiple selection, bound to the state |
| `Slider` | `slider` bound to the state |
| A2UI's data model, `path` bindings and `List` templates; cards' `$data` and `${a.b}` | `state.model...` and `state.data...`, bound with `{{...}}` |
| A2UI's `action`; `Action.Submit`, `Action.Execute`, `Action.OpenUrl`, `Action.ShowCard`, `selectAction` | `onClick` steps: `emit` (`action`, `submit`, `execute`, `openUrl` with an `https` link only) and, for `ShowCard`, a `toggle` that shows the card |
| A2UI's updates after `beginRendering` | revisions, applied in order when the screen loads |

Anything else, from an unknown element to a password field, is a visible placeholder ("Unsupported: Video") with a
warning naming its place; an Adaptive Card's `fallback` is followed first. Text from the source is never evaluated: a
text with `{{` in it goes into `state.literals` and is shown through a binding to it. Limits: 1 MiB of source, 32
elements deep, 2,000 elements and messages. Only the first A2UI surface converts, and a converted screen has fonts
only when `--font` names one (AffrescoUI has no font of its own), so load one through `affrescoui import --font` for
text.

## Migrating from version 1

Version 1 grew by additions; version 2 keeps one form of each thing it wrote two ways, before 1.0. What changed:

| Version 1 | Version 2 |
|---|---|
| `"from"` on a `row`, a `column` or a `stack` (and in their styles) | `"justify"`, the same values |
| `"align"` on a `stack` | `"justify"` |
| `len(x)` in expressions | `length(x)` |
| A feature's `"requires": { "format": 1 }` | `"format": 2` |
| `"version": 1` | `"version": 2` |

The same rewrites apply to every file that holds screen content: screens and their include parts, scenario files,
change sets (and the patches of their revisions), feature packages and catalogue indexes.

- `affrescoui migrate <file>... [--write]` lists each rewrite, `file: place: what`, and with `--write` writes it. It
  changes only the keys and values above, so ids, key order and spacing stay as written, and
  running it twice changes nothing. `affrescoui migrate --check <file>...` only says which files need it and exits
  with 1 if any does: CI runs it over every JSON file of the repository.
- The loader reads a version 1 file as version 2, in memory, and logs one notice for the screen and its include parts;
  nothing is written. A version 1 change set or scenario file is migrated the same way when it is read.
- A revision written for version 2 that uses a removed key or function is refused with a hint naming the new one:
  `unknown property 'from' for this type ('from' was removed in version 2: use 'justify')`, `unknown function 'len'
  ('len' was removed in version 2: use 'length')`.
- A history on disk is never rewritten. When its head was written for version 1, the screen opens with one commit on
  top, by `format`, "format version 2", that holds the migration; older commits stay as they were recorded, and
  restoring one of them brings back version 1 content, which the checker then refuses with the hints above.
- A signed file (a feature package, a catalogue index) is left as version 1 by `affrescoui migrate`, since rewriting
  it would break its signature: readers check the signature, then migrate it in memory. Sign it again after migrating.

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
affresco::Screen screen = affresco::Screen::fromFile("index.screen.json");
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

See the templates, a folder each in `examples/` ([`examples/README.md`](examples/README.md)): `launcher` (the one
`affrescoui_launcher` opens without a file: the apps as cards, `fs` services on the folders the host grants, a copy
from one root into another, apps started through the host's app-programs grant), `kanban` (rows, components, drag
and drop, a popover), `smart-home` (themes, computed values, switches
and sliders, animations), `music-player` (a playback timer, seeking), `code-editor` (an application type built on
`editor`, tabs, a terminal, a Cmd+P palette), `habit-tracker` (validation-free forms, `watch`, `persist`) and
`sign-in` (screens, navigation, `check`).

**Inside the host's OpenGL.** A host that has its own window and context (a game, a 3D tool) embeds the screen
instead of giving it a window: `affresco::Embedding embedding(loader, clock, "Title", 800, 600)` in the current
context, `screen.embed(embedding)`, then each frame `embedding.frame(width, height, scale, 0)` over the host's picture,
or `embedding.renderTexture(width, height, scale)` for a texture the host shows where it likes. Its input goes to
`embedding.window()`. The OpenGL state each call changes is restored before it returns.

**Second windows.** A screen can open in a second window of the same application that shares the first one's
OpenGL objects and default font: `screen.createWindow(app, mainWindow)` (or `app.createWindow(title, width, height,
mainWindow)` from code) reads the document's `"window"` as `createWindow(app)` does. Textures, buffers and glyph
atlases made in one window's context can be used in the other; each window still draws with its own renderer. Hiding
it (`setVisible(false)`) keeps it for later; closing it closes only it. A presenter view is one: the audience sees a
second window, the presenter the first.

**Images of panels.** `window.renderToImage(panel, scale)` draws a laid-out panel and its subtree into a bitmap of its
bounds times `scale`: what a thumbnail, a sorter or a second window shows. The image is opaque (black where nothing
draws), the panel's own transform is drawn but its ancestors' are not, and a hidden panel, an empty one or a headless
window gives none. `screen.setImage(name, image)` then shows it in an `image` node, in this window or another: a
second window that mirrors a panel shows such an image and is given a new one when the panel changes. A document
does the same without the host: a node's `snapshot` draws it into a named image whenever it changes.

**From C and Python.** `affresco/affresco.h` (`modules/capi`) is the same in C, for hosts in other languages: create a
screen from a file or a string (`afr_options.native_actions` lists the actions the host will answer, so the document's
check knows them), read and set its state, answer its actions and calls, and show it in a window, headless or
embedded. `bindings/python` wraps it: `Screen.from_file(...)`, `screen.state["count"] = 3`, `@screen.on_action("save")`,
`screen.render_png("out.png")`. A native call is still reached only when the host grants it, and a change from the host
is a revision the document's rules decide.
