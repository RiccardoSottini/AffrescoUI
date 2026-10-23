# Tempo, a music player

Playlists, a library of 3,000 songs in a table, albums and artists in grids, search, a queue you reorder, now playing with a visualiser and lyrics, an equaliser, crossfade and a mini player. Playback is simulated: a timer, no audio. The `music-player` template: a screen document whose one piece of C++, the visualiser surface, the stock host registers.

```sh
./build/affrescoui_host examples/tempo/music-player.screen.json
./build/affrescoui test examples/tempo/music-player.scenarios.json
```

| File | What it is |
|---|---|
| `images/` | The pictures the screen shows |
| `pages.json` | The pages (albums, artists, a collection, search, now playing, sound), included by the screen |
| `strings.json` | The string tables: English, Spanish and French |
| `Visualiser.hpp`, `Visualiser.cpp` | The `visualiser` type: bars drawn on a GPU surface, registered by the host |
| `music-player.policy.json` | Its approval rules' example revisions (`affrescoui policy test`) |
| `music-player.scenarios.json` | Its scenarios (`affrescoui test`; ctest `scenarios` plays them with the rendering check's runner) |
| `music-player.screen.json` | The screen document |

Its fonts are in [`../shared/fonts`](../shared/fonts/README.md).

## What it shows

- **Screen documents in parts:** `include` of the pages and the string tables; pages as components with `use`.
- **Widgets:** virtualised tables (the library of 3,000 songs, a collection, search results), grids of albums and artists, a list for the queue (dragged, or moved with Alt+Up and Down), the `curves` control for a ten-band equaliser with presets, sliders, a waveform, a tab list for the menu.
- **An application type:** `visualiser`, a GPU surface in C++ (`Visualiser.cpp`) that the host and the `affrescoui` tool register.
- **A simulated player:** an `every` timer moves the song along, crossfades into the next one and steps the lyrics.
- **Commands:** titled actions in the command palette (Cmd+Shift+P) and Quick Open of songs (Cmd+P); shortcuts for playback (Space, arrows), pages (Cmd+L, Cmd+2, Cmd+3, Cmd+F, Cmd+5, Cmd+6), the queue (Cmd+U), the mini player (Cmd+M), the theme (Cmd+T), the language (Cmd+Shift+L).
- **Settings** that persist (`settings` with a schema): language and crossfade; the theme, the equaliser and the volume persist too.
- **Themes and locales:** light and dark tokens; English, Spanish and French string tables.
- **Accessibility:** roles and names, live regions, the menu as a tab list with arrow keys, sliders and lists with the keyboard.
- **Layouts** from 1040 × 640 to full screen, and a mini player.

## How it holds together

- **Permissions:** Studio peers change it directly; an agent's change waits for a Studio reviewer (a change to its actions always does, even under a grant), and a budget caps how many nodes an agent changes at once (20 added, 5 removed, 50 changed); no one may change the status bar. The theme, settings, equaliser and volume persist.
- **Threading and ownership:** it has no C++ of its own but the visualiser, a GPU surface that the host registers. The stock host owns the window and the screen, and runs the document on its UI thread.
- **Extension points:** the library, the playlists and the queue are entries of `state`; a page is a part of `pages.json`; a language is a table in `strings.json`.
- **Assets:** the pictures in `images/` are simple vector drawings made for this project, under the repository's Apache-2.0 licence. The fonts' licences are with them in `../shared/fonts`.
