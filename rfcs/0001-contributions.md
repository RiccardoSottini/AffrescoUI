# RFC 0001: Contributions: commands, keybindings, menus and settings

- **Issue:** none yet (written for roadmap item 8.1, wave 3)
- **Author:** the `3-rfcs` fleet agent (Claude), for the maintainer
- **Changes:** the screen format; the MCP tools (one read, optional)
- **Prototype:** [`prototypes/0001-contributions/`](prototypes/0001-contributions/README.md): a screen that runs
  today and builds a palette, a menu, a keybinding editor and a settings page from data, the same app in the proposed
  syntax, and the edits measured below

## The problem

Large apps such as IDEs and creative tools are mostly made of **commands**: hundreds of things the user can do, each
reachable from a keyboard shortcut, a menu, a context menu, a toolbar button and a command palette, each enabled only
when it makes sense, and each with settings that change how it behaves. VS Code describes all of that as data: an
extension's `package.json` *contributes* commands, keybindings, menus and configuration, and `when` clauses decide
where each applies. The workbench then builds its palette, menus, keybinding editor and settings page from those
declarations, so a new command shows up everywhere at once.

AffrescoUI screens have the raw parts but not the declarations:

- An **action** is a name and a list of steps. It has no title, category, icon or enablement, so nothing can list
  it to a person. The Forge template's Cmd+P palette (`examples/assets/screens/code-editor.screen.json`) can only open
  files, because a palette of commands would need a second, hand-kept list in the state.
- **`keys`** (on the document, or on a node) maps one key press to a handler. There are no chords (`mod+k mod+s`), no
  condition (a key that means one thing in the editor and another in a list), and no way for a menu or a palette to
  show which key runs a command.
- A **menu** is a hand-built tree of nodes inside an `overlay`. Adding an entry means writing a styled node with an
  `onClick`, in the right place, for each menu that should show it. Nothing can add an entry to a menu that it
  didn't write, which features (RFC 0004) need.
- **Settings** are plain state. There is no description of them, so no settings page can be built, no value is
  checked, and an agent has to read the whole state to learn what can be configured.

The prototype measures what this costs today. Adding one command with a shortcut and a menu entry takes **118
tokens** when the lists are kept in the state by hand and **163** when the menu is built from nodes (the template
way), and in the second case the command is still missing from the palette (see "The cost in tokens").

**Who needs it:** any app with more than a handful of actions: an IDE, an image editor, a slide tool, a music tool,
a CAD viewer, a game launcher with a settings page.

**Out of scope here:**
- the widgets: menu bars, context menus and dialogs (8.2c), the quick pick and fuzzy matching (8.3), native menus
  (2.7). They read what this RFC declares; they are designed in their own items;
- setting scopes beyond one user (per-workspace and per-language overrides): a later RFC (open question 8);
- compatibility with VS Code's extension API or its `package.json` format: there is no JavaScript extension host.

## The design

Five additions, each small, all additive within format version 1:

| Part | Where | What it adds |
|---|---|---|
| **Commands** | `actions` | An action written as an object with a `title` is a command: `title`, `category`, `icon`, `enablement`, `do` |
| **Keybindings** | `keys` (document and node) | Chords (`"mod+k mod+s"`), bindings with `when`, lists of bindings, one resolution order |
| **Context keys** | the `context` variable; a node's `context` key | What `when` and `enablement` read about focus, overlays, chords, the platform and widgets |
| **Menus** | a `menus` section | Named contribution points with groups and keyed items |
| **Settings** | a `settings` section; the `settings` variable | A schema per setting; values kept in `state.settings`; a generated settings page |

And five read-only variables, computed when read, that palettes, menus, keybinding editors and settings pages bind
to: `commands`, `keybindings`, `menus`, `settings` and `settingsList`.

### Commands: actions with a title

An action can already be a step, a list of steps or a name. It gains one more form, an **object with `do`**:

```json
"actions": {
	"files.save": {
		"title": "Save",
		"category": "File",
		"icon": "save",
		"enablement": "{{computed.activeFile && computed.activeFile.dirty}}",
		"do": [{ "call": "writeFile", "with": { "file": "{{computed.activeFile}}" } }]
	},
	"closeTab": [{ "remove": "state.tabs", "at": "{{at}}" }]
}
```

