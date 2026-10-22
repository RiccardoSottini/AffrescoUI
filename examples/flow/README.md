# Flow, a Kanban board

Boards as tabs, a table of every card, a card's details beside them, drag and drop, a members popover, Cmd+P and Cmd+Shift+P. The `kanban` template: a screen document with no C++ of its own, which the stock host runs.

```sh
./build/affrescoui_host examples/flow/kanban.screen.json
./build/affrescoui test examples/flow/kanban.scenarios.json
```

| File | What it is |
|---|---|
| `kanban.policy.json` | Its approval rules' example revisions (`affrescoui policy test`) |
| `kanban.scenarios.json` | Its scenarios (`affrescoui test`; the rendering check plays them too) |
| `kanban.screen.json` | The screen document |

`affrescoui_host` opens it when it is given no screen.

Its fonts are in [`../shared/fonts`](../shared/fonts/README.md).
