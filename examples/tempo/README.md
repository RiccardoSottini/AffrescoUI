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
| `strings.json` | The string tables: English, Italian and Arabic |
| `Visualiser.hpp`, `Visualiser.cpp` | The `visualiser` type: bars drawn on a GPU surface, registered by the host |
| `music-player.policy.json` | Its approval rules' example revisions (`affrescoui policy test`) |
| `music-player.scenarios.json` | Its scenarios (`affrescoui test`; the rendering check plays them too) |
| `music-player.screen.json` | The screen document |

Its fonts are in [`../shared/fonts`](../shared/fonts/README.md).

## What it shows

- **Screen documents in parts:** `include` of the pages and the string tables; pages as components with `use`.
- **Widgets:** virtualised tables (the library of 3,000 songs, a collection, search results), grids of albums and artists, a list for the queue (dragged, or moved with Alt+Up and Down), the `curves` control for a ten-band equaliser with presets, sliders, a waveform, a tab list for the menu.
- **An application type:** `visualiser`, a GPU surface in C++ (`Visualiser.cpp`) that the host and the `affrescoui` tool register.
- **A simulated player:** an `every` timer moves the song along, crossfades into the next one and steps the lyrics.
- **Commands:** titled actions in the command palette (Cmd+Shift+P) and Quick Open of songs (Cmd+P); shortcuts for playback (Space, arrows), pages (Cmd+L, Cmd+2, Cmd+3, Cmd+F, Cmd+5, Cmd+6), the queue (Cmd+U), the mini player (Cmd+M), the theme (Cmd+T), the language (Cmd+Shift+L).
- **Settings** that persist (`settings` with a schema): language and crossfade; the theme, the equaliser and the volume persist too.
- **Themes and locales:** light and dark tokens; English, Italian and Arabic string tables, Arabic right to left.
- **Accessibility:** roles and names, live regions, the menu as a tab list with arrow keys, sliders and lists with the keyboard.
- **Layouts** from 1040 × 640 to full screen, and a mini player.