| Key | Type | Default | Meaning |
|---|---|---|---|
| `do` | handler | required | What the command runs: a step, a list of steps or an action name, as anywhere else |
| `title` | text (may be bound) | none | Makes the action a **command**: it is listed in `commands`, and palettes and menus show it |
| `category` | text (may be bound) | none | A group for palettes and the keybinding editor: `"File"` shows as "File: Save" |
| `icon` | icon name | none | An icon from the host's icon set (2.2), for menus and toolbars |
| `enablement` | expression | `true` | When it is false, keys, menus and palettes don't run the command, and show it disabled |
| `description` | text | none | A longer sentence for the palette and the keybinding editor |
| `note` | text | | A comment, as everywhere |

- **One name, one place.** A command is an action, so everything that names actions (`onClick`, `run`, `keys`,
  channel actions, `Screen::runAction`) works with commands unchanged, and the checker's knowledge of actions covers
  them.
- **An object without a step key is a command.** A step is an object whose first key is a step name (`set`, `run`,
  ...); `do`, `title` and the others aren't step names, so the two forms can't be confused. An object with both is an
  error: `actions.files.save: an action is either a step or a command ({ "title", "do" }), not both`.
- **Names.** Command names are words joined by dots, a namespace first: `files.save`, `view.toggleMinimap`,
  `todo.refresh`. Dots are plain characters in JSON Pointer, so `/actions/files.save` needs no escaping. The
  namespace is a convention today and a rule for features (RFC 0004: a feature's commands start with its name).
- **Arguments.** A command runs without variables from keys, menus and palettes. Callers that pass some still can
  (`{ "run": "files.save", "with": { "file": 3 } }`); bindings and menu items may give `with` too.
- **Enablement guards the user's ways in, not the program's.** `enablement` is checked when a key, a menu item or a
  palette runs the command. A `run` step or a native call runs it regardless, as in VS Code, so a screen's own logic
  never stops working because a condition about the UI is false.

### Keybindings: chords, `when`, and one resolution order

`keys` stays where it is (the document's `keys`, and `keys` on any node) and keeps every form it accepts today. A
key's value can now also be a **binding object** or a **list of bindings**:

```json
"keys": {
	"mod+s": "files.save",
	"mod+k mod+s": "preferences.keybindings",
	"escape": [
		{ "when": "{{context.overlay}}", "do": "closeOverlay" },
		{ "when": "{{context.chord}}", "do": "cancelChord" }
	],
	"shift+alt+f": { "when": "{{context.editorFocus}}", "do": "editor.format" }
}
```

| Form | Matches |
|---|---|
| A handler: an action name, a step, a list of steps (as today) | always |
| A binding: `{ "when": "{{...}}", "do": handler, "with": {...} }` | when `when` is truthy (no `when`: always) and, if `do` names a command, the command is enabled |
| A list of bindings (every item a binding object) | the first item that matches, in order |

- **Chords** are presses separated by one space: `"mod+k mod+s"`. Each press is written as today (`mod`, `cmd`,
  `ctrl`, `alt`, `shift`, then the key). At most four presses.
- **A list** holds several bindings for one key in one scope, which features need: two features can both bind
  `escape` in the document's `keys`, each with its own `when`. Installing the second one turns the value into a list
  (RFC 0004 writes that edit).
- **Errors name the place:** `keys.escape[1]: a list of bindings: each item needs "do"`, `keys.mod+k mod+s mod+x
  mod+y mod+z: a chord has at most four presses`.

**Resolution.** On every key press the runtime resolves the press in this order, and stops at the first match:

1. **Text input.** While a text field has the focus (`textarea`, `editor`, or a registered type that takes text), a
   press without Cmd, Ctrl or Alt goes to the text, except Escape and bindings on the focused node and its
   ancestors (so `enter` on a `textarea` still works). This is today's rule, now written down for every scope.
2. **A chord in progress.** If earlier presses started a chord, only bindings whose next press matches are
   candidates. A press that matches none ends the chord and is dropped (VS Code does the same), and Escape ends it.
   There is no timeout. While a chord is waiting, `context.chord` holds what was pressed (`"mod+k"`), so a status bar
   can say so.
3. **Scopes, from the focus outward.** The focused node's `keys`, then its parent's, up to the root, then the
   document's `keys`. An overlay is inside its document parent, so a menu's keys win over the page's while it has the
   focus. When nothing has the focus, only the document's `keys` apply.
4. **Inside a scope,** the key's value is tried in order (a list first to last). A binding is skipped if its `when` is
   false or its command is disabled, and resolution goes on, to the next item and then the next scope out.
5. **A press that starts a chord** waits, if any binding that could match in any scope is a chord starting with it;
   a single-press binding of the same key is then shadowed, and the checker says so (below).
6. **Nothing matched:** the node's `onKey` handlers bubble as today.

Today the focused node's keys and the document's keys are matched by two separate hooks (`Events::key` and
`Events::screenKey`); the implementation (8.1a) makes them one walk in this order, without changing what existing
screens do.

**The checker** reports, for every scope: two bindings of the same key whose `when` is missing or written the same
(`keys.mod+s: bound twice in the same scope ('files.save' and 'files.saveAll'): give one a "when"`); a single press
shadowed by a chord that starts with it; a binding whose `do` names an unknown action; and a `when` that doesn't
parse.

### Context keys: what `when` reads

`when` and `enablement` are ordinary expressions (`{{ }}`). They read everything expressions read (`state`,
`computed`, `settings`, ...) and one new variable, **`context`**, which the runtime keeps up to date:

| Context key | Value |
|---|---|
| `context.focus` | The `id` of the focused node, or `null` |
| `context.focusPath` | The ids from the root to the focused node: `contains(context.focusPath, 'sidebar')` is "the focus is in the sidebar" |
| `context.textInputFocus` | A text field has the focus |
| `context.overlay` | The `id` of the topmost open overlay, or `null` |
| `context.chord` | The presses of a chord in progress (`"mod+k"`), or `null` |
| `context.platform` | `"mac"`, `"windows"` or `"linux"` |
| `context.<name>` | A key a node declares, or a widget offers, while the focus is inside that node (below) |

**Nodes declare context keys** with a `context` key, holding names and values (values may be bound). They are set
while the focus is inside the node, and an inner node's value wins over an outer one's:

```json
{ "id": "editor", "type": "code", "context": { "editorFocus": true, "editorLanguage": "{{file.language}}" } }
```

That is how a workbench says "the editor has the focus" without any built-in notion of an editor:
`"when": "{{context.editorFocus && context.editorLanguage == 'cpp'}}"`.

**Widgets offer context keys.** A node type can name keys it keeps up to date, through a new optional hook of the
type registry (`TypeRegistry`, next to the value hooks): `editor` offers `hasSelection` and `readOnly`; the `tree`
and `list` of 8.2b will offer `listFocus` and `selectionCount`. A registered application type does the same through
`Screen::registerType`. They are visible while the focus is in a node of that type, like a node's own `context`.

**Selection, and anything else the app knows,** is state: a `when` can read `state.selection` or a `computed` value.
Prefer a named `computed` value to a long `when`: it is evaluated once per change, reads like a sentence
(`"when": "{{computed.canPaste}}"`), and the checker checks it.

`context` is rebuilt only when the focus moves, an overlay opens or closes, or a chord starts or ends (a walk up the
focus path, at most the tree's depth). Its keys are read like any variable, so the checker knows the built-in names
and the names a document declares; a name declared nowhere is reported (`'context.editorFocsu' isn't declared by any
node (did you mean 'context.editorFocus'?)`).

### Menus: contribution points

A **menu** is a named list of items that any part of the document, and any feature, can add to. The app declares
which menus exist by naming them in a `menus` section:

```json
"menus": {
	"editor.context": {
		"groups": ["navigation", "edit", "file"],
		"items": {
			"editor.goToDefinition": { "group": "navigation" },
			"clipboard.cut": { "group": "edit", "order": 1 },
			"clipboard.copy": { "group": "edit", "order": 2 },
			"refactor": { "submenu": "editor.refactor", "title": "Refactor", "group": "edit" },
			"files.save": { "group": "file", "when": "{{computed.activeFile}}" }
		}
	},
	"editor.refactor": { "items": {} },
	"statusBar.right": { "items": {} }
}
```

| Key | Meaning |
|---|---|
| menu name | Words joined by dots, a namespace first: `editor.context`, `view.title`, `statusBar.right`. Not slashes: a `/` needs `~1` in every JSON Pointer that addresses it |
| `title` | The menu's own title, for a menu bar or a submenu (optional) |
| `groups` | The order of the groups. Groups not listed come after, by name. Separators are drawn between groups |
| `items` | Item name → item. **The name is the command it runs** unless the item says otherwise (`run`, `submenu`) |
| item `run` | The command to run, when it isn't the item's name. `with` passes it variables |
| item `submenu` | Another menu's name, shown as a submenu; the item then needs a `title` |
| item `group`, `order` | Its group, and its place in the group (numbers; then the title, alphabetically) |
| item `when` | Shown only when truthy |
| item `title`, `icon` | Override the command's |

- **Items are keyed, not listed.** An item is addressed by its name (`/menus/editor.context/items/files.save`), so
  adding, moving or removing one is a one-line edit that doesn't depend on its position, two features can't collide
  on an index, and a reviewer reads `set /menus/editor.context/items/files.save to {"group": "file"}`.
- **A menu is a contribution point.** An app offers it by declaring it, even empty (`"statusBar.right": { "items":
  {} }`). An item in a menu nobody declared is a checker finding, and RFC 0004's features `require` menus by name.
- **Menus are data, not widgets.** The `menus` variable turns each menu into a list ready to draw (below). A menu bar,
  a context menu, a toolbar, a status bar or a native macOS menu (2.7) reads the same list, so one declaration feeds
  every surface. The widgets that draw them are 8.2c's.

### Settings: a schema, values in state, a generated page

```json
"settings": {
	"editor.fontSize": { "type": "number", "default": 14, "minimum": 6, "maximum": 72,
	                     "description": "The editor's font size, in pixels." },
	"editor.wordWrap": { "enum": ["off", "on", "bounded"], "default": "off" },
	"workbench.colorTheme": { "enum": ["light", "dark"], "default": "light", "title": "Color Theme" }
},
"persist": ["state.settings"]
```

| Key | Meaning |
|---|---|
| setting name | Words joined by dots, a namespace first, as for commands. No name may be the start of another (`editor` and `editor.fontSize` can't both be settings) |
| `type`, `enum`, `minimum`, `maximum`, `pattern`, `items` | A JSON Schema for the value: the same subset typed state uses (4.4b), so the checker, revisions and the page share one validator |
| `default` | Required; must satisfy the schema |
| `title` | Default: the last part of the name, in words (`fontSize` gives "Font Size") |
| `category` | Default: the first part of the name, in words (`editor` gives "Editor"), so the page shows "Editor: Font Size" |
| `description`, `order`, `enumTitles` | For the settings page: a sentence, a place within the category, labels for an `enum`'s values |

**Values.** The value of every setting is read from the **`settings`** variable, nested by the dots:
`{{settings.editor.fontSize}}`. It is the setting's value in `state.settings` if the user changed it, else its
`default`. Only changed values are stored, flat, by full name: `state.settings` is `{"editor.fontSize": 16}`. That
keeps the stored file small, lets a new default reach everyone who never changed it, and makes a reset a removal.

**Changing a value.** Steps may write paths under `settings`, which go to `state.settings`, checked against the
setting's schema:

```json
{ "set": "settings.editor.fontSize", "to": 16 }
{ "toggle": "settings.editor.minimap" }
{ "remove": "settings.editor.fontSize" }
```

- `toggle` and `increment` start from the current value, which may be the default.
- `remove` resets to the default.
- The segments after `settings` are joined by dots to name the setting, so a page can write any setting by name:
  `{ "set": "settings[setting.name]", "to": "{{event.value}}" }`.
- A path that names no setting, or a value the schema refuses, is an error that names the place:
  `onClick[0].set: no setting 'editor.fontSiz' (did you mean 'editor.fontSize'?)`, `... 'editor.fontSize' must be
  at most 72`.

**Persistence stays a capability of the screen file.** Settings are kept across restarts only if the file's `persist`
lists `state.settings`, as for any state. A revision can add a setting (a new key in a path that is already
persisted, like pushing an item onto a persisted list), but can't make the screen write anything new to disk.

**Revisions** that change `/state/settings/...` are checked against the schema like any typed state (4.4b), and
permission rules apply to them as to any state path.

### The variables palettes, menus and pages bind to

All five are read-only, built when first read after the document or the state changed (like `computed`), and listed
in the catalog:

| Variable | Value |
|---|---|
| `commands` | Every command, by category then title: `{ name, title, category, icon, description, enabled, key, keyLabel }`. `key` is the first binding that runs it in the document's `keys` (`"mod+s"`), and `keyLabel` that key as the platform writes it (`"⌘S"` on macOS, `"Ctrl+S"` elsewhere) |
| `keybindings` | Every binding, in resolution order: `{ key, keyLabel, command, title, when, scope }`. `scope` is `"screen"` or the node's `#id`; `command` is the action name when `do` names one, else `null` |
| `menus` | Menu name → its items ready to draw, `when` applied and sorted: `{ name, title, icon, enabled, key, keyLabel, group, separator, submenu, run, with }`. `separator` is true on the first item of a group after another group |
| `settings` | Every setting's value, nested by the dots |
| `settingsList` | Every setting, by category then `order` then title: `{ name, title, category, description, type, enum, enumTitles, minimum, maximum, default, value, modified }` |

### A command palette, a menu, a keybinding editor and a settings page are components

None of these is a built-in widget. Each is an ordinary component over the variables above, which the prototype
proves on today's engine (`prototypes/0001-contributions/today.screen.json`, where the lists are kept in the state by
hand because the variables don't exist yet):

- **The command palette** is an overlay with a `textarea` and a `column` repeating over a filtered `commands`.
  Enter runs the first match:

  ```json
  "computed": { "paletteItems": "{{filter(commands, it.enabled && contains(lower(it.category + ': ' + it.title), lower(state.query)))}}" },
  ...
  { "id": "paletteItem", "repeat": { "for": "command", "in": "computed.paletteItems", "key": "{{command.name}}" },
    "use": "commandRow", "with": { "command": "{{command}}" },
    "onClick": [{ "set": "state.palette", "to": false }, { "run": "{{command.name}}" }] }
  ```

  With 8.3, `filter` and `contains` give way to the quick pick's fuzzy `match` and its highlights; the palette stays a
  component.
- **A menu** repeats over `menus['view.appearance']`, draws a separator where `item.separator` is true, and runs
  `item.run` with `item.with` on click. A context menu (8.2c) takes the same list as its `items`.
- **The keybinding editor** is a table over `keybindings`, with a search box. "Record a key" is a `textarea` whose
  `onKey` collects `event.shortcut` (twice for a chord). Saving is a `patch` step that the screen file holds, which
  writes the user's binding into the document's `keys`: the user's keybindings are revisions, with history, undo,
  review and export as a change set, rather than a second file (open question 2).
- **The settings page** repeats over `settingsList` and picks a control by `type` with `when`: a `toggle` for a
  boolean, a `slider` or a number field for a number, a row of choices or a dropdown for an `enum`, a `textarea` for
  text; a search box filters by title and description; a "modified" dot and a reset button use `modified` and
  `{ "remove": "settings[setting.name]" }`.

**Where they live.** The library ships them as a document of components (a palette, a menu, a keybinding editor, a
settings page) that any screen imports by name once RFC 0004's imports exist, and that an app restyles with its
design tokens or replaces. Until then they are an example file to copy. They contain only format constructs, so an
agent can change them like the rest of the app, and a reviewer can read them.

### What an agent writes

Adding a command with a shortcut and a menu entry is three small edits in one revision:

```json
{ "type": "revision", "author": "agent", "message": "Add Toggle Word Wrap with Alt+Z and a View menu entry",
  "set": {
	"/actions/view.toggleWordWrap": { "title": "Toggle Word Wrap", "category": "View", "do": { "toggle": "state.wordWrap" } },
	"/keys/alt+z": "view.toggleWordWrap",
	"/menus/view.appearance/items/view.toggleWordWrap": { "group": "editor" }
  } }
```

The reviewer sees three lines, each readable on its own:

```
set /actions/view.toggleWordWrap to {"title": "Toggle Word Wrap", "category": "View", "do": {"toggle": "state.wordWrap"}}
set /keys/alt+z to "view.toggleWordWrap"
set /menus/view.appearance/items/view.toggleWordWrap to {"group": "editor"}
```

The command then appears in the palette, the menu shows it with its key, the keybinding editor lists it, and its
title can be changed in one place. A conditional shortcut is one edit too:
`"/keys/alt+z": { "when": "{{context.editorFocus}}", "do": "view.toggleWordWrap" }`.

### The catalog, the checker and the MCP server

- **The catalog** lists the new sections, the command keys, the binding forms, the built-in context keys, the five
  variables and the steps' `settings` paths, from the registries (7.10 generates the schema and the guide from them).
- **The checker** learns commands (actions), menus and settings: it reports an item, binding or `run` naming an
  unknown action; an item in an undeclared menu; a `submenu` naming an unknown menu; `settings.x` reads and writes of
  unknown settings; a `default` its schema refuses; undeclared `context` keys; and the keybinding conflicts above.
- **The MCP server** needs nothing new: `get_screen` with `at: "/actions"` or `"/menus"` reads them. An optional
  `get_commands` tool (the `commands` list, about 15 tokens a command) would let an agent answer "what can this app
  do?" without reading every action's steps (open question 6).

### The C++ side

- `Screen::runAction(name)` already runs a command. A host that builds native menus (2.7) reads the `menus` and
  `commands` variables through a new read-only `Screen::variable(name)` (or the catalog), and runs items with
  `runAction`.
- The type registry gains the optional context-key hook described above. Nothing else in the public API changes.

## The alternatives

**1. A new `commands` section, beside `actions`.** VS Code keeps commands (ids with titles) apart from the code that
runs them. Here it would be `"commands": { "view.toggleWordWrap": { "title": "...", "run": "view.toggleWordWrap" } }`
next to the action.
- *For:* `actions` keeps one shape; a command could point at a native action that the document doesn't define.
- *Against:* every command is written twice, under two names that must stay in step; the checker needs a new
  cross-reference; the measured edit is 88 tokens against 74; and "is this action a command?" is answered in another
  section. A native action can still be a command: `{ "title": "Pick a file", "do": "pickFile" }`.
- **Recommended: extend `actions`.** One name, one place; the card's own wording ("actions that gain a title").

**2. A `keybindings` list, as in VS Code's `keybindings.json`.** `[{ "key": "alt+z", "command": "...", "when":
"..." }]`, priority by position.
- *For:* familiar to VS Code users; several bindings per key are natural.
- *Against:* items are addressed by index, so edits depend on position and two features appending can't be told
  apart in review; it duplicates `keys`, which already exists on the document and on nodes; node-scoped bindings
  would need a `scope` field instead of locality. Measured edit: 84 tokens against 74.
- **Recommended: keep `keys`,** with bindings and lists for the cases a list exists for.

**3. The key on the command** (`"key": "mod+s"` inside the action). Short, but a key then has two homes (the command
and `keys`), node-scoped keys can't use it, and the user's own bindings would still need `keys`. Rejected.

**4. Menus as node trees, as today.** Full control of the look, but no feature can add to a menu it didn't write,
native menus can't be generated, and each entry costs a styled node (163 tokens measured). Rejected as the way to
declare menus; a hand-built node tree stays possible for anything that isn't a menu.

**5. Settings as typed state alone (4.4b), without a section.** Typed state gives types, but not titles,
descriptions, categories or defaults that stay out of the stored values, so no page can be generated and a reset
can't exist. The section reuses 4.4b's schema subset rather than competing with it.

**6. Doing nothing.** Every app keeps its own lists in state and builds its own menus, as the prototype does: it works
(the prototype renders), but every command lives in three to four hand-kept places, menus can't be contributed to,
and the cost of a command grows with the number of surfaces that show it.

## The cost in tokens

Measured with `python3 rfcs/prototypes/tokens.py rfcs/prototypes/0001-contributions` (tiktoken `o200k_base`, the
stand-in `tests/perf/tokens.py` uses; minified JSON, notes left out). Compare ratios: Claude's tokenizer isn't public.

| Agent edit | Today | With this RFC |
|---|---|---|
| A command with a shortcut and a menu entry, in the palette too | 118 (lists kept in the state by hand); 163 (a menu node, and no palette entry) | **74** (three edits, message included) |
| The command alone | 20 (an action without a title, in no palette or menu) | 30 |
| A shortcut | 16 | 16 |
| A shortcut only while the editor has the focus | not expressible (an `if` in the handler consumes the key) | 27 |
| A menu entry | 29 (an item kept in the state by hand); 116 (a styled node) | 20 |
| A setting, with its page entry | not expressible (state plus a hand-built row) | 31 |
| A user rebinds a key (one removed, one added) | 27 | 27 |
| First read of the prototype app (whole document) | 2,446 | 2,290 (−6%) |

Alternatives, same edit: a separate `commands` section 88; a `keybindings` list 84.

## Performance

- **Key presses.** Bindings are indexed once per version of the document (with 7.6's compiled nodes): per scope, a
  map from the normalised press to its bindings. A press costs a walk up the focus path (the tree's depth) with one
  map lookup per scope that has `keys`, plus evaluating the `when` of only the bindings of that key. No allocation
  per press in steady state. Today a press scans every entry of the document's `keys` and normalises each one
  (`Events::screenKey`), so this is cheaper than now for screens with many shortcuts.
- **`context`** is rebuilt on focus, overlay and chord changes only: at most the depth of the tree, never per frame.
- **`commands`, `menus`, `keybindings`, `settingsList`** are built when first read after a change, then cached like
  `computed`; reading them costs nothing while nothing changed. Building `commands` evaluates each command's
  `enablement`: with 500 commands, about 500 small expressions (an estimate of 0.25 ms from the 488 ns per evaluation
  measured in wave 1, before 7.5 made expressions 4.4× faster), and only on frames where something that binds to
  `commands` is visible and the state changed. Menus are built when
  the screen reads them (an open menu), so a closed menu costs nothing.
- **Settings** add one lookup per read (`state.settings` then the default), cached in the `settings` variable.
- **What 8.1a and 8.1b must measure:** a key press with 500 bindings in the document and a focus 30 levels deep
  (target: under 20 µs); building `commands` with 500 commands (under 0.5 ms); and the `update/400` and `load`
  benchmarks unchanged for screens that use none of this (within 1%).

## Security review

- **New input.** Commands, bindings, menus and settings arrive in documents and revisions, which are untrusted.
  Limits: a chord has at most four presses; a binding list at most 64 items; menus nest (`submenu`) at most 8 levels
  and may not form a cycle (the checker and the runtime both refuse one); a settings schema has the nesting limit of
  typed state (4.4b). The `revision` and `json` fuzz targets get seeds with each new form, and a new `keys` fuzz
  target covers shortcut parsing and resolution (8.1a).
- **No new capability.** Nothing here reaches outside the document. Commands are actions, so the existing guard
  holds unchanged: a revision can't add, change or move a control step, nor add a way to run one.
- **New ways to run an action must be guarded.** A menu item and a binding are triggers, like `onClick` and `keys`
  today. `checkChange` must refuse a revision that adds or changes a menu item or a binding whose command holds
  control steps (directly or through the actions it runs), names a native action, or names a computed action. This
  is the existing rule ("a way to run a control step") extended to `menus` and the new binding forms, and 8.1a and
  8.1b each add unit tests for the refusal in `tests/unit/rules.cpp`.
- **`enablement` and `when` are expressions:** pure, with the expression nesting limit, and they can't change state.
- **Running commands from a palette is the user's own input.** The palette's `run` with a computed name is written in
  the screen file (or the trusted component library), which may do so; a revision still can't add a computed `run`.
  While a proposal is previewed, control steps run only from the user's own input, and a palette pick or a menu
  click is that input.
- **Misleading changes stay reviewable.** An agent could rebind `mod+s` to another command, or give a harmless
  title to a command that deletes things. Neither widens a capability, and both arrive as one readable line for the
  reviewer (`set /keys/mod+s to "files.deleteAll"`). A screen that wants to stop it denies agents `/keys` or
  `/menus` with a permission rule; the templates already send agents' changes to review.
- **Settings values are state:** permission rules govern revisions to `/state/settings`, and the schema bounds what
  is stored. Persisting them is still granted only by the screen file's `persist`. No secret belongs in a setting
  (RFC 0003 keeps secrets out of documents and state).
- **No new callbacks or timers.** A chord in progress has no timer. The `context` keys are rebuilt synchronously on
  focus changes; nothing outlives the runtime.

## Compatibility

- **Additive within format version 1.** Every screen that loads today loads unchanged and means the same: an action
  in the object form, a binding object, a list of bindings, `menus`, `settings`, the node key `context` and the
  variables are all new. One rule tightens: an action object with no step key used to be an "unknown step" error, and
  becomes a command.
- **Variables.** `context`, `commands`, `keybindings`, `menus`, `settings` and `settingsList` become reserved names: a
  component parameter, `let`, `repeat` or `with` variable of the same name would shadow them. No template uses these
  names; the checker warns where a document does.
- **History** stays RFC 6902: the new sections are ordinary document keys. **Change sets** that add commands or menu
  items apply to any copy of the screen that declares the menus they use.
- **The schema, `SCREEN_FORMAT.md` and the catalog** change together (generated from the registries once 7.10 lands).
- **The public C++ API** stays source-compatible: one optional hook in the type registry, one optional read.

## What it costs

| | |
|---|---|
| **Tokens** | A command with a shortcut and a menu entry: 74 tokens, against 118 to 163 today, and it reaches the palette and every menu. Reads shrink a little (−6% on the prototype), because hand-kept lists disappear |
| **Speed** | Key presses get cheaper (indexed bindings). Commands and menus are built lazily and cached. Screens that use none of it pay nothing (to be measured by 8.1a and 8.1b) |
| **Security** | No new capability. `checkChange` gains two triggers (menu items, the new binding forms) under the existing "a way to run a control step" rule |
| **`SCREEN_FORMAT.md`** | Rows for the action object, binding forms and chords, the `context` node key, two sections (`menus`, `settings`), five variables, `settings` paths in steps, and a "Keyboard resolution" paragraph replacing the current "Shortcuts" one |

## Open questions

1. **Extend `actions`, or add a `commands` section?** *Recommendation:* extend `actions` (alternative 1). Cheaper
   edits, one name per command, and no cross-reference to keep in step.
2. **Where do the user's own keybindings live?** In the document's `keys`, written by the keybinding editor's `patch`
   step (a revision: history, undo, export as a change set, reviewable), or in persisted state (a second layer that
   overrides `keys`). *Recommendation:* the document. It needs no second resolution layer, and "the user customises
   the app" is exactly what version control is for. Settings values stay in state, because they are data.
3. **Menu names with dots, or VS Code's slashes?** *Recommendation:* dots (`editor.context`): a slash costs `~1` in
   every pointer and every reviewer line.
4. **Should a disabled command's binding fall through to the next binding and scope, or consume the key?**
   *Recommendation:* fall through. It lets an outer binding of the same key apply when the inner command can't run.
   VS Code consumes it in some cases; falling through is easier to predict.
5. **Should `enablement` also stop `run` steps?** *Recommendation:* no, as in VS Code: it guards the user's ways in
   (keys, menus, palettes). A screen's own logic that wants the check writes an `if`.
6. **An MCP `get_commands` tool?** *Recommendation:* yes, small and read-only, once 8.1a lands: it answers "what can
   this app do?" in about 15 tokens per command, without the steps.
7. **Where the standard components live.** *Recommendation:* a document of components shipped with the library and
   imported by name (RFC 0004's imports), not C++ widgets, so they are restyled and changed like any part of an app.
8. **Setting scopes** (per workspace, per language, per window). *Recommendation:* a later RFC, once the document
   server (8.8) defines what a workspace is. The flat storage by full name leaves room for `"[cpp]"`-style override
   objects.
9. **Reading settings nested (`settings.editor.fontSize`) but storing them flat.** *Recommendation:* as designed:
   nested reads are the short, familiar form in bindings; flat storage keeps resets, defaults and diffs simple, and
   no name may be the start of another, so the two never disagree.
