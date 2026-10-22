# Quaderno, a code editor

A menu bar, an explorer tree, editor tabs, quick open, a command palette, problems and a terminal. The `code-editor` template: a screen document that the stock host runs.

```sh
./build/affrescoui_host examples/code-editor/code-editor.screen.json
./build/affrescoui test examples/code-editor/code-editor.scenarios.json
```

| File | What it is |
|---|---|
| `code-editor.policy.json` | Its approval rules' example revisions (`affrescoui policy test`) |
| `code-editor.scenarios.json` | Its scenarios (`affrescoui test`; ctest `scenarios` plays them with the rendering check's runner) |
| `code-editor.screen.json` | The screen document |

Its `code` type is the host's, in [`../shared/CodeEditor.cpp`](../shared/CodeEditor.cpp).

Its fonts are in [`../shared/fonts`](../shared/fonts/README.md).

## How it holds together

- **Permissions:** Studio peers change it directly; an agent's change waits for a Studio reviewer (a change to its actions always does, even under a grant), and a budget caps how many nodes an agent changes at once (20 added, 5 removed, 50 changed); no one may change the status bar. Nothing persists.
- **Threading and ownership:** it has no C++ of its own but the host's `code` type. The stock host owns the window and the screen, and runs the document on its UI thread.
- **Extension points:** files are entries of `state`; menus and commands are `menus` and `actions`.
- **Assets:** no pictures of its own; the fonts' licences are with them in `../shared/fonts`.
