# Haven parity

Haven aims at the look and the flow of a mature open-source home dashboard, built only from a JSON screen and run by
the stock host. All names, data, text and pictures are invented; the icons are Lucide. Each row is one thing a user
of such a dashboard notices; the status is `done`, `partial` or `missing`, and the counts are updated every round.

**Counts:** 27 done, 4 partial, 8 missing (39).

## Shell

| Feature | Status | Notes |
| --- | --- | --- |
| Sidebar with the app title, pages and their icons | done | Home, Energy, Floor plan, Logbook, History, Automations, Settings; Areas beside the rooms heading |
| Selected page highlighted in the sidebar | done | |
| Rooms tree in the sidebar, floors that fold | done | |
| Coloured top bar with the page title, search and edit | done | edit says it is coming |
| Light and dark themes | done | light by default, as the reference |
| Right to left (Arabic) | partial | the rooms tree keeps its left-to-right indent |
| Notifications button with a badge | done | |
| Search dialog over entities | missing | |
| Collapsible sidebar | missing | |

## Dashboard tiles

| Feature | Status | Notes |
| --- | --- | --- |
| Summary chips row (lights on, weather, climate) | done | |
| Area card with a picture and its sensors | done | |
| Light tiles: icon, state, toggle, brightness | done | |
| Scene buttons with icons | done | |
| Thermostat card with a dial, target and modes | done | |
| Cover (blinds) card with open, close and position | done | |
| Device tiles with one action each | done | |
| Round toggle switches | partial | the library draws a square switch |
| Tile names never cut at the smallest window | done | |
| Edit mode for the dashboard | missing | |

## More-info dialogs

| Feature | Status | Notes |
| --- | --- | --- |
| Light dialog: big brightness bar, colour temperature | done | opens from a light tile |
| Climate dialog: dial, modes, history | done | its own dial, modes and a 24-hour line with a scale |
| Cover dialog: position, tilt | partial | position, no tilt |
| Device dialog: attributes, related, logbook | partial | attributes and activity, no related |
| Dialogs dim the page behind them | done | |

## Energy

| Feature | Status | Notes |
| --- | --- | --- |
| Energy page with the week and the hour | done | hourly use stacked by solar and grid; the week by day, device or room |
| Energy distribution (grid, solar, home) | done | three nodes joined by flows, with the day's totals |
| Sources table and costs | done | grid in and out, solar, net cost; self-use gauges and a daily budget bar |

## Automations

| Feature | Status | Notes |
| --- | --- | --- |
| Automations list with on and off | done | searchable table with name, last ran and enabled columns; disabled rules dimmed; a floating New automation button |
| Editor: triggers, conditions, actions as cards | done | an editor bar with back, name, enabled and Run; When, And if, Then do cards with outlined add buttons; a graph view; an overflow menu with Run, Duplicate and Delete, which asks first |
| Automation trace / last run | done | a step-by-step timeline of the last run; Run starts it now |

## Areas, logbook and history

| Feature | Status | Notes |
| --- | --- | --- |
| Areas page with a card per room | done | grouped by floor; a picture, temperature, humidity and lights on; a card opens its room |
| Logbook: a dated list of events | done | today and yesterday, rules marked in the accent colour, searchable |
| History: state bars and lines over time | done | a day timeline per entity with labelled states and a temperature line |

## Settings

| Feature | Status | Notes |
| --- | --- | --- |
| Settings list grouped in cards | missing | |
| Profile: language, theme | missing | |
| Devices and entities tables | missing | |
| About page | missing | |

## Add-ons

| Feature | Status | Notes |
| --- | --- | --- |
| Chat assistant panel | missing | waits for 14-browser M1 |
| Shared kit components | done | the kit menu, dialog and quick pick, themed through `kit.*` colours |
