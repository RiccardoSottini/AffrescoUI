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
| Like a song, and a list of the liked ones | done | The heart in the player or a song's menu; Cmd+4 shows them. Playing one, the liked songs after it (up to 50) come next |
| A context menu on a song: play next, add to the queue | done | A right click or Shift+F10: play, play next, add to the queue, add to a playlist, like, remove from this playlist. The queue takes any song, from any mix, the library or a search |
| Make, rename and delete playlists | done | The "+" by the mixes, a playlist's "..." menu, a double click on its name; deleting asks first |
| Add a song to a playlist, remove it | done | The song's menu: "Add to playlist" (or a new one), "Remove from this playlist" |

## Playing

| Feature | Status | Notes |
|---|---|---|
| Play, pause, previous, next, seek, volume | done | Space and the arrows too; previous restarts a song past its first 3 seconds |
| The sound of what plays | done | A beat for the simulated songs; a song may name a sound of its own (an attached file) |
| Shuffle | done | Cmd+S: the songs up next shuffled, and back in order |
| Repeat: off, all, one | done | Cmd+R |
| A queue to see and reorder | done | By dragging, or Alt and the arrows |
| Crossfade into the next song | partial | Heard: the song fades out, the next fades in. One sound plays at a time, so the two don't overlap |
| Equaliser with presets | partial | On a curves control; it shapes the visualiser, and a note says the sound isn't filtered (the audio module has no filters yet) |
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
| 3 | 22 | 4 | 0 |
| 4 (playlists, a queue of any song, a distinct look) | 26 | 2 | 0 |
