# Haven, a smart-home dashboard

Rooms in a tree by floor, scenes, lights, climate, blinds, an energy page with charts, a settings page, light and dark themes. The `smart-home` template: a screen document with no C++ of its own, which the stock host runs.

```sh
./build/affrescoui_host examples/haven/smart-home.screen.json
./build/affrescoui test examples/haven/smart-home.scenarios.json
```

| File | What it is |
|---|---|
| `images/` | The pictures the screen shows |
| `smart-home.policy.json` | Its approval rules' example revisions (`affrescoui policy test`) |
| `smart-home.scenarios.json` | Its scenarios (`affrescoui test`; the rendering check plays them too) |
| `smart-home.screen.json` | The screen document |

Its fonts are in [`../shared/fonts`](../shared/fonts/README.md).
