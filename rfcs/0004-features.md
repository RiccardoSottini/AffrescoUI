# RFC 0004: Features as packages

- **Issue:** none yet (written for roadmap item 8.7, wave 3; uses 4.6's imports; decides the format for D12's
  signatures)
- **Author:** the `3-rfcs` fleet agent (Claude), for the maintainer
- **Changes:** the screen format; the live protocol and the MCP tools (`installFeature`); the security model
  (who may install what, signatures)
- **Prototype:** [`prototypes/0004-features/`](prototypes/0004-features/README.md): an app with two contribution
  points and two exports, a word count and a TODO tree as packages, a broken package, and `compose.py`, which checks a
  package against the app, writes the install revision, and expands the result into a screen today's engine checks
  and renders
- **Builds on:** RFC 0001 (commands, menus, settings), RFC 0002 (services, models), RFC 0003 (connections)

## The problem

A large app is a set of features: an explorer, search, source control, a terminal, a word count, a TODO list, a
layers panel, a slide sorter. People want to add one, switch it off, remove it, get a newer version, and take a
feature from one app into another ("add the slide sorter to my image editor"). Agents build features on request and
should be able to hand them to someone else.

Today AffrescoUI has **change sets** (`VERSION_CONTROL.md`): the commits after a point, exported and imported all at
once. A change set is a good way to share a customisation, but not a feature:
- it says nothing about what it **needs** (a status bar, the text being edited, a search service) or what it
  **provides**, so nothing can tell, before applying it, whether it fits another app;
- its parts land **mixed into the app's own sections** (an action here, a state key there, a node in the status bar),
  so it can't be switched off or removed as a unit, and a reviewer can't see where it begins and ends;
- names can **collide** with the app's or another change set's (two `toggle` actions);
- it has **no version**, no publisher and no signature, so it can't be upgraded or trusted.

The prototype measures the difference on a word count: added by hand it is 150 tokens of edits in four sections;
switching it off is four removals (28 tokens) that must be remembered and replayed to switch it back on.

**Who needs it:** any app that grows by features, and the people who share them: an IDE's extensions, a creative
tool's panels and filters, a dashboard's widgets, a game launcher's plugins.

