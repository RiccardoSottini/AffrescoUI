# AffrescoUI Screen Format

A JSON format for describing a user interface that AffrescoUI builds and shows at runtime: **what is on screen**, plus the
app's **state, bindings and actions**, so behaviour lives in the file too. Application code can add more behaviour
(native actions, custom types, templates).

Screens are **live**: the UI follows the state, and **revisions** (JSON Patch) can change the document, layout and
behaviour, while the app runs. A revision can come from the app itself or from a program connected over a channel,
for example one driven by an LLM.

**Current version: 1.** Every file declares `"version"`. The schema keeps each version's definition separately
(`$defs/v1`, and later `$defs/v2`, ...) and the loader checks the version first, so future versions can be added
without breaking existing files.

- File extension: `.screen.json`
- JSON Schema (editor autocompletion and validation): [`schema/affrescoui-screen.schema.json`](schema/affrescoui-screen.schema.json).
  Point a file at it with `"$schema"`.
- Try any file without writing code: `./build/affrescoui_host path/to/file.screen.json`

```json
{
	"$schema": "../schema/affrescoui-screen.schema.json",
	"format": "affrescoui-screen",
	"version": 1,
	"window": { "title": "Hello", "width": 600, "height": 400 },
	"fonts": { "default": { "file": "Roboto-Regular.ttf", "size": 16 } },
	"colors": { "accent": "#6366f1" },
	"root": {
		"color": "#f8fafc",
		"children": [
			{ "type": "label", "text": "Click me", "anchors": { "left": 24, "top": 24 }, "width": 160, "height": 44,
			  "align": "center", "color": "@accent", "textColor": "#ffffff", "onClick": "hello" }
		]
	}
}
```

## Top level

| Key | Required | Meaning |
|---|---|---|
| `version` | yes | `1`. |
| `root` | yes | Node for the window's root panel. It always fills the window, so layout properties aren't allowed on it. |
| `format` | | `"affrescoui-screen"`, for tools. |
| `window` | | `title`, `width`, `height`, `minWidth`, `minHeight` (integers). |
| `fonts` | | `name → { "file", "size" }`. Paths are relative to the screen file. `default` is used by text without a `font`. |
| `colors` | | `name → "#rrggbb"`. Referenced anywhere a color is expected as `"@name"`. |
| `styles` | | `name → properties`, applied to a node with `"style": "name"` or `["a", "b"]`. Later styles win, and the node's own properties win over all. |
| `templates` | | `name → node`. Built by code on demand, any number of times (`Screen::instantiate`). A `repeat` in the file is usually simpler. |
| `state` | | The app's data, any JSON object. |
| `permissions` | | Who may change the running screen, like Claude Code's permission rules: see [Version control](VERSION_CONTROL.md). `{ "allow": ["studio"], "ask": ["agent(/actions)"], "deny": ["*(#statusBar)"], "reviewers": ["studio"] }`. A rule is `author` or `author(pattern)`; deny beats ask beats allow, and what no rule allows is reviewed. `reviewers` may decide on proposals over the live protocol. Revisions can never change this section. |
| `actions` | | `name → steps`, run by `onClick`, `onSubmit` or other actions. |

## Nodes

A node is a panel: a rectangle positioned relative to its parent and clipped to it. Children are drawn on top of their
parent, and later siblings on top of earlier ones.

### Values
- **Length:** a number of pixels (`120`, `"120px"`) or a percentage of the parent's matching dimension (`"50%"`).
- **Color:** `"#rgb"`, `"#rrggbb"`, `"#rrggbbaa"`, `"@name"` (from `colors`) or `"transparent"`.
- **Align:** `"start"`, `"center"` or `"end"`.

