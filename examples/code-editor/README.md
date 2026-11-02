# Quaderno, a code editor

A desktop text editor: a side bar with open files and folders, tabs, a minimap, the Quick Switcher (files, symbols, lines, also scoped to a file), Find Symbol in Project, Find in Files with a results buffer, a command palette, a console and a status bar; context menus on the tabs and the text, a theme picker (dark, light, the system's), Distraction Free, and its settings and key bindings as JSON files. The `code-editor` template: an app with no C++ of its own, which the stock host runs. What it covers is in [`PARITY.md`](PARITY.md).

**Its `lisp/` is a real Lisp program** (RFC 0005; SCREEN_FORMAT.md, "The Lisp language"), because it has behaviour: tabs that open, close
and ask before losing a change, a console, Settings that apply, and some thirty editing commands that differ only by name.
`json/` beside it is the same app written in JSON, also by hand: the Launcher's Settings choose which one opens, the same scenarios check both, and `tools/notation/lisp_examples.py` leaves both alone.

```sh
./build/affrescoui_launcher examples/code-editor/lisp/index.screen.lisp
./build/affrescoui test examples/code-editor/lisp/index.scenarios.json
```

| File | What it is |
|---|---|
| `PARITY.md` | The parity matrix: the reference's workflows, done, partial or missing |
| `lisp/app.lisp` | The app: its entry and its capabilities |
| `lisp/index.screen.lisp` | The screen document: the view, the state, the menus, the one-step actions and the text scans (problems, symbols, Find in Files), which run as expressions |
| `lisp/quaderno.lisp` | The program: files and tabs, the console, the search results and Settings as functions, and the commands; `defedit` makes each of the editor's own edits a command |
| `lisp/parts/look.lisp` | Its look: the colours in dark and light, the syntax colours, and the kit's tokens mapped to them |
| `lisp/index.policy.lisp` | Its approval rules' example revisions (`affrescoui policy test`) |
| `lisp/index.scenarios.json` | Its scenarios, which stay JSON (`affrescoui test`; ctest `scenarios` plays them with the rendering check's runner) |

Its editor is the library's `editor`, with the languages in [`../shared/languages`](../shared/languages).

Its fonts are in [`../shared/fonts`](../shared/fonts/README.md).

## How it holds together

- **Permissions:** `studio` peers (the person's client) change it directly; an agent's change waits for the person's review (on its card in the assistant chat, or from a `studio` peer) (a change to its program or its actions always does, even under a grant), and a budget caps how many nodes an agent changes at once (20 added, 5 removed, 50 changed); no one may change the status bar. Nothing persists.
- **Threading and ownership:** it has no C++ of its own. The stock host owns the window and the screen, and runs the document and its program on its UI thread.
- **Extension points:** files are entries of `state`; menus and commands are `menus` and `actions`.
- **Assets:** no pictures of its own; the fonts' licences are with them in `../shared/fonts`.
