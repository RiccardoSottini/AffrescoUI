# Streak, a habit tracker

Habits to tick, a heatmap, charts, a day-by-day history and notes, kept on disk with `persist`. The `habit-tracker` template: a screen document with no C++ of its own, which the stock host runs.

```sh
./build/affrescoui_host examples/streak/habit-tracker.screen.json
./build/affrescoui test examples/streak/habit-tracker.scenarios.json
```

| File | What it is |
|---|---|
| `habit-tracker.policy.json` | Its approval rules' example revisions (`affrescoui policy test`) |
| `habit-tracker.scenarios.json` | Its scenarios (`affrescoui test`; the rendering check plays them too) |
| `habit-tracker.screen.json` | The screen document |

Its fonts are in [`../shared/fonts`](../shared/fonts/README.md).