**Out of scope here:** a marketplace (a service, not a format); paid features; running untrusted native code (a
feature's native logic is a service or provider the host grants, RFC 0002); live co-editing of a feature by two
people.

## The design

| Part | What it is |
|---|---|
| **A package** | An `affrescoui-feature` manifest: a name (its namespace), a release (semver), what it **requires** and what it **provides** |
| **The app's side** | **Contribution points** (nodes marked `point`, menus from RFC 0001, setting categories) and **exports** (named values features read as `app.<name>`) |
| **The `features` section** | One record per installed feature: its manifest, where it came from, `enabled` and `order`. Installing, switching, ordering, upgrading and removing are edits to it, so each is a reviewable revision |
| **Composition** | Checks that run before a person sees the proposal: requirements met, names in the feature's namespace, no conflicts, no capability, and the checker's proof on the merged screen |
| **Signatures and the catalogue** | Ed25519 signatures over canonical JSON (D12), and a signed catalogue index |
| **Shared points** | Contribution point and export names that unrelated apps agree on, so a feature written for one fits another, with adapters where they differ |

### A package

The word count, whole (`prototypes/0004-features/word-count.feature.json`):

```json
{
	"format": "affrescoui-feature",
	"version": 1,
	"name": "wordCount",
	"release": "1.0.0",
	"title": "Word count",
	"description": "Shows the words (or characters) of the text being edited in the status bar; click to switch.",
	"license": "Apache-2.0",
	"requires": {
		"format": 1,
		"points": ["statusBar.right"],
		"exports": { "editor.text": { "type": "string" } }
	},
	"provides": {
		"state": { "characters": false },
		"computed": {
			"words": "{{len(filter(split(join(split(app.editor.text, '\\n'), ' '), ' '), len(trim(it)) > 0))}}",
			"characters": "{{len(app.editor.text)}}"
		},
		"actions": {
			"toggle": { "title": "Toggle Word or Character Count", "category": "Word Count", "do": { "toggle": "state.wordCount.characters" } }
		},
		"menus": { "statusBar.context": { "items": { "toggle": {} } } },
		"views": {
			"statusBar.right": {
				"wordCount-label": {
					"style": "status", "cursor": "hand", "onClick": "wordCount.toggle",
					"text": "{{state.wordCount.characters ? plural(computed.wordCount.characters, 'character') : plural(computed.wordCount.words, 'word')}}"
				}
			}
		}
	}
}
```

| Key | Meaning |
|---|---|
| `format`, `version` | `"affrescoui-feature"`, `1` (the manifest format's version, like a screen's) |
| `name` | One camelCase word: the feature's **namespace** (`wordCount`) |
| `release` | The feature's own version, semver (`1.0.0`) |
| `title`, `description`, `license`, `publisher`, `homepage`, `note` | For people and catalogues |
| `requires` | What must exist for it to work (below) |
| `provides` | What it adds (below) |
| `scenarios` | Its automation scenarios (7.4), run when it is installed and in the catalogue's checks |
| `signature` | Ed25519, over the rest of the manifest (below) |

**`requires`:**

| Key | Meaning |
|---|---|
| `format` | The screen format version it is written for |
| `points` | Contribution points it puts views or items into: node points (`statusBar.right`), menus (`editor.context`) |
| `exports` | Values it reads, each with a JSON Schema (`{ "type": "string" }`); the app's export must match |
| `models` | Data models it reads (RFC 0002), each with the shape it needs |
| `services` | Services and methods it invokes (RFC 0002): `{ "search": ["find"] }` |
| `connections` | Connections and operations it requests (RFC 0003): `{ "api": ["listProjects"] }` |
| `features` | Other features and the releases it works with: `{ "wordCount": "^1.0.0" }` (`^`, `~`, `>=` or exact) |

**`provides`:** `state` (its own state, at `state.<name>`), `computed`, `actions` (commands too, RFC 0001),
`components`, `styles`, `keys`, `menus` (items for menus the app declares), `settings` (RFC 0001), `views` (nodes
for node contribution points), `exports` (values it offers other features, for adapters).

### Namespaces

A feature's name is its namespace, and a package can only add names inside it:

| What | Declared in the manifest as | Used everywhere (by the feature, the app, agents and reviewers) as |
|---|---|---|
| state | `"state": { "characters": false }` | `state.wordCount.characters` |
| computed | `"computed": { "words": ... }` | `computed.wordCount.words` |
| actions, commands | `"actions": { "toggle": ... }` | `wordCount.toggle` |
| components, styles | `"components": { "badge": ... }` | `"use": "wordCount.badge"` |
| settings | `"settings": { "showCharacters": ... }` | `settings.wordCount.showCharacters` |
| menu items | `"items": { "toggle": {} }` | the item `wordCount.toggle` |
| node ids | written in full: `"wordCount-label"` | `#wordCount-label` |

- **Declarations are local, references are full.** A reviewer reads `wordCount.toggle` wherever it is used, so a name
  says whose it is; and a package's declarations can't step outside its namespace.
- **Node ids use a hyphen** (`wordCount-label`), because a dot in an id would clash with anchors' `#id.edge`.
- **Keys (shortcuts) are global,** since a key press has one meaning; RFC 0001's checker reports a feature's
  binding that collides with the app's or another feature's in the same scope without a `when` to tell them apart.
- **The app's own names stay unprefixed.** A feature can't use a namespace that an installed feature, or the app's
  own names (`wordCount.*` actions, a `state.wordCount` key), already use.

**Imports (4.6).** A component library is a feature that provides only components (and styles). A screen imports it
under a short name:

```json
"imports": { "ui": "workbench@^1" }
...
{ "use": "ui.palette", "with": { "items": "{{commands}}" } }
```

`imports` maps a local alias to an installed feature and a range; `ui.palette` then means that feature's `palette`
component. It is how RFC 0001's standard palette, menu, keybinding editor and settings page are shared, and how a
change set records which version of a library it was made against (the alias's range is in the document).
Component-local state and screens with parameters, the rest of 4.6, are separate format items.

### The app's side: contribution points and exports

```json
{ "id": "statusRight", "point": "statusBar.right", "type": "row", "from": "end", "gap": 4, "children": [] }
```

```json
"exports": {
	"editor.text": { "type": "string", "value": "{{state.files[state.active].text}}", "description": "The text of the file being edited." },
	"workspace.files": { "type": "array", "value": "{{state.files}}" }
}
```

- **`point`** on a node names a **node contribution point**: features' `views` for that point become its children,
  after the app's own, in the features' `order`. A point has one name; two nodes with the same point are a checker
  finding. RFC 0001's menus are contribution points too (a feature's `menus` items join a declared menu), and so are
  setting categories.
- **`exports`** names values the app offers: an expression, a JSON Schema of its type, and a sentence. Features read
  them as `app.<name>` (nested by the dots, like settings), never the app's state directly, so the app can change how
  it stores things without breaking its features, and a feature can move to another app that exports the same name.
- **The app decides what it offers.** Points and exports are ordinary document keys that revisions may add, so an
  agent can open an app to a feature ("give the status bar a point for features") as a reviewed change.

### The `features` section, and how the runtime merges it

Installing a feature writes **one record**:

```json
"features": {
	"wordCount": {
		"release": "1.0.0",
		"source": "catalog:https://features.example.com/index.json",
		"digest": "sha256:9f2c…",
		"publisher": "ed25519:4b1e…",
		"enabled": true,
		"order": 10,
		"requires": { … },
		"provides": { … }
	}
}
```

- **The manifest is kept whole in the record,** so the document is self-contained: history, diffs, blame, change sets
  and pictures of proposals need nothing outside it, and `source`, `digest` and `publisher` say where it came from.
- **The runtime merges** every `enabled` record into the screen it builds, in `order` (then name), after
  dependencies: state, computed, actions, components, styles, keys, menu items, settings and views, under their full
  names. With compiled nodes (7.6) the merge happens once per version, and each feature's part is cached by its
  record's hash, so a change to one feature recompiles only that feature.
- **`#id` paths reach into features.** `#wordCount-label/textColor` finds the node inside its record
  (`/features/wordCount/provides/views/statusBar.right/wordCount-label/textColor`), so agents edit an installed
  feature exactly as they edit the app (`Rules::findNode` searches the records too).

| Operation | The revision | Tokens (prototype) |
|---|---|---|
| Install | `set /features/<name>` to the record (the host writes it from the catalogue) | 225 for the word count's record; an agent asks with 20 (below) |
| Switch off, on | `set /features/wordCount/enabled false` | 14 |
| Order | `set /features/wordCount/order 5` | 13 |
| Upgrade | `set /features/wordCount` to the new release's record, with local edits carried over (below) | the record |
| Remove | `remove /features/wordCount` (its state goes with it; settings values the user changed are kept) | 10 |

**Who installs.** An agent asks the host rather than writing a manifest out:

```json
{ "type": "installFeature", "feature": "wordCount", "release": "^1.0.0" }
```

The host finds it in its catalogues, verifies the signature, resolves the features it requires (installing them in the
same revision, all or nothing), runs the composition checks, and submits the revision as the asking peer, so the
permission rules and review apply as to any change. MCP gets an `install_feature` tool for it. A peer may also send a
record in a plain revision (an agent's own feature, unsigned), which goes through the same checks.

**Upgrades keep local edits.** If the record was edited after install (an agent restyled `#wordCount-label`), the
upgrade computes the JSON Patch from the installed release's manifest to the record and applies it to the new
release's manifest. If an operation doesn't apply (the node is gone in the new release), the upgrade is refused with
that operation named, and the person decides (open question 5).

### Composition: checks and proofs

Before a proposal reaches a reviewer, the install is checked, and every problem names its place in the manifest.
The prototype's `compose.py check` on a package that is wrong in every way:

```
broken.feature.json: 9 problems
  broken.requires.points: the app has no contribution point 'toolbar.main' (it has: sidebar.views, statusBar.right)
  broken.requires.exports.editor.text: needs a number, the app exports a string
  broken.requires.exports.selection.items: the app doesn't export it
  broken.requires.services.git: the app declares no service 'git' (a host must grant it: a revision can't)
  broken.requires.features.todoTree: needs ^2.0.0, 1.2.0 is installed
  broken.provides.services: a capability: only the host can grant it, never a revision
  broken.provides.actions.upload.do: 'call' is a control step: a feature that uses one is installed by the host, not by a revision
  broken.provides.views.statusBar.right.statusLabel: id 'statusLabel' must start with 'broken-'
  broken.provides.keys.mod+1: the app binds it already (give one a "when")
```

The checks, in order:
1. **The manifest** is valid (its own schema, with the nesting limits of a revision).
2. **Requirements:** each point, export (with a compatible type), model (with a compatible shape), service and method,
   connection and operation exists; each required feature is installed in a release its range allows; the format
   version matches. No dependency cycles.
3. **Namespaces:** every declaration lands inside the feature's namespace; no namespace or id is taken.
4. **Capabilities:** no capability section (`services`, `connections`, `channels`, `persist`, `permissions`) and,
   for an install by revision, no control step (`call`, `send`, `connect`, `patch`, `respond`, review steps), exactly
   as `checkChange` rules any revision.
5. **Conflicts:** keybindings (RFC 0001's checker), and two features putting the same item into one menu.
6. **The checker's proof:** the merged screen has no finding the screen without the feature didn't have: every
   reference resolves (state paths, computed values, actions, components, styles, fonts, colours, exports), with
   typed state (4.4b) types match too. This is the existing rule for revisions ("a revision can't bring new
   findings"), applied to the merged screen, so it **proves the composition fits before anyone sees it**.
7. **Its scenarios** (7.4) run against the merged screen headless, and their results go with the proposal (5.2).

The prototype runs checks 2 to 5 itself and proves 6 with the real checker: `compose.py expand` writes the merged
screen in today's format, and `affrescoui check` finds no problems in it (the word count and the TODO tree together).

### Who may install what

| A feature that | Installed by |
|---|---|
| provides only declarative parts (nodes, state, computed, actions without control steps, components, styles, keys, menus, settings) and **uses** services, connections and models the app already declares | a revision, under the permission rules and review, like any change |
| needs a **capability** the app doesn't declare (a service, a connection, a channel, persisted state), or contains **control steps** | only the host: the Studio's Features screen or `affrescoui import --feature`, which act with the host's authority (like `--plugin` change sets today), show the person each capability it asks for ("starts the program `rg`, reads files in `workspace`"), and still apply the deny rules |

This keeps the line drawn for every capability: granted by the host, never widened by a revision. Using what the
app already declares follows RFC 0002's and RFC 0003's rule (writes always reviewed).

### Signatures (D12)

- **Algorithm:** Ed25519 through Monocypher (CC0 or BSD-2-Clause, two files, no other dependency), as D12
  recommends.
- **What is signed:** the manifest without its `signature` key, as canonical JSON (RFC 8785, the JSON
  Canonicalization Scheme), so formatting changes don't break it and any content change does.
- **`signature`:** `{ "publisher": "ed25519:<public key, base64url>", "value": "<signature, base64url>" }`.
- **Trust:** the host keeps the publishers it trusts (C++: `screen.features().trustPublisher(key, name)`, or a file
  in the data folder). The record keeps `digest` (SHA-256 of the canonical manifest) and `publisher`.
- **Policy:** installs from a catalogue must be signed by a trusted publisher; a tampered manifest (a digest or
  signature that doesn't match) is refused with the reason. An unsigned feature written by an agent may be installed
  by revision (it is reviewed like any change, and its record says `"publisher": null`); a host may require
  signatures for everything.
- **Keys** are the publisher's own; the library never holds a private key. A `affrescoui sign` command signs a
  manifest with a key the publisher passes (from a file or the keychain).

### The catalogue

A catalogue is a signed JSON index any static host can serve (a folder, a web server, a git repository):

```json
{
	"format": "affrescoui-catalog",
	"version": 1,
	"name": "Example features",
	"updated": "2026-10-20T00:00:00Z",
	"publishers": { "ed25519:4b1e…": { "name": "Example Studio" } },
	"features": {
		"wordCount": {
			"title": "Word count",
			"description": "Words or characters of the text being edited, in the status bar.",
			"releases": [
				{ "release": "1.0.0", "url": "word-count-1.0.0.feature.json", "digest": "sha256:9f2c…",
				  "publisher": "ed25519:4b1e…", "requires": { "points": ["statusBar.right"], "exports": ["editor.text"] },
				  "size": 1874 }
			]
		}
	},
	"signature": { "publisher": "ed25519:4b1e…", "value": "…" }
}
```

- `requires` is repeated in the index (names only), so a host or an agent can list **which features fit this app**
  without downloading them.
- Hosts add catalogues in C++ or in the data folder; documents never name one (a catalogue is where code-like content
  comes from, so it is the host's choice, like a capability).
- Limits: an index at most 8 MiB, a manifest at most 1 MiB, at most 10,000 features an index; both parsed with
  `parseUntrusted`.

### Contribution points shared across apps

Features move between apps when the apps agree on names. The library documents a small set of **well-known points
and exports** (in a `CONTRIBUTION_POINTS.md` that grows with the apps), each with the shape of what it accepts:

| Kind | Name | Meaning |
|---|---|---|
| point | `statusBar.left`, `statusBar.right` | Small labels in a status bar |
| point | `sidebar.views`, `panel.views` | Views stacked in a side bar or a bottom panel |
| point | `toolbar.main` | Buttons in the main toolbar |
| menu | `editor.context`, `view.title`, `statusBar.context` | RFC 0001 menus |
| export | `document.title`, `document.text` | The open document's title and text |
| export | `selection.items` | What is selected, as a list (`{ "kind", "id", "label" }`) |
| model | `document.pages` | The pages, slides or layers of a document, as rows (`{ "id", "title", "thumbnail" }`) |

- An app **offers** a well-known name by declaring a point or export with that name and shape; the checker checks
  the shape.
- **Adapters** cover the gap when an app has what a feature needs under another shape: an adapter is a small feature
  that `provides.exports` (or a model) under the well-known name, computed from the app's own exports. An agent writes
  it ("map this image editor's layers to `document.pages`"), and it is installed, reviewed and removed like any
  feature. That is how a slide sorter written for a slide tool lands in an image editor: install the adapter, then the
  sorter, in one reviewed revision, both proven by the checker.
- Names are namespaced by convention (`statusBar.*`, `document.*`) and versioned by their shapes: a breaking change to
  a shape is a new name.

### Worked example: a TODO tree for Forge

The Forge template's explorer gains a TODO tree. The prototype's version reads the open files through an export so
it runs today (`todo-tree.feature.json`); the version for a real workspace uses the search service (RFC 0002):

```json
{
	"format": "affrescoui-feature", "version": 1, "name": "todoTree", "release": "2.0.0", "title": "TODO tree",
	"requires": {
		"points": ["sidebar.views", "view.title"],
		"services": { "search": ["find"] },
		"exports": { "workspace.open": { "type": "object" } }
	},
	"provides": {
		"state": { "open": true, "items": [], "scanning": false },
		"computed": { "files": "{{keys(groupBy(state.todoTree.items, 'path'))}}" },
		"actions": {
			"refresh": { "title": "Refresh TODOs", "category": "TODO Tree", "icon": "refresh",
			             "do": [{ "set": "state.todoTree.items", "to": [] },
			                    { "invoke": "search.find", "with": { "pattern": "TODO|FIXME", "regex": true }, "id": "todoTree.scan",
			                      "append": "state.todoTree.items", "keep": 5000, "loading": "state.todoTree.scanning" }] },
			"toggle": { "title": "Show or Hide the TODO Tree", "category": "TODO Tree", "do": { "toggle": "state.todoTree.open" } }
		},
		"keys": { "mod+shift+t": "todoTree.toggle" },
		"menus": { "view.title": { "items": { "refresh": { "group": "navigation" } } } },
		"views": { "sidebar.views": { "todoTree-view": { "type": "column", "children": [ "… a header, and a tree of files and lines …" ] } } }
	}
}
```

`search.find` is a `read` method, so the install is a revision under the permission rules (if Forge declares the
`search` service; otherwise the host must install it). `groupBy` stands for a list function the format would need
(open question 9); the prototype groups by file with a nested `repeat` instead. In the prototype, `todoTree` 1.2.0
also requires `wordCount` `^1.0.0` to show a dependency being checked.

### Worked example: a word count in the status bar

The package above. Installed into the prototype app with the TODO tree, merged and drawn:

```sh
python3 compose.py check   app.screen.json word-count.feature.json todo-tree.feature.json   # both can be installed
python3 compose.py install app.screen.json word-count.feature.json todo-tree.feature.json -o install.revision.json
python3 compose.py expand  app.screen.json word-count.feature.json todo-tree.feature.json -o merged.screen.json
../../../build/affrescoui check merged.screen.json                                          # no problems
```

The merged screen shows "19 words" in the status bar (a click switches to characters) and a TODO tree of three items
under the explorer (a click opens the file; Shift+Cmd+T hides it), checked with the render runner's scripted clicks.

## The alternatives

**1. Expanded installs** (write the feature's parts into the app's sections, as change sets do, plus a small record).
- *For:* the runtime needs no merge step; the document reads as one app.
- *Against:* switching off and removing are many edits (28 tokens against 14 and 10 for the word count) that must be
  replayed exactly; upgrades must find and replace scattered parts; a reviewer can't see where a feature ends; the
  composition can only be re-checked by reconstructing it.
- **Recommended: records,** merged by the runtime.

**2. Change sets as they are.** Already portable and checked against deny rules, but with no requirements, namespace,
version, switch or signature. They stay what they are good for (sharing a customisation of one screen), and a change
set may now contain a feature record.

**3. `include` files.** Merged at load, section by section, with no namespace, no live install and no record in
history. Rejected for features; `include` stays for splitting one app's files.

**4. Code plugins** (VS Code's model: a JavaScript extension host). Rejected by decision 3 (no scripts inside
documents). Native logic arrives as providers and services the host grants (RFC 0002), a WASM provider later.

**5. Automatic dependency resolution across catalogues** (npm-style). The install resolves required features from the
host's catalogues and proposes them in the same revision, but never picks between publishers on its own: if two
catalogues offer a feature, the person chooses.

**6. Doing nothing.** Features stay change sets: they can be shared but not checked for fit, switched off, upgraded or
trusted, and mixing features across apps can't start.

## The cost in tokens

Measured with `python3 rfcs/prototypes/tokens.py rfcs/prototypes/0004-features` (tiktoken `o200k_base`, minified,
notes left out):

| Agent edit | Today | With this RFC |
|---|---|---|
| Add the word count | 150 (edits in four sections, written by the agent) | 20 (`installFeature`; the host writes the 225-token record) |
| Switch it off | 28 (four removals, to be replayed to switch it on) | 14 |
| Remove it | 28 | 10 |
| Restyle a part of it | 15 | 15 |
| Order it among other features | not expressible | 13 |
| The word count's manifest | | 235 |
| The TODO tree's manifest | | 505 |
| Reading which features fit an app | not expressible | the catalogue's `requires` names: 20 for the word count (`{"wordCount":{"requires":{...}}}`) |

## Performance

- **Merging** happens when a version is compiled (7.6), not per update or frame, and each feature's part is cached
  by its record's hash: a revision to one feature recompiles that feature only. A disabled feature costs nothing.
- **Composition checks** run on install and upgrade only. The checker already takes 0.05 to 0.2 ms per template; a
  feature adds what it adds (its nodes and expressions), so the cost follows the feature, not the app.
- **Runtime cost** of an installed feature is the cost of its nodes and computed values, the same as if written by
  hand; `app.<name>` exports are evaluated like `computed` values (cached until the state they read changes).
- **What 8.7a must measure:** compiling the Forge template with 10 features against without (target: within 5% of the
  same parts written by hand); toggling a feature on a large screen (target: one frame).

## Security review

- **New input:** manifests and catalogue indexes from outside (untrusted): size limits (an index 8 MiB, a manifest
  1 MiB), `parseUntrusted`, the revision's depth limit, and the manifest schema. New fuzz targets: the manifest
  checks, the catalogue index, and signature verification with malformed keys and signatures.
- **No new capability through a revision.** A record installed by revision is checked by `checkChange` exactly like
  the parts it adds: no capability sections, no control steps, no way to run one. A feature that needs more is
  installed only by the host, which shows each capability to the person.
- **Self-approval:** a feature can't contain review steps or `patch` when installed by revision; `installFeature`
  submits as the asking peer, so the asking agent's permissions apply and it can't review its own install.
- **Signatures** give provenance, not safety: a signed feature from a trusted publisher still can't widen a
  capability, and an unsigned one is reviewed. Tampering (digest or signature mismatch) is refused. Verification is
  constant-time in Monocypher; keys and signatures are parsed with fixed lengths.
- **Namespace confusion:** a feature can't declare names outside its namespace, take a used namespace or id, or
  shadow the app's names, so an agent can't hide an action named like the app's own inside a feature.
- **Exports** are read-only values; a feature reading `app.editor.text` can't write the app's state except through
  actions the app or the feature declares, under the same rules as any step.
- **Secrets:** none in manifests or catalogues (they are documents, visible to every peer); a feature that needs a
  token uses a connection's sign-in (RFC 0003).
- **Callbacks:** none new; merged parts are ordinary nodes, timers and subscriptions owned by the runtime.

## Compatibility

- **Additive within format version 1:** new sections (`features`, `exports`, `imports`), a new node key (`point`), a
  new variable (`app`), a new manifest format and a catalogue format. Existing screens are unchanged.
- **History** stays RFC 6902: installs, switches and upgrades are edits to `/features`. Change sets may carry
  feature records.
- **`#id` lookup** extends into feature records; ids stay unique across the app and its features (the checker
  already rejects duplicates).
- **The schema, `SCREEN_FORMAT.md` and the catalog** change together; `VERSION_CONTROL.md` gains "Features" next to
  "Change sets"; the live protocol gains `installFeature` (and MCP `install_feature`).
- **The public C++ API** gains `Screen::features()` (catalogues, trusted publishers, install with the host's
  authority); nothing changes. Monocypher is a new dependency (D12).

## What it costs

| | |
|---|---|
| **Tokens** | Installing a feature costs an agent 20 tokens; switching or removing 10 to 14. Manifests are 235 (word count) to 505 (TODO tree) tokens, written once by their author |
| **Speed** | Merged at compile time, cached per feature; disabled features cost nothing; checks run on install only |
| **Security** | Records are checked like the parts they add, so no capability arrives by revision; features that need one go through the host. Signatures (Ed25519, Monocypher) for catalogue installs. Three new fuzz targets |
| **`SCREEN_FORMAT.md`** | A "Features" section (`features`, `exports`, `imports`, `point`, `app.*`, namespaces), the manifest and catalogue formats (perhaps their own `FEATURES.md`), and `CONTRIBUTION_POINTS.md` for the shared names |

## Open questions

1. **Records merged by the runtime, or expanded installs?** *Recommendation:* records (alternative 1): one-line
   switches and removals, a visible boundary for reviewers, and composition that can always be re-checked.
2. **Features that need a capability: host only?** *Recommendation:* yes, through the Studio's Features screen or the
   CLI with the host's authority, showing each capability; never by a peer's revision. It is the existing capability
   line, applied to packages.
3. **Signatures required for every install, or only from catalogues?** *Recommendation:* required from catalogues;
   an unsigned feature an agent wrote may be installed by revision, since it is reviewed like any change. A host may
   require signatures everywhere.
4. **Monocypher for Ed25519 (D12).** *Recommendation:* yes (CC0 or BSD-2-Clause, two source files, audited, no other
   dependency), pinned by hash, in wave 6 when 5.5 builds the catalogue.
5. **Upgrades that can't carry a local edit over.** *Recommendation:* refuse the upgrade with the operation named, and
   let the person choose: keep the old release, or upgrade and drop the edit (a second, explicit revision).
6. **The `exports` section and the `app` variable.** *Recommendation:* as designed: features read the app through
   named, typed exports, never `state` directly, so apps can change their storage and features can move between
   apps.
7. **Ids with a hyphen (`wordCount-label`) while other names use a dot.** *Recommendation:* keep the hyphen: a dot in
   an id clashes with `#id.edge` in anchors.
8. **Where the catalogue's name goes.** The plan has a working name for the catalogue; public files call it "the
   feature catalogue" until the maintainer decides to publish a name.
9. **List functions features will want** (`groupBy`, `flatMap`). *Recommendation:* add them through the expression
   function registry when the first feature needs them (the TODO tree does); they are one table row each.
