# Haven parity

Haven aims at the look and the flow of a mature open-source home dashboard, built only from a JSON screen and run by
the stock host. All names, data, text and pictures are invented; the icons are Lucide. Each row is one thing a user
of such a dashboard notices; the status is `done`, `partial` or `missing`, and the counts are updated every round.

**Counts:** 50 done, 0 partial, 0 missing (50).

## Shell

| Feature | Status | Notes |
| --- | --- | --- |
| Sidebar with the app title, pages and their icons | done | Home, Energy, Floor plan, Activity log, History, Rooms, Automations; Settings and Notifications at the foot |
| Selected page highlighted in the sidebar | done | the user at the foot while the profile is open |
| Views as tabs in the top bar | done | one tab per room, the shown one underlined |
| The user at the foot of the sidebar | done | an initial in a circle; it opens the profile |
| Coloured top bar with the views, search and a menu | done | the menu holds search and Arrange cards |
| Light and dark themes | done | light by default, as the reference |
| Notifications button with a badge | done | |
| Search dialog over controls | done | E or the magnifier opens it, a rounded dialog over every light, climate, cover, device and the alarm, dimming the page: a filled field with a magnifier and a clear button; Enter opens the picked one's dialog |
| Entity icons in the search | done | each result's icon in a circle, its name and its area beneath; Haven's own component replaces the kit's quick pick, with the same search and keys |
| Collapsible sidebar | done | the menu button folds it to icons with tooltips |

## Dashboard tiles

| Feature | Status | Notes |
| --- | --- | --- |
| Summary chips row (lights on, weather, climate) | done | |
| Area card with a picture and its sensors | done | |
| Light tiles: icon, state, toggle, brightness | done | the icon switches the light; a rounded bar in its colour sets the level, empty while the light is off |
| Scene buttons with icons | done | |
| Thermostat card with a dial, target and modes | done | minus and plus under the target, in the dial's gap |
| Cover (blinds) card with open, close and position | done | a rounded position bar |
| Device tiles with one action each | done | |
| Alarm panel tile and its keypad dialog | done | a button per mode; disarming takes a code |
| Round toggle switches | done | drawn round switches in the automations, the card editor and the general settings; tiles use bars |
| Tile names never cut at the smallest window | done | |
| Edit mode for the dashboard | done | a slate-green bar with close, the title and Done; each section outlined with even padding, with a pencil in its corner that covers none of its content; Add card in the edit bar |
| Card editor: visual and code | done | a filled title field with its label inside and the options in a form, or the card as JSON, checked on save, beside a live preview of the card as the dashboard draws it (the thermostat with the current temperature, its dial, minus and plus clear below it, and modes); cards cannot yet be moved |
| Snackbar for short messages | done | a dark bar at the bottom centre with Dismiss, or a call's progress |

## More-info dialogs

| Feature | Status | Notes |
| --- | --- | --- |
| Light dialog: big brightness bar, colour temperature | done | opens from a light tile; the bar fills with a flat top and a handle line |
| Climate dialog: dial, modes, history | done | its own dial with minus and plus in its gap, a Mode dropdown, and a 24-hour line with a scale |
| Cover dialog: position, tilt | done | a position bar and a narrower tilt bar, open, stop and close |
| Device dialog: properties, related, activity | done | the speaker with previous, play or pause, next and a volume bar; battery, last change, the automation it is used in (which opens it), recent activity |
| Dialogs dim the page behind them | done | the search too |
| Dialog header: area above the name, history, settings and more | done | the more menu opens the related automation or the properties |

## Energy

| Feature | Status | Notes |
| --- | --- | --- |
| Energy page with the week and the hour | done | hourly use stacked by solar and grid on a kWh axis with gridlines; the week by day, device or room |
| Where the energy goes (grid, solar, home) | done | three nodes joined by flows, with the day's totals |
| Sources table and costs | done | grid in and out, solar, net cost; self-use gauges and a daily budget bar |

## Automations

| Feature | Status | Notes |
| --- | --- | --- |
| Automations list with on and off | done | a coloured app bar; a searchable table with name and last ran columns, without a count that could be cut; disabled rules dimmed; a menu per row to run, duplicate, disable or delete it; a floating New automation button |
| Editor: triggers, conditions, actions as cards | done | a coloured app bar with back, the name and more; a banner with Enable while it is disabled; When, And if, Then do cards, each step an outlined row with a chevron and a menu to duplicate or delete it, and outlined add buttons; a graph view; an overflow menu with Run, Disable, Duplicate and Delete, which asks first |
| Confirm dialogs | done | rounded, with text buttons: Cancel, then Delete in red |
| Automation trace / last run | done | a step-by-step timeline of the last run; Run starts it now |

## Rooms, activity log and history

| Feature | Status | Notes |
| --- | --- | --- |
| Areas page with a card per room | done | grouped by floor; a picture, temperature, humidity and lights on; a card opens its room |
| Activity log: a dated list of events | done | today and yesterday, the entity and what it did with the time beneath, rules marked in the accent colour; a date range field and a Choose a control, room or device field that filters it |
| History: state bars and lines over time | done | a continuous day timeline per entity with labelled states and a temperature line on a scale with gridlines and hours |
| History: period and target pickers | done | a date range field and a Choose entity, area or device button, which opens the search |
| Full-size app bar actions and add buttons | done | 24-pixel icons in every app bar; Add an area floats at the bottom corner |

## Settings

| Feature | Status | Notes |
| --- | --- | --- |
| Settings list grouped in cards | done | coloured icon circles, a subtitle and a chevron per row |
| Profile: user, language, theme | done | the user and Log out; the language in the same filled dropdown as the general settings; the theme as Auto, Light and Dark radio buttons |
| Devices and controls tables | done | Connections, Devices, Controls and Utilities tabs in the app bar (under it in a narrow window), Filters then a Search 3 devices field that narrows the table, a full-width table with lines between rows and the manufacturer, model, area and integration, and a floating add button |
| About page | done | the logo, version, devices and licence, then links to the documentation, issues and source |
| General: climate, scenes, budget, notifications | done | rows from the settings schema in a card capped at 1040 pixels: filled number fields, round switches, the language in a dropdown, Reset when changed |

## Add-ons

| Feature | Status | Notes |
| --- | --- | --- |
| Chat assistant panel | done | the shared assistant add-on in Haven's colours, opened from a chat button in the top bar; requests come back as proposals to accept or reject |
| Overflow menus | done | raised and rounded, with a shadow, an icon and a title per item, on the app bars, the dialogs, each automation and each step |
| Shared kit components | done | the kit menu and dropdowns, themed through `kit.*` colours; the search is Haven's own |
