# Streak, a habit tracker

Habits to check day by day, with scores, charts, a calendar and notes, kept on disk with `persist`. The `habit-tracker` template: a screen document with no C++ of its own, which the stock host runs.

```sh
./build/affrescoui_host examples/streak/index.screen.json
./build/affrescoui test examples/streak/index.scenarios.json
```

| File | What it is |
|---|---|
| `PARITY.md` | What it covers of the app it is modelled on, area by area |
| `index.policy.json` | Its approval rules' example revisions (`affrescoui policy test`) |
| `index.scenarios.json` | Its scenarios (`affrescoui test`; ctest `scenarios` plays them with the rendering check's runner) |
| `index.screen.json` | The screen document |

Its fonts are in [`../shared/fonts`](../shared/fonts/README.md).

## What it shows

Laid out for the desktop: the list and the selected habit's detail side by side
([`PARITY.md`](PARITY.md) lists every area and its gap).

- **The list**: a row per habit with its colour, score ring and name under a header of the last days; a click on a
  box checks it (done, skipped, cleared), a measurable box asks for the number, Shift+F10 opens the day's popup with a
  note. Sort by hand (drag to reorder), name, colour, score or status; hide completed, show archived.
- **Habits**: yes or no, or measurable with a unit and a target at least or at most; daily, every N days, N a week or
  N a month; a reminder at a time on chosen days, shown as a `toast` by an `every` timer.
- **The detail**: at a glance, target, a strength chart and progress bars with a period, a calendar heatmap, longest runs,
  frequency dots by weekday and notes, all `computed` from the habit's entries.
- **The edit dialog**: the built-in `dialog` with name, question, a 20-colour picker in a `popover`, frequency,
  reminder and notes; archive (with undo), delete (asks first) and export as CSV to the clipboard.
- **Settings**: light, dark and true black, the first day of the week, reverse order, skip days, notifications, the
  language and sync.
- **Commands and keys**: a command palette (`mod+k`, `quickPick`), `mod+n` and `mod+shift+n` new habits, `mod+e`
  edit, `mod+shift+h` archive, `mod+backspace` delete, `mod+comma` settings, `mod+j` theme, `mod+up`/`mod+down`
  between habits, `mod+enter` checks today, `mod+s` sync.
- **Themes and languages**: English, Español and Français through `strings` and `t()`, the body set to `dir: auto`.
- **Layout**: one pane at a time below 960 window units, the list and the detail side by side above.
- **Scenarios**: 27, one per workflow above.

## How it holds together

- **Permissions:** `studio` peers (the person's client) change it directly; an agent's change waits for the person's review (on its card in the assistant chat, or from a `studio` peer) (a change to its actions always does, even under a grant), and a budget caps how many nodes an agent changes at once (20 added, 5 removed, 50 changed); no one may change the app bar. The habits, the list's order and filters, and the settings persist. The `streakServer` connection syncs the habits with a document server at a local address, signed in with a placeholder provider; its policy shows that no one may move where the habits sync.
- **Threading and ownership:** it has no C++ of its own. The stock host owns the window and the screen, and runs the document on its UI thread.
- **Extension points:** a habit is an entry of `state.habits` (its days in one `entries` string); a language is a table of `strings`; `ring`, `switch`, `periodButton`, `prefRow`, `toggleRow` and `stat` are its local components.
- **Assets:** no pictures of its own; the fonts' licences are with them in `../shared/fonts`.
