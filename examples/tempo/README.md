# Tempo, a music player

Playlists, a library of 3,000 songs in a table, a queue with a waveform, a playback timer and seeking. The `music-player` template: a screen document with no C++ of its own, which the stock host runs.

```sh
./build/affrescoui_host examples/tempo/music-player.screen.json
./build/affrescoui test examples/tempo/music-player.scenarios.json
```

| File | What it is |
|---|---|
| `images/` | The pictures the screen shows |
| `music-player.policy.json` | Its approval rules' example revisions (`affrescoui policy test`) |
| `music-player.scenarios.json` | Its scenarios (`affrescoui test`; the rendering check plays them too) |
| `music-player.screen.json` | The screen document |

Its fonts are in [`../shared/fonts`](../shared/fonts/README.md).
