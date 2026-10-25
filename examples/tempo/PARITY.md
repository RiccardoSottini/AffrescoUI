# Tempo's parity with a desktop music player

What a daily user of a desktop music player expects, and where Tempo stands. Layout and behaviour only: every name,
picture, song and line of text is Tempo's own. `done`: it works as such a user expects. `partial`: it is there, with a
difference the note says. `missing`: not yet.

## The window

| Feature | Status | Notes |
|---|---|---|
| A sidebar of pages and playlists, the page beside it, the player bar below | done | |
| Light and dark themes | done | Cmd+T |
| Languages | done | English, Spanish and French (Cmd+Shift+L) |
| Small windows and full screen | done | From 1040 × 640 |
| Back and forward through the pages | done | The arrows by the name, Cmd+[ and Cmd+]: pages, playlists, albums and artists |

## The library

| Feature | Status | Notes |
|---|---|---|
| Playlists, each a table of its songs | done | Records of the `playlists` model |
| A library of thousands of songs in a table | done | 3,000, virtualised |
| Albums and artists in grids, each opening its songs | done | |
| Search that finds songs as you type | done | Fuzzy, on the title and the artist |
| Quick open of a song, a palette of commands | done | Cmd+P, Cmd+Shift+P |
| Like a song, and a list of the liked ones | missing | |
| A context menu on a song: play next, add to the queue | missing | |

## Playing

| Feature | Status | Notes |
|---|---|---|
| Play, pause, previous, next, seek, volume | done | Space and the arrows too |
| The sound of what plays | done | A beat for the simulated songs; a song may name a sound of its own (an attached file) |
| Shuffle | done | Cmd+S: the songs up next shuffled, and back in order |
| Repeat: off, all, one | done | Cmd+R |
| A queue to see and reorder | done | By dragging, or Alt and the arrows |
| Crossfade into the next song | partial | Visual only: the sound doesn't fade |
| Equaliser with presets | partial | On a curves control; visual only, the sound isn't filtered |
| Now playing: the cover, a visualiser and the lyrics | done | The visualiser is the spectrum of what plays (the sound's `bands`) |
| Lyrics that follow the time; click a line to go there | done | |
| Mini player | done | Cmd+M, inside the window |
| Mute | done | The volume icon, Cmd+Alt+↓; the volume is kept |

## Everywhere

| Feature | Status | Notes |
|---|---|---|
| Keyboard access and accessibility names | done | Roles, names and live regions; the menu is a tab list |
| Settings that persist | done | Language, crossfade, theme, equaliser, volume |
| Agents propose; the person reviews | done | A change to the audio section always waits for the person |

| Round | Done | Partial | Missing |
|---|---|---|---|
| 1 (a folder of files) | 17 | 3 | 6 |
| 2 | 22 | 2 | 2 |