### Every type
| Key | Meaning |
|---|---|
| `type` | `panel` (default), `label`, `textarea`, `stack`, or a type the application registers. |
| `id` | Name for code to find the node (`screen.get<affresco::Label>("title")`). |
| `style` | Style name or list of names. |
| `children` | Array of nodes. |
| `x`, `y` | Position of the top-left corner inside the parent (used when the matching edge isn't anchored). |
| `width`, `height`, `minWidth`, `minHeight` | Size. The minimum wins over a smaller size. |
| `anchors` | `{ "top", "bottom", "left", "right" }`: keep that edge this far from the parent's same edge. Anchoring two opposite edges stretches the node. |
| `fill` | Shorthand: anchor all four edges this far in (`0` or `true` to cover the parent). |
| `color` | Background. Default: light grey for panels, transparent for labels and stacks. |
| `visible` | `false` hides the node and its children. |
| `cursor` | `arrow`, `ibeam`, `hand`, `resize-horizontal`, `resize-vertical`. |
| `hover`, `pressed`, `focused` | `{ "color" }` shown in that state (pressed wins over focused, focused over hover). |
| `onClick` | Fired when the left button is released over the node: an action name, or steps to run. |
| `repeat` | `{ "for": "message", "in": "state.messages", "index": "i" }`: build the node once per list item. `for` defaults to `item` and `index` to `index`. |
| `when` | Build the node only if the value is truthy. |
| `locked` | `true`: no revision may change this node or anything inside it (short for the rule `"deny": ["*(#id)"]`). Needs an `id`. |

### `label`
| Key | Meaning |
|---|---|
| `text` | The text. |
| `textColor` | Default black. |
| `font` | Font name. Default: `default`. |
| `padding` | Pixels between the edges and the text. |
| `align` | One value for both axes, or `{ "horizontal", "vertical" }`. |
| `wrap` | Break lines that don't fit the width. |
| `fitText` | `true` or `{ "maxWidth" }`: the label sizes itself to its text (plus padding), wrapping at `maxWidth`. |

### `textarea`
Everything `label` has, plus `placeholder` and `placeholderColor`. It's a single-line input: click it to type.

| Key | Meaning |
|---|---|
| `bind` | A state path kept in sync with the text in both directions (`"state.draft"`). |
| `onSubmit` | Enter pressed: an action name or steps. `event.text` is the text. |
| `autofocus` | Focus it when it is first built. |

### `stack`
Places its children one below the other, in order. Children keep their horizontal anchors and width; the stack manages
their vertical position.

| Key | Meaning |
|---|---|
| `gap` | Pixels between children. |
| `align` | `start` (from the top) or `end` (the block sits at the bottom, like a chat; overflow is clipped at the top). |

## State, bindings and actions

### Values
Wherever a value is expected, it can be computed:

| Form | Example | Meaning |
|---|---|---|
| Binding | `"{{state.threads[state.current].title}}"` | The value at a path, keeping its JSON type. |
| Template | `"Clicked {{state.clicks}} times"` | A string with values inserted. |
| Operator | `{ "$clamp": ["{{event.x}}", 180, 520] }` | An object with one `$` key. |

**Paths** start with a variable: `state`, `event` (what triggered the action), `window` (`width`, `height`),
`revision` (the current commit: `number`, `id`, `short`, `author`, `committer`, `message`, `time`), `proposals` (open
proposals: `id`, `author`, `message`, `head`, `short`, `commits`, `operations`, `touches`, `remember`, `reason`,
`time`), `history` (the commits on `main`, like `revision`), `preview` (the id of the proposal being previewed, or
`null`), `mode` (`default`, `acceptEdits` or `plan`), or a `repeat`/`forEach` name. They continue with `.key` and `[index]`, and
an index can itself be a path: `state.threads[state.current]`. A missing value is `null`.

**Operators.** One-argument operators take their value directly (`{"$length": "{{state.items}}"}`). The others take an
array of arguments.
- Logic: `$if` `[condition, then, else]`, `$and`, `$or`, `$not`.
- Comparison: `$equals`, `$notEquals`, `$greater`, `$less`, `$greaterOrEqual`, `$lessOrEqual`.
- Numbers: `$add`, `$subtract`, `$multiply`, `$divide`, `$min`, `$max`, `$clamp` `[value, min, max]`, `$round`.
  Integer inputs give integer results.
- Collections and text: `$length`, `$empty`, `$concat` (strings or lists), `$join` (`[list, ", "]`: the items as text).

**Truthy** means anything except `false`, `null`, `0`, `""`, `[]` and `{}`.

### Actions
An action is a list of steps. Each step is an object whose first key says what it does:

| Step | Example |
|---|---|
| `set` | `{ "set": "state.current", "to": "{{index}}" }` |
| `push` | `{ "push": "state.threads[state.current].messages", "value": "{{state.draft}}" }` |
| `remove` | `{ "remove": "state.items", "at": 0 }`, or without `at` to delete a key |
| `toggle` | `{ "toggle": "state.dark" }` |
| `increment` | `{ "increment": "state.clicks", "by": 1 }` |
| `if` | `{ "if": { "$empty": "{{state.draft}}" }, "then": [...], "else": [...] }` |
| `forEach` | `{ "forEach": "state.items", "as": "item", "index": "i", "do": [...] }` |
| `while` | `{ "while": { "$less": ["{{state.n}}", 10] }, "do": [...] }` (at most 10,000 iterations) |
| `run` | `{ "run": "otherAction" }` |
| `call` | `{ "call": "nativeName" }`: a C++ function registered with `Screen::onAction` |
| `emit` | `{ "emit": "saved", "data": { "id": "{{state.current}}" } }`: an event for every connected peer |
| `patch` | `{ "patch": [JSON Patch ops], "message": "..." }`: the app revises its own document |
| `focus` | `{ "focus": "input" }` |
| `log` | `{ "log": "value is {{state.n}}" }` (to stderr) |
| `stop` | `{ "stop": true }`: end the action |
| `preview` | `{ "preview": "{{proposals[0].id}}" }`: show a proposal live, without committing it |
| `endPreview` | `{ "endPreview": true }`: back to the committed screen |
| `accept` | `{ "accept": "{{proposals[0].id}}" }`: merge a proposal, approved by `user` |
| `reject` | `{ "reject": "{{proposals[0].id}}", "reason": "..." }` |
| `restore` | `{ "restore": "{{entry.number}}" }`: a new revision equal to an earlier one (a number or a commit id) |
| `revert` | `{ "revert": "{{entry.number}}" }`: a new revision that undoes that one and keeps the later ones |
| `undo` | `{ "undo": true }`: revert the latest revision |

The last seven, plus `patch` and `call`, are **control steps**: they can be written in the screen file, but no
revision may add or change them.

Only paths under `state` can be changed. After an event's actions finish, the UI is updated once. Panels are reused
where the structure didn't change, so focus, typing and drags in progress are kept. Errors in actions are reported
(on stderr and to the peers) and never crash the app.

## Live screens: revisions and the channel

### Revisions
A revision is a version-controlled change to a running screen:

```json
{ "type": "revision", "parent": 3, "author": "assistant", "message": "Dark theme",
  "patch": [ { "op": "replace", "path": "/colors/background", "value": "#0b1220" },
             { "op": "add", "path": "#header/children/-", "value": { "id": "counter", "type": "label" } },
             { "op": "replace", "path": "/state/sidebarWidth", "value": 400 } ] }
```

- `patch` is standard [JSON Patch (RFC 6902)](https://datatracker.ietf.org/doc/html/rfc6902), applied one operation
  at a time. Paths can start with `#id` to address a node by its id instead of its position: `#header/children/-`.
- Operations on `/state/...` change the live state. All others change the document: layout, styles, colors, fonts,
  actions and templates.
- Revisions are **atomic**. If anything fails (invalid patch, unknown id, a property the new document gets wrong),
  nothing changes and the revision is rejected with the error.
- `parent` is optional: a revision number or commit id. If it's given and isn't the current revision, the revision is
  rejected as a conflict. `proposal` (optional) pushes onto the author's own open proposal.
- Every applied revision is a **commit** on `main`, named by its SHA-256 (`a3f9c21` for short) and numbered from `0`
  (the file). Reverting, restoring and undoing create new commits, so history only grows. State changes aren't undone.
- **Permissions** decide what happens to a revision from each author: applied, proposed (a branch a reviewer merges or
  rejects), or rejected. Deny rules and control steps hold for every revision. See
  [VERSION_CONTROL.md](VERSION_CONTROL.md), which also describes the modes, "don't ask again" grants and **change
  sets** (exported revisions, shareable like plugins).

### Peers and the live protocol
The screen doesn't care where changes come from. Any number of **peers** (programs on the other end of a channel)
read and change it with one JSON object per line:

- `Screen::listen(executor, "/tmp/affrescoui-501/agent.sock", "agent")` accepts peers on a local socket
  (`affresco::channel::SocketServer`); `Screen::connect(channel, "name")` adds one over any `affresco::Channel` (a
  `ProcessChannel` child process, `StdioChannel`, or your own).
- **The host decides who a peer is**, by the socket or channel it came through: its revisions are authored as that
  name, followed by the name the peer gives itself (`agent/claude-code`). The `hello` message says which name that is
  (`"you"`) and whether it may review (`"canReview"`).

| Direction | Message |
|---|---|
| → peer | `{"type": "hello", "format": "affrescoui-screen", "revision": n, "head": {commit}, "document": {...}, "state": {...}, "mode": "default", "you": "agent", "canReview": false}` on connect |
| → peer | answers to its revisions: `{"type": "applied", "revision": n, "commit": id}`, `{"type": "proposed", "proposal": p, "commit": head}` or `{"type": "rejected", "error": "..."}` |
| → all peers | `{"type": "committed", "revision": n, "id", "short", "parents", "author", "committer", "message", "time", "proposal"}` whenever a commit lands on `main` |
| → all peers | `{"type": "review", "proposals": [...], "previewing": p or null, "mode": "default", "grants": [...]}` whenever that changes |
| → all peers | a reviewer's decision: `{"type": "applied", "proposal": p, "commit": merge}` or `{"type": "rejected", "proposal": p, "error": reason}` |
| → all peers | `{"type": "event", "name": "...", "data": {...}, "revision": n}` from `emit` steps, `{"type": "error", "message": "..."}` |
| → peer | answers: `{"type": "document", ...}` (with head, proposals, mode and grants), `{"type": "history", "entries": [...]}`, `{"type": "diff", "patch": [...]}`, `{"type": "changes", "changes": {...}}`, `{"type": "done"}` |
| peer → | `{"type": "revision", ..., "proposal"?: p}`, `{"type": "revert" / "restore", "revision": n or id}`, `{"type": "undo"}` (all follow the author's permissions) |
| peer → | `{"type": "getDocument"}`, `{"type": "getHistory", "limit"?: n}`, `{"type": "diff", "from", "to"}` or `{"type": "diff", "proposal": p}`, `{"type": "exportChanges", "from": n, "name": "..."}` |
| reviewer → | `{"type": "accept", "proposal": p, "head": id, "dontAskAgain"?: true}` (`head`: the version reviewed), `{"type": "reject", "proposal": p, "reason"?}`, `{"type": "preview", "proposal": p}`, `{"type": "endPreview"}`, `{"type": "setMode", "mode": "acceptEdits"}`, `{"type": "revoke", "rule": "agent(#board)"}`: only for authors in `permissions.reviewers` |
| peer → other peers | `{"type": "request", "id", "text"}` and `{"type": "reply", "request", "text"}`: passed on to every other peer with `"from"` added, never interpreted (e.g. a person's request in plain words, for an AI agent) |

Requests may carry an `"id"`; answers repeat it.

The examples put this together (see the README): `affrescoui_host` shows a screen and listens on `studio.sock` (author
`studio`, a reviewer) and `agent.sock` (author `agent`); `affrescoui_studio` is the person's client; the Python MCP server
(`mcp/affrescoui_mcp.py`) connects an AI agent to `agent.sock` in both directions.

## Errors

Loading stops at the first problem and names its place in the file:

```
AffrescoUI screen: root.children[0].anchors: unknown edge 'lefft' (expected top, bottom, left or right)
AffrescoUI screen: root.children[0].width: expected a length: a number of pixels, "12px" or "50%"
AffrescoUI screen: root.children[0].text: unknown property 'text' for this type
```

## From code

```cpp
affresco::Application app;
affresco::Screen screen = affresco::Screen::fromFile("kanban.screen.json");
screen.registerType("chart", [] { return std::make_unique<MyChart>(); }); // a custom type, before createWindow
affresco::Window& window = screen.createWindow(app);

screen.onAction("save", [&](affresco::Panel&) { /* ... */ });
auto& search = screen.get<affresco::TextArea>("search");
screen.listen(app, "/tmp/affrescoui-501/agent.sock", "agent"); // peers can change it from now on
app.run();
```

See the templates in `examples/assets/screens/`: `kanban.screen.json` (the one `affrescoui_host` opens), `smart-home`,
`music-player`, `code-editor` and `habit-tracker`; `studio.screen.json` (the
Studio client: its whole UI and behaviour, with a few lines of C++ to plug it into the host) and `sign-in.screen.json`.
