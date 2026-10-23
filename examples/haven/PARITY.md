# Haven parity

Haven aims at the look and the flow of a mature open-source home dashboard, built only from a JSON screen and run by
the stock host. All names, data, text and pictures are invented; the icons are Lucide. Each row is one thing a user
of such a dashboard notices; the status is `done`, `partial` or `missing`, and the counts are updated every round.

**Counts:** 45 done, 3 partial, 1 missing (49).

## Shell

| Feature | Status | Notes |
| --- | --- | --- |
| Sidebar with the app title, pages and their icons | done | Home, Energy, Floor plan, Logbook, History, Areas, Automations; Settings and Notifications at the foot |
| Selected page highlighted in the sidebar | done | |
| Views as tabs in the top bar | done | one tab per room, the shown one underlined |
| The user at the foot of the sidebar | done | an initial in a circle; it opens the profile |
| Coloured top bar with the views, search and a menu | done | the menu holds search and Edit dashboard |
| Light and dark themes | done | light by default, as the reference |
| Right to left (Arabic) | done | the sidebar moves to the right and every card mirrors; the floor plan and the automation graph keep their drawing |
| Notifications button with a badge | done | |
| Search dialog over entities | done | E opens it over every light, climate, cover, device and the alarm, dimming the page; Enter opens its dialog |
| Entity icons in the search | partial | names and areas; the kit's quick pick has no icons |
| Collapsible sidebar | done | the menu button folds it to icons with tooltips |

## Dashboard tiles

| Feature | Status | Notes |
| --- | --- | --- |
| Summary chips row (lights on, weather, climate) | done | |
| Area card with a picture and its sensors | done | |
| Light tiles: icon, state, toggle, brightness | done | the icon switches the light; a rounded bar in its colour sets the level |
| Scene buttons with icons | done | |
| Thermostat card with a dial, target and modes | done | minus and plus under the target, in the dial's gap |
| Cover (blinds) card with open, close and position | done | a rounded position bar |
| Device tiles with one action each | done | |
| Alarm panel tile and its keypad dialog | done | a button per mode; disarming takes a code |
| Round toggle switches | done | drawn round switches in the automations, the card editor and the general settings; tiles use bars |
| Tile names never cut at the smallest window | done | |
| Edit mode for the dashboard | done | a blue-grey bar with close, the title and Done; each card outlined, with a pencil in its corner that covers none of its content |
| Card editor: visual and code | done | title and options in a form, or the card as JSON, checked on save, beside a live preview of the card; cards cannot yet be added or moved |
| Snackbar for short messages | done | a dark bar at the bottom centre with Dismiss, or a call's progress |

## More-info dialogs

| Feature | Status | Notes |
| --- | --- | --- |
| Light dialog: big brightness bar, colour temperature | done | opens from a light tile; the bar fills with a flat top and a handle line |
| Climate dialog: dial, modes, history | done | its own dial, modes and a 24-hour line with a scale |
| Cover dialog: position, tilt | partial | position, no tilt |
| Device dialog: attributes, related, logbook | partial | attributes and activity, no related |
| Dialogs dim the page behind them | done | the search too |
| Dialog header: area above the name, history and settings | done | |

## Energy

| Feature | Status | Notes |
| --- | --- | --- |
| Energy page with the week and the hour | done | hourly use stacked by solar and grid; the week by day, device or room |
| Energy distribution (grid, solar, home) | done | three nodes joined by flows, with the day's totals |
| Sources table and costs | done | grid in and out, solar, net cost; self-use gauges and a daily budget bar |

## Automations

| Feature | Status | Notes |
| --- | --- | --- |
| Automations list with on and off | done | a coloured app bar; a searchable table with name, last ran and enabled columns; disabled rules dimmed; a floating New automation button |
| Editor: triggers, conditions, actions as cards | done | a coloured app bar with back, name, enabled, Run and more; When, And if, Then do cards with outlined add buttons; a graph view; an overflow menu with Run, Duplicate and Delete, which asks first |
| Confirm dialogs | done | rounded, with text buttons: Cancel, then Delete in red |
| Automation trace / last run | done | a step-by-step timeline of the last run; Run starts it now |

## Areas, logbook and history

| Feature | Status | Notes |
| --- | --- | --- |
| Areas page with a card per room | done | grouped by floor; a picture, temperature, humidity and lights on; a card opens its room |
| Logbook: a dated list of events | done | today and yesterday, the entity and what it did with the time beneath, rules marked in the accent colour, searchable |
| History: state bars and lines over time | done | a day timeline per entity with labelled states and a temperature line on a scale with gridlines and hours |
| History: period and target pickers | done | a date range field and a Choose entity, area or device button, which opens the search |
| Full-size app bar actions and add buttons | done | 24-pixel icons in every app bar; Add an area floats at the bottom corner |

## Settings

| Feature | Status | Notes |
| --- | --- | --- |
| Settings list grouped in cards | done | coloured icon circles, a subtitle and a chevron per row |
| Profile: user, language, theme | done | the user and Log out; the language in a dropdown; the theme as Auto, Light and Dark radio buttons |
| Devices and entities tables | done | one table of devices with their room and state |
| About page | done | |
| General: climate, scenes, budget, notifications | done | rows from the settings schema in a card capped at 1040 pixels: filled number fields, round switches, the language as choices, Reset when changed |

## Add-ons

| Feature | Status | Notes |
| --- | --- | --- |
| Chat assistant panel | missing | waits for 14-browser M1 |
| Shared kit components | done | the kit menu, dialog and quick pick, themed through `kit.*` colours |
