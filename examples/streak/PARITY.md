# Streak: parity

Streak is a habit tracker laid out for the desktop: the habit
list and the selected habit's detail side by side, one pane at a time below 960 window units. It replicates the
design and the workflows, not the product: its own name, Lucide icons, invented habits and text, no assets of the
original.

Each area is `done`, `partial` or `missing`, with the gap in one line. Counts at the end.

## Main list

| Area | Status | Gap |
|---|---|---|
| A row per habit: colour, score ring, name | done | |
| A header of the last days (weekday, day of the month), as many as fit | done | |
| Click a box to check it: done, skipped (if allowed), cleared | done | |
| Done in the habit's colour, a faded tick for "on target" by frequency, × for missed | done | |
| Measurable boxes: the number and its unit, faded below the target | done | |
| A long-press equivalent: a right click or Shift+F10 on a box, Shift+Space on a row (or a click on a measurable box) opens the day's popup (value, note, yes/skip/no) | done | |
| A dot on days with a note | done | |
| Sort: manually, by name, colour, score, status | done | |
| Hide completed, show archived | done | |
| Drag to reorder | done | |
| An empty list's message | done | |
| Selecting several habits: a right click (or Shift+F10) on a row picks it, clicks add more; a bar archives or deletes them, Escape leaves | done | the bar has no colour change for the picked habits |

## Habit types and frequencies

| Area | Status | Gap |
|---|---|---|
| Yes or no habits | done | |
| Measurable habits: unit, target, at least or at most | done | |
| Daily, every N days, N per week, N per month | done | |
| Reminders: a time and the days | done | shown as a toast while the app is open, not a system notification |

## Detail

| Area | Status | Gap |
|---|---|---|
| The detail bar with a stripe and title in the habit's colour, back, edit, more | done | |
| Question, frequency and reminder line | done | |
| At a glance: strength ring, strength, month and year change, total | done | |
| Target card for measurable habits (today, week, month, quarter, year) | done | |
| Score chart with a period (day to year) | done | |
| History bars with a period | done | |
| Calendar heatmap, scrolled to the latest weeks | done | |
| Longest runs as bars | done | |
| Frequency: dots per weekday and month | done | |
| Notes of past days | done | |

## Edit dialog

| Area | Status | Gap |
|---|---|---|
| Name, question, notes | done | |
| A 20-colour picker | done | |
| Frequency and target | done | |
| Reminder with a time and weekday chips | done | the time is a text field, not a clock picker |
| Archive, delete (asks first), export CSV | done | CSV goes to the clipboard: a document can't write files |
| A name is required | done | |
| Cmd+A in a field | partial | the library's text field has no select-all or caret movement yet |

## Settings

| Area | Status | Gap |
|---|---|---|
| Theme: light, dark, true black | done | |
| First day of the week, reverse order of days, skip days | done | |
| Notifications on or off | done | |
| Language (English, Español, Français) | done | |
| Export all as CSV | done | |
| Sync (optional, a placeholder sign-in) | done | |
| A full backup to a JSON file, and restoring one (adds or replaces habits, skips damaged entries) | done | |
| Widgets | missing | a desktop document has no home-screen widgets |

## Desktop extras

| Area | Status | Gap |
|---|---|---|
| Keyboard: Cmd+N, Cmd+Shift+N, Cmd+E, Cmd+Backspace, Cmd+Shift+H, Cmd+comma, Cmd+J, Cmd+K, Cmd+arrows, Cmd+Enter | done | |
| Accessible names on every box and control | done | |
| A narrow window shows one pane | done | |
| The assistant chat add-on (Cmd+Shift+A), in the app's colours | done | |

## Counts

- done: 43
- partial: 1
- missing: 1
