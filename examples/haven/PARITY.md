# Haven parity

Haven aims at the look and the flow of a mature open-source home dashboard, built only from a JSON screen and run by
the stock host. All names, data, text and pictures are invented; the icons are Lucide. Each row is one thing a user
of such a dashboard notices; the status is `done`, `partial` or `missing`, and the counts are updated every round.

**Counts:** 13 done, 4 partial, 21 missing (38).

## Shell

| Feature | Status | Notes |
| --- | --- | --- |
| Sidebar with the app title, pages and their icons | done | Home, Energy, Floor plan, Automations, Settings |
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
| Tile names never cut at the smallest window | partial | two names are clipped at 1040 × 640 |
| Edit mode for the dashboard | missing | |

## More-info dialogs

| Feature | Status | Notes |
| --- | --- | --- |
| Light dialog: big brightness bar, colour temperature | missing | |
| Climate dialog: dial, modes, history | missing | |
| Cover dialog: position, tilt | missing | |
| Device dialog: attributes, related, logbook | missing | |

## Energy

| Feature | Status | Notes |
| --- | --- | --- |
| Energy page with the week and the hour | partial | the layout predates this work |
| Energy distribution (grid, solar, home) | missing | |
| Sources table and costs | missing | |

## Automations

| Feature | Status | Notes |
| --- | --- | --- |
| Automations list with on and off | missing | |
| Editor: triggers, conditions, actions as cards | missing | |
| Automation trace / last run | missing | |

## Areas, logbook and history

| Feature | Status | Notes |
| --- | --- | --- |
| Areas page with a card per room | missing | |
| Logbook: a dated list of events | missing | |
| History: state bars and lines over time | missing | |

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
| Shared kit components | missing | waits for 14-kit M1 |
