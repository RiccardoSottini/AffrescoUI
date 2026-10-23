# Streak, a habit tracker

Habits to tick, a heatmap, charts, a day-by-day history and notes, kept on disk with `persist`. The `habit-tracker` template: a screen document with no C++ of its own, which the stock host runs.

```sh
./build/affrescoui_host examples/streak/habit-tracker.screen.json
./build/affrescoui test examples/streak/habit-tracker.scenarios.json
```

| File | What it is |
|---|---|
| `habit-tracker.policy.json` | Its approval rules' example revisions (`affrescoui policy test`) |
| `habit-tracker.scenarios.json` | Its scenarios (`affrescoui test`; ctest `scenarios` plays them with the rendering check's runner) |
| `habit-tracker.screen.json` | The screen document |

Its fonts are in [`../shared/fonts`](../shared/fonts/README.md).

## What it shows

- **Schedules**: every habit is daily, on chosen weekdays, or n times a week; "Due today" and the streaks follow the
  schedule, and the habit dialog edits it (weekday chips, a times-a-week stepper).
- **Goals and categories**: a weekly or monthly goal per habit with its progress, a category per habit (a `comboBox`
  that takes new ones) and a chip row that filters the list. Category and habit names are data, not translated.
- **Reminders**: a time per habit, checked by an `every` timer, shown as a `toast`; one switch turns them all off.
- **Notes**: a note per day and habit (`textarea` with `maxLength`, `mod+d` for today's), listed on their own page.
- **Pages**: Today, History (a `table` of the last weeks), Notes and Stats (a 53 x 7 year heatmap, streak and
  completion charts, goals and a weekday chart) in `tabs`.
- **Sync**: a `connections` entry to the document server with device sign-in at a placeholder provider
  (`id.example.com`); the policy example shows that no one, studio or agent, may move it.
- **Commands and keys**: a command palette (`mod+k`, `quickPick`), `mod+1`..`mod+4` for the pages, `mod+shift+n`
  new habit, `mod+j` theme, `mod+l` language, `mod+comma` settings, `mod+s` sync; the habit list is one tab stop
  driven by the arrows, Space ticks, E edits, Escape closes the dialog and returns to the row.
- **Themes and languages**: light and dark; English, Italiano and العربية through `strings` and `t()` with plural
  and select forms and `formatDate`, the body set to `dir: auto` so Arabic lays out right to left.
- **Layout**: a narrow single column below 1000 pixels and two columns above, from 480 x 640 to full screen.
- **Expressions**: `computed` values, `watch` (one debounced, sending changes to the server) and `persist` of
  habits, notes, settings and reminders.
- **Scenarios**: 37, covering every page, the dialog by mouse and by keyboard only, both themes, the three
  languages, the palette and shortcuts, resizes and a regression for each bug fixed.

## How it holds together

- **Permissions:** Studio peers change it directly; an agent's change waits for a Studio reviewer (a change to its actions always does, even under a grant), and a budget caps how many nodes an agent changes at once (20 added, 5 removed, 50 changed); no one may change the status bar. The habits, weeks, notes, categories, theme, language and reminders persist. The `streakServer` connection syncs the habits with a document server at a local address, signed in with a placeholder provider; its policy shows that no one may move where the habits sync.
- **Threading and ownership:** it has no C++ of its own. The stock host owns the window and the screen, and runs the document on its UI thread.
- **Extension points:** a habit is an entry of `state.habits`; a category of `state.categories`; a language is a table of `strings`; `chip` is its local component.
- **Assets:** no pictures of its own; the fonts' licences are with them in `../shared/fonts`.
