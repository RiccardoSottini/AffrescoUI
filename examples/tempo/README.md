# Tempo, a music player

Playlists, a library of 3,000 songs in a table, albums and artists in grids, search, a queue you reorder, now playing with a visualiser and lyrics, an equaliser, crossfade and a mini player. The songs are simulated, a timer moving each along; what you hear is a beatbox loop made for Tempo (`audio/beatbox.wav`), on a host that gives the screen sound. The `music-player` template: a folder of files that the stock `affrescoui_launcher` runs, with nothing compiled for it.

```sh
./build/affrescoui_launcher examples/tempo/index.screen.json --socket out/s
./build/affrescoui test examples/tempo/index.scenarios.json
```

| File | What it is |
|---|---|
| `audio/` | `beatbox.wav`, the loop Tempo plays, and `make_beat.py`, which synthesises it (`python3 examples/tempo/audio/make_beat.py`) |
| `images/` | The pictures the screen shows |
| `pages.json` | The pages (albums, artists, a collection, search, now playing, sound), included by the screen |
| `strings.json` | The string tables: English, Spanish and French |
| `index.policy.json` | Its approval rules' example revisions (`affrescoui policy test`) |
| `index.scenarios.json` | Its scenarios (`affrescoui test`; ctest `scenarios` plays them with the rendering check's runner) |
| `index.screen.json` | The screen document |

Its fonts are in [`../shared/fonts`](../shared/fonts/README.md).

## What it shows

- **Screen documents in parts:** `include` of the pages and the string tables; pages as components with `use`.
- **Widgets:** virtualised tables (the library of 3,000 songs, a collection, search results), grids of albums and artists, a list for the queue (dragged, or moved with Alt+Up and Down), the `curves` control for a ten-band equaliser with presets, sliders, a waveform, a tab list for the menu.
- **A visualiser of what plays:** the beat declares `"bands": 32`, so its state has `spectrum`, how loud each band of frequencies was, 20 times a second (`"updates": 50`); two `path` nodes draw it as bars and their reflection (`computed.bars`). With no sound (headless, or a host that gives none) the bars stay low and flat.
- **A simulated player:** an `every` timer moves the song along, crossfades into the next one and steps the lyrics. The queue and the play order hold `list:track` keys (`-1` for the library), so any song can be queued.
- **Playlists as records:** made, renamed (a text field in place of the name) and deleted (a `dialog`) with `insert`, `update` and `delete` steps on the `playlists` model; the queue, the liked songs and what plays are remapped in the same action.
- **Sound:** the `audio` section declares the beat, a looping WAV; play and pause, a new song and the volume slider drive it through `watch` and `audio` steps, and its state is at `state.beat`. Tempo opens playing silently: the first press of play starts the beat. Hosts that give no sound (the rendering check, `affrescoui test`) leave everything else as it was.
- **A song's own sound:** a song (a record of the `songs` model, or a track of a `playlists` record) may name a sound of the `audio` section in its `sound` field, and playing that song plays it instead of the beat (`computed.sound`; `state.soundOn` names the one started). So "load this MP3 and link it to that song", with the file attached in the assistant, is one proposal the person reviews: the attachment as an asset, `"audio": { "own": { "file": "asset:song.mp3", "into": "state.sounds.own" } }`, and a run-once `update` of the song's record setting `"sound": "own"` (the data layer: a revision of the view never writes records). A change to the `audio` section is always reviewed. The crossfade is heard (a fade out, then in); the equaliser shapes the visualiser only, as the audio module has no filters.
- **Commands:** titled actions in the command palette (Cmd+Shift+P) and Quick Open of songs (Cmd+P); shortcuts for playback (Space, arrows; Cmd+S shuffles, Cmd+R repeats, Cmd+Alt+↓ mutes), pages (Cmd+L, Cmd+2, Cmd+3, Cmd+4 for the liked songs, Cmd+F, Cmd+5, Cmd+6; Cmd+[ and Cmd+] go back and forward), the queue (Cmd+U), a song's menu (a right click or Shift+F10: play, play next, add to the queue, add to or remove from a playlist, like), a playlist's menu (rename, delete), the mini player (Cmd+M), the theme (Cmd+T), the language (Cmd+Shift+L).
- **Settings** that persist (`settings` with a schema): language and crossfade; the theme, the equaliser and the volume persist too.
- **Themes and locales:** light and dark tokens; English, Spanish and French string tables.
- **Accessibility:** roles and names, live regions, the menu as a tab list with arrow keys, sliders and lists with the keyboard.
- **Layouts** from 1040 × 640 to full screen, and a mini player.

## How it holds together

- **Permissions:** `studio` peers (the person's client) change it directly; an agent's change waits for the person's review (on its card in the assistant chat, or from a `studio` peer) (a change to its actions always does, even under a grant), and a budget caps how many nodes an agent changes at once (20 added, 5 removed, 50 changed); no one may change the status bar. The theme, settings, equaliser and volume persist; the songs and the playlists are records of the `songs` and `playlists` models, the 3,000 songs made into the store once.
- **Threading and ownership:** it has no C++ of its own. The stock host owns the window and the screen, and runs the document on its UI thread.
- **Extension points:** the library, the playlists and the queue are entries of `state`; a page is a part of `pages.json`; a language is a table in `strings.json`.
- **Assets:** the pictures in `images/` are simple vector drawings made for this project, under the repository's Apache-2.0 licence; the beat in `audio/` is synthesised by `make_beat.py` from sines and seeded noise (a kick, a snare, hi-hats, a clap and a bass blip; 120 beats a minute, four bars, 16-bit mono at 22,050 Hz, 8 seconds, -1 dBFS), this project's own work under the same licence, with no samples from anywhere. The fonts' licences are with them in `../shared/fonts`.
