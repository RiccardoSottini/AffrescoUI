# Tempo, a music player

A desktop streaming app's layout in one JSON screen: a resizable library, gradient pages with sticky table headers, a
right panel with the song playing or the queue, and a full player bar. Playback is simulated: a timer, no audio. All
the music, artists, lyrics and pictures are invented. The `music-player` template: no C++ of its own.

```sh
./build/affrescoui_host examples/tempo/music-player.screen.json
./build/affrescoui test examples/tempo/music-player.scenarios.json
```

| File | What it is |
|---|---|
| `images/` | The pictures the screen shows |
| `catalog.json` | The invented catalogue: artists, albums, songs, playlists, shows, genres and lyrics, as `state` |
| `panels.json` | The window's parts: the top bar, Your Library, the right panel, the player bar, full screen and the mini player |
| `pages.json` | The pages: home, playlist and album, artist, search and genres, shows, lyrics |
| `strings.json` | The string tables: English, Italian and Arabic |
| `music-player.policy.json` | Its approval rules' example revisions (`affrescoui policy test`) |
| `music-player.scenarios.json` | Its scenarios (`affrescoui test`; ctest `scenarios` plays them with the rendering check's runner) |
| `music-player.screen.json` | The screen document: fonts, colours, state, actions and shortcuts |
| `PARITY.md` | What the layout covers, feature by feature |

Its fonts are in [`../shared/fonts`](../shared/fonts/README.md).

## What it shows

- **Screen documents in parts:** `include` of the catalogue, the panels, the pages and the string tables; parts as
  components with `use`.
- **Your Library:** chips by kind, search and sort, list, compact and grid views, pins, a context menu; it widens,
  narrows and collapses.
- **Pages:** a gradient header from the cover's colour, a big play button, a table with sortable columns, hover play,
  the row playing in the accent colour with an equaliser, multi-select (Cmd/Ctrl, Shift), drag to reorder, a context
  menu with submenus, a sticky header and a sticky table head; artists with a banner, popular songs, discography chips
  and fans also like; home with a greeting, shortcuts and shelves; search with genre tiles, a top result and songs.
- **The player:** like, shuffle, repeat (off, all, one), a seek bar with a hover thumb, lyrics, the queue, a device
  button, volume, mini player and full screen with a visualiser drawn as a `path`.
- **Commands:** titled actions in the command palette (Cmd+Shift+P) and Quick Open of songs (Cmd+P); shortcuts for
  playback (Space, Cmd+arrows), pages (Alt+Left and Right, Alt+Shift+H, Cmd+K), the panels (Alt+Shift+Q, R, Y), the
  mini player (Cmd+M), full screen (Cmd+Shift+F), the theme (Cmd+T), the language (Cmd+Shift+L), and Cmd+/ for the list.
- **Themes and locales:** light and dark tokens; English, Italian and Arabic string tables, Arabic right to left.
- **Accessibility:** roles and names, live regions for the lyric playing, menus and tables with the keyboard.

## How it holds together

- **Permissions:** Studio peers change it directly; an agent's change waits for a Studio reviewer (a change to its
  actions always does, even under a grant), and a budget caps how many nodes an agent changes at once (20 added, 5
  removed, 50 changed); no one may change the player bar. The theme, the library's layout, the volume and what was
  liked or saved persist.
- **Threading and ownership:** it has no C++ of its own. The stock host owns the window and the screen, and runs the
  document on its UI thread.
- **Extension points:** the catalogue is `state` in `catalog.json`; a page is a component of `pages.json`; a language
  is a table in `strings.json`.
- **Assets:** the pictures in `images/` are simple vector drawings made for this project, under the repository's
  Apache-2.0 licence. The icons are Lucide's and the fonts' licences are with them in `../shared/fonts`.
