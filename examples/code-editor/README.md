# Bottega, a code editor

A menu bar, an explorer tree, editor tabs, quick open, a command palette, problems and a terminal. The `code-editor` template: a screen document that the stock host runs.

```sh
./build/affrescoui_host examples/code-editor/code-editor.screen.json
./build/affrescoui test examples/code-editor/code-editor.scenarios.json
```

| File | What it is |
|---|---|
| `code-editor.policy.json` | Its approval rules' example revisions (`affrescoui policy test`) |
| `code-editor.scenarios.json` | Its scenarios (`affrescoui test`; the rendering check plays them too) |
| `code-editor.screen.json` | The screen document |

Its `code` type is the host's, in [`../shared/CodeEditor.cpp`](../shared/CodeEditor.cpp).

Its fonts are in [`../shared/fonts`](../shared/fonts/README.md).
