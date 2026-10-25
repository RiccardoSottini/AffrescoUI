# Quaderno, a code editor

A desktop text editor: a side bar with open files and folders, tabs, a minimap, the Quick Switcher (files, symbols, lines, also scoped to a file), Find Symbol in Project, Find in Files with a results buffer, a command palette, a console and a status bar; context menus on the tabs and the text, a theme picker (dark, light, the system's), Distraction Free, and its settings and key bindings as JSON files. The `code-editor` template: a screen document that the stock host runs. What it covers is in [`PARITY.md`](PARITY.md).

```sh
./build/affrescoui_launcher examples/code-editor/index.screen.json
./build/affrescoui test examples/code-editor/index.scenarios.json
```

| File | What it is |
|---|---|
| `PARITY.md` | The parity matrix: the reference's workflows, done, partial or missing |
| `index.policy.json` | Its approval rules' example revisions (`affrescoui policy test`) |
| `index.scenarios.json` | Its scenarios (`affrescoui test`; ctest `scenarios` plays them with the rendering check's runner) |
| `index.screen.json` | The screen document |
| `parts/look.json` | Its look: the colours in dark and light, the syntax colours, and the kit's tokens mapped to them |

Its editor is the library's `editor`, with the languages in [`../shared/languages`](../shared/languages).

Its fonts are in [`../shared/fonts`](../shared/fonts/README.md).

## How it holds together

- **Permissions:** `studio` peers (the person's client) change it directly; an agent's change waits for the person's review (on its card in the assistant chat, or from a `studio` peer) (a change to its actions always does, even under a grant), and a budget caps how many nodes an agent changes at once (20 added, 5 removed, 50 changed); no one may change the status bar. Nothing persists.
- **Threading and ownership:** it has no C++ of its own. The stock host owns the window and the screen, and runs the document on its UI thread.
- **Extension points:** files are entries of `state`; menus and commands are `menus` and `actions`.
- **Assets:** no pictures of its own; the fonts' licences are with them in `../shared/fonts`.
