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

| Key | Meaning |
|---|---|
| `version` | Required. `1`. |
| `root` | Required. The window's root node. It always fills the window, so layout properties aren't allowed on it. |
| `format` | `"affrescoui-screen"`, for tools. |
| `note` | A comment. |
| `window` | `title`, `width`, `height`, `minWidth`, `minHeight` (integers). |
| `fonts` | `name → { "file", "size", "fallback" }`, paths relative to the screen file. `default` is used by text without a `font`. Text is UTF-8 and shows any character the font has. `"fallback": ["symbols"]` names other fonts, asked in order for a character this one lacks; their glyphs are drawn at this font's size. A character none of them has shows as `?`. |
| `colors` | `name → "#rrggbb"`, or per theme `name → { "light": "#fff", "dark": "#111827" }`. Used as `"@name"`. |
| `theme` | Which variant of the colors is shown, usually bound: `"{{state.theme}}"`. Without a match, the first variant. |
| `styles` | `name → properties`, applied with `"style": "name"` or `["a", "b"]`. A style may itself have a `style`. |
| `components` | `name → { "params": {defaults}, "node": node }`: reusable nodes with parameters (see Components). |
| `screens` | `name → node`: the screens an `outlet` shows; `{"navigate": "name"}` switches. The first is shown first. |
| `templates` | `name → node`, built by code on demand (`Screen::instantiate`). |
| `state` | The app's data, any JSON object. |
| `computed` | `name → expression`: values derived from the state, read as `computed.name` (see Values). |
| `watch` | `"state.path" → handler`: runs after an update in which that value changed (`event.value`, `event.previous`). |
| `actions` | `name → steps`, run by handlers, `run` steps, shortcuts and channels. |
| `keys` | Screen-wide shortcuts: `"mod+k" → handler` (see Events). |
| `onLoad` | A handler run once after the first update. |
| `channels` | Programs the screen talks to (see Channels). A capability: revisions can't change it. |
| `persist` | `["state.habits", ...]`: state paths kept on disk across restarts. A capability: revisions can't change it. |
| `include` | `["parts/sidebar.json", ...]`: files merged in at load, section by section (this file's entries win). |
| `permissions` | Who may change the running screen (see [Version control](VERSION_CONTROL.md)). Revisions can't change it. |

## Nodes

A node is a panel: a rectangle positioned relative to its parent and clipped to it. Children are drawn on top of their
parent, later siblings on top of earlier ones.

### Values
- **Length:** pixels (`120`, `"120px"`) or a percentage of the parent's matching dimension (`"50%"`).
- **Color:** `"#rgb"`, `"#rrggbb"`, `"#rrggbbaa"`, `"@name"` (from `colors`, in the current theme) or `"transparent"`.
- **Align:** `"start"`, `"center"` or `"end"`.

### Every type
| Key | Meaning |
|---|---|
| `type` | `panel` (default), `label`, `textarea`, `editor`, `stack`, `row`, `column`, `grid`, `toggle`, `checkbox`, `slider`, `outlet`, or a type the application registers. |
| `id` | A name to address the node (`#id` in edits, `nodes.id`, anchors, `focus`). |
| `style` | Style name or list of names; may be computed: `"{{on ? 'chipOn' : 'chip'}}"`. |
| `children` | Array of nodes. |
| `x`, `y` | Position inside the parent (used when the matching edge isn't anchored). |
| `width`, `height`, `minWidth`, `minHeight` | Size. The minimum wins over a smaller size. |
| `anchors` | `{ "top", "bottom", "left", "right" }`: keep that edge this far from the parent's same edge. Anchoring opposite edges stretches the node. A value `"#id.edge + 8"` puts the edge at another node's edge (`"top": "#header.bottom + 12"`, `"bottom": "#footer.top - 8"`). |
| `fill` | Anchor all four edges this far in (`0` or `true` to cover the parent). |
| `grow` | In a `row`/`column`: the share of the spare room it takes, like CSS flex-grow. |
| `color` | Background. Default: light grey for panels, transparent for labels, stacks and controls. |
| `visible` | `false` hides the node and its children. |
| `cursor` | `arrow`, `ibeam`, `hand`, `resize-horizontal`, `resize-vertical`. |
| `tooltip` | Text shown after the pointer rests on the node. |
| `disabled` | `true`: no events (hover styling still applies). |
| `scroll` | `"vertical"`, `"horizontal"`, `"both"` or `"none"`: the wheel or trackpad scrolls the children; a thin bar shows where. |
| `overlay` | `{ "anchor": "#id", "placement": "below", "open": "state.menuOpen", "dismiss": "outside" }`: drawn above everything, unclipped, next to its anchor (default: its parent). Placements: `below`, `below-end`, `above`, `above-end`, `right`, `left`, `center` (in the window), `window` (placed by its own x, y, anchors in the window). `open` is a state path (or a value); with `"dismiss": "outside"`, a click outside it or Escape sets it to false. |
| `hover`, `focused`, `dragging`, `dragOver`, `pressed` | Properties shown in that state (later ones win: pressed over drag over over dragging over focused over hover). Any visual property: `{ "color", "textColor", "style", ... }`. A node counts as hovered and pressed while one of its children is. |
| `transition` | `{ "color": 150, "textColor": 150 }`: changes of these colours animate over that many milliseconds. |
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

### `label`
| Key | Meaning |
|---|---|
| `text` | The text. |
| `textColor` | Default black. |
| `font` | Font name. Default: `default`. |
| `padding` | Pixels between the edges and the text. |
| `align` | One value for both axes, or `{ "horizontal", "vertical" }`. |
| `wrap` | Break lines that don't fit the width. |
| `fitText` | `true` or `{ "maxWidth" }`: the label sizes itself to its text (plus padding). |

### `textarea`
Everything `label` has, plus `placeholder`, `placeholderColor`, `bind` (a state path kept in sync both ways, or
`"{{...}}"` giving one), `onChange` (`event.value`), `onSubmit` (Enter, `event.text`) and `autofocus`. Single line.

### `editor`
A multi-line text editor (Cmd on macOS, Ctrl elsewhere): click, drag, double and triple click select; arrows,
Home/End, Page Up/Down, Alt+arrows (words), Cmd+arrows (line and document ends), Shift to extend; Enter keeps the
indentation; Tab/Shift+Tab indent; Cmd+A/C/X/V/Z, Cmd+Shift+Z. The caret's line is highlighted; the wheel scrolls.
Tabs become 4 spaces. Applications build richer editors on it (syntax colours, commands): see
`examples/CodeEditor.cpp`, the Forge template's `code` type.

| Key | Meaning |
|---|---|
| `bind` | A state path holding the text (lines joined by `"\n"`), kept in sync both ways. Without it, `text` sets the text. |
| `caret` | A state path the editor writes its caret to: `{ "line", "column", "selected" }`, counted from 1. |
| `font`, `textColor` | Font and text colour. |
| `lineNumbers` | `true` shows line numbers in a gutter. |
| `lineNumberColor`, `activeLineNumberColor` | Line numbers, and the caret's line number. |
| `lineColor`, `selectionColor`, `caretColor` | The caret's line background, the selection, the caret. |
| `autofocus`, `onChange` | Focus when built; `event.value` is the new text. |

### `row`, `column`, `stack`, `grid`
`column` (also `stack`) places its children one below the other, `row` side by side; they manage their children's
positions along that direction and leave the other direction to the children's own anchors.

| Key | Meaning |
|---|---|
| `gap` | Pixels between children. |
| `from` | Where the block sits: `start` (default), `center` or `end` (a column from the bottom, like a chat: overflow is clipped at the top). `align` is an older name for it on `stack`. |
| `direction` | On a `stack`: `"row"` or `"column"`. |
| `wrap` | On a row: continue on a new line when a child doesn't fit. |

`grid` places its children in equal `columns` (an integer), `gap` apart; each row is as tall as its tallest child.

### `toggle`, `checkbox`, `slider`
Value controls. `bind` keeps the value in a state path; without it, `checked` (toggle, checkbox) or `value` (slider)
shows a value and `onChange` (`event.value`) reacts. A control keeps its own clicks (they don't bubble).

| Key | Meaning |
|---|---|
| `trackColor` | The empty or off part. |
| `activeColor` | The filled or on part. |
| `knobColor` | The knob (or the inside of the checkbox). |
| `min`, `max`, `step` | Slider range (default 0 to 100) and snapping. |

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
`children`. Changing a component changes every use: one edit restyles a whole list.

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
  `a ?? b` (b when a is null), `c ? a : b`;
- functions: `len`, `empty`, `round`, `floor`, `ceil`, `abs`, `min`, `max` (numbers or a list), `clamp(v, lo, hi)`,
  `concat`, `join(list, sep)`, `split(text, sep)`, `slice(list or text, from, to?)`, `contains`, `indexOf`, `keys`,
  `values`, `entries`, `merge(a, b)`, `reverse`, `range(n)` (n up to 10000), `upper`, `lower`, `trim`, `number`, `text`,
  `type`, `fixed(n, digits)`, `pad(n, width, fill?)` (width up to 10000), `plural(n, 'card', 'cards'?)`, `now()`,
  `time(ms, 'ddd D MMM HH:mm')`, `parseJson`, `json`;
- per item, with the item as `it` and its position as `index`: `filter(list, it.done)`, `map(list, it.name)`,
  `count(list, cond?)`, `sum(list, expr?)`, `find(list, cond)`, `some`, `every`, `sort(list, key?, 'desc'?)`.

Each function is also an operator: `len(x)` is `{"$length": x}`, `filter(l, c)` is `{"$filter": [l, c]}`.

**Variables:** `state`; `computed`; `event` (what triggered a handler); `window` (`width`, `height`); `theme`;
`screen` (the screen an outlet shows); `nodes` (`nodes.sidebar.width`: a built node's `x`, `y`, `width`, `height`,
`visible`, from the last layout); `revision` (the current commit: `number`, `id`, `short`, `author`, `committer`,
`message`, `time`); `history`; `proposals` (`id`, `author`, `message`, `head`, `short`, `commits`, `operations`,
`touches`, `changes`, `remember`, `reason`, `time`); `preview`; `mode`; and the names given by `repeat`, `let`,
`with`, `forEach`, a component's params, `it`/`index` inside list functions.

**`computed`** values are evaluated when first read after the state changed, then cached; they may use each other
(not in a circle). Prefer them to actions that recount things by hand.

**Truthy** means anything except `false`, `null`, `0`, `""`, `[]` and `{}`.

## Events

A handler is an action name, a step, or a list of steps. Pointer events get `event.x`/`y` (in the window),
`event.left`/`top` (in the handling node) and its `width`/`height`.

| Handler | When |
|---|---|
| `onClick`, `onDoubleClick`, `onContextMenu` | Left click, two clicks on the same node, right click. |
| `onPress`, `onRelease` | Left button down, up. |
| `onDrag` | The pointer moves while pressed: `event.dx`, `event.dy` since the press. |
| `onDrop` | A dragged node was dropped here: `event.data`. |
| `onHover`, `onLeave` | The pointer enters or leaves the node (or a child). |
| `onFocus`, `onBlur` | Keyboard focus. |
| `onScroll` | `event.dx`, `event.dy`. |
| `onKey` | A key on the focused node: `event.key`, `shortcut`, `shift`, `ctrl`, `alt`, `cmd`. |
| `onChange` | A bound or value control changed: `event.value`. |
| `onSubmit` | Enter in a text area: `event.text`. |
| `onShow`, `onHide` | The node was built, or removed (by `when`, `repeat` or a revision). |

**Bubbling:** click, double click, context menu, press, release, drag, scroll and key events go to the node or, if it
has no handler, its nearest ancestor with one. A row with `onClick` needs no invisible overlay above its labels.

**Shortcuts:** `"keys"` on the document (anywhere) or on a node (while it or a child is focused): `"mod+k"`,
`"cmd+shift+p"`, `"escape"`, `"up"`, `"f5"`. `mod` is Cmd on macOS, Ctrl elsewhere. While a text area is focused, keys
without Cmd, Ctrl or Alt go to the text (Escape still works).

## Actions

An action is a list of steps. Each step is an object whose first key says what it does:

| Step | Example |
|---|---|
| `set` | `{ "set": "state.current", "to": "{{index}}" }` |
| `push` | `{ "push": "state.columns[c].cards", "value": "{{state.draft}}" }` |
| `remove` | `{ "remove": "state.items", "at": 0 }`, or without `at` to delete a key |
| `toggle` | `{ "toggle": "state.dark" }` |
| `increment` | `{ "increment": "state.clicks", "by": 1 }` |
| `if` | `{ "if": "{{len(state.draft) == 0}}", "then": [...], "else": [...] }` |
| `forEach` | `{ "forEach": "state.items", "as": "item", "index": "i", "do": [...] }` |
| `while` | `{ "while": "{{state.n < 10}}", "do": [...] }` (at most 10,000 iterations) |
| `run` | `{ "run": "moveCard", "with": { "from": "{{c}}", "to": "{{c + 1}}" } }`: an action with variables; the name may be computed |
| `call` | `{ "call": "pickFile", "with": {...}, "into": "state.file", "onError": [...] }`: a native function (`Screen::onCall`); `error` in `onError` |
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

**Control steps** (`call`, `send`, `connect`, `patch` and the review and history steps) can be written in the screen
file, but no revision may add or change them: an agent can rearrange how the app uses the outside world and its own
history, never widen it. Nor may a revision move one to another trigger (a button's `onClick`, an `every` timer,
`keys`, `watch`, `onLoad`), or add a `run` or a handler that names an action with control steps, a native action, or
a computed action name.

Only paths under `state` can be changed. After an event's steps finish, the UI is updated once. Panels are reused where
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
`%APPDATA%\affrescoui\<file name>.state.json`), loaded at start and written when they change.

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
- Revisions are **atomic**. If anything fails, nothing changes and the error names the operation:
  `revision.patch[0] (set #nope/color).path: no node with id 'nope'`.
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
| → peer | answers: `document` (with head, proposals, mode, grants), `history`, `diff`, `changes`, `catalog`, `done` |
| peer → | `revision` (compact edits or `patch`, `proposal`?), `revert` / `restore` (`revision`), `undo` |
| peer → | `getDocument`, `getHistory` (`limit`?), `diff` (`from`, `to` or `proposal`), `exportChanges` (`from`, `name`), `getCatalog` |
| reviewer → | `accept` (`proposal`, `head`, `dontAskAgain`?), `reject`, `preview`, `endPreview`, `setMode`, `revoke` |
| peer → other peers | `request` (`id`, `text`) and `reply` (`request`, `text`), relayed with `from` |

Requests may carry an `"id"`; answers repeat it.

### Catalog
`getCatalog` (MCP: `get_catalog`) answers what this screen's host offers: node types and their properties, common
properties, events, steps, control steps, operators, variables and sections, plus the custom types, native actions and
native functions the application registered. Agents read it instead of guessing.

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
  aren't defined; component parameters the component doesn't declare; unknown steps; expressions that don't parse.
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
// A custom type has the properties of the built-in type it derives from, plus its own, given here with their handler
screen.registerType("gauge", [] { return std::make_unique<MyGauge>(); }, {"needle"},
                    [&](affresco::Panel& panel, const std::string& propertiesJson) { /* {"needle": "@accent"} */ });
screen.onCall("pickFile", [](const std::string& argumentsJson) { return R"("/tmp/a.png")"; }); // {"call": "pickFile"}
affresco::Window& window = screen.createWindow(app); // also connects the file's channels
screen.listen(app, "/tmp/affrescoui-501/agent.sock", "agent"); // peers can change it from now on
app.run();
```

See the templates in `examples/assets/screens/`: `kanban` (the one `affrescoui_host` opens: rows, components, drag and
drop, a popover), `smart-home` (themes, computed values, switches and sliders, animations), `music-player` (a playback
timer, seeking), `code-editor` (an application type built on `editor`, tabs, a terminal, a Cmd+P palette), `habit-tracker` (validation-free forms, `watch`,
`persist`), `sign-in` (screens, navigation, `check`), and `studio` (the Studio client: its whole UI, behaviour and
connection).
