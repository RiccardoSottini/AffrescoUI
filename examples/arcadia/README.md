# Arcadia, a game launcher

A game launcher built only from what an outsider has: the public docs (`README.md`, `SCREEN_FORMAT.md`,
`VERSION_CONTROL.md`, `CHANGELOG.md`), the public headers, the templates, the CLI and the MCP server. It is a
single screen document, with no C++: the stock host runs it.

```sh
./build/affrescoui_host examples/arcadia/game-launcher.screen.json
./build/affrescoui test examples/arcadia/game-launcher.scenarios.json --out out/shots
python3 examples/arcadia/e2e_test.py build
```

## What it shows

- **Library**: collections in a `tree` (all, installed, favourites, genres), sorting by name, last played or play
  time (a chip row, a setting and the chord `mod+k mod+s`), a cover grid driven by the arrow keys (left/right move,
  up/down jump a row, mirrored in Arabic), a `table` view and a search field whose down arrow moves into the grid.
- **Game page**: a screenshot `carousel` with a caption, achievements with a progress bar, and dated news.
- **Friends**: made-up names with presence (online, in game, away, offline), an offline toggle and a short
  message box with a character count.
- **Downloads**: a queue that advances on an `every` timer at the speed set in settings, with pause, resume,
  reorder and cancel per item, pause all / resume all (`mod+shift+d`) and installed games when one finishes.
- **Settings**: theme (dark, light), language (English, Italiano, العربية), cover size, sort, speed, uninstalled
  games and confirmations, each a declared setting, with keyboard-usable sliders and a reset.
- **Languages**: every visible text goes through `strings` and `t()`, with ICU plurals; Arabic lays the screen out
  right to left (`dir` on the body).
- **Keyboard and commands**: every control has a role and a name; the commands back the buttons, menus, the
  context menu and the palette (`mod+shift+p`, `mod+p` for games); keys `mod+1..5`, `mod+f`, `mod+enter`,
  `mod+d`, `alt+left`, `escape` and the chords `mod+k mod+t` (theme) and `mod+k mod+l` (language).
- **Layouts**: from the 960 x 640 minimum, where the top bar drops the name and labels to fit, to full screen, where the cover
  grid takes the extra width.
- **Scenarios**: 33 in `game-launcher.scenarios.json`, covering each of the above, both themes, the three languages,
  keyboard-only use and the bugs found while walking the app across sizes, themes and languages.

### Gamepad

Not supported yet, on purpose. The library reads no gamepad or joystick today. GLFW already polls pads with
`glfwGetGamepadState`, so the simplest path is a host-level mapping from pad buttons to the key events the screen
already handles (d-pad to arrows, A to enter, B to escape, shoulders to `mod+1..5`). A screen then needs nothing new,
and every keyboard scenario covers the pad as well. A `gamepad` key name in `keys` can come later if a screen needs
buttons that keys can't express.

## What is in it

| Part | How it is made |
|---|---|
| Library, covers, details | A `split` (sidebar, covers, details) with a `tree` of collections, a repeated grid of cards with `image` covers (SVG files in `covers/`), and a `table` view with sorting |
| Top bar | `tabs` bound to `state.view` (role `tablist`), a search field, a palette button and a menu button (built-in `menu`); the window menu bar |
| Game page | The built-in `carousel` over the game's shots, achievements and news, all repeated from state |
| Friends, downloads | Repeated rows over state, an `every` timer for download progress, sliders with key bindings |
| Game actions | `commands` with titles, categories and icons; the same commands back the buttons, the menus, the context menu (built-in `contextMenu`) and the command palette |
| Quick pick | The built-in `quickPick` with two providers: games by name, and commands after `>` |
| Keys | `mod+p`, `mod+shift+p`, `mod+1..5`, `mod+f`, `mod+enter`, `mod+d`, `mod+shift+d`, `alt+left`, `escape` and the chords `mod+k mod+t`, `mod+k mod+l`, `mod+k mod+s` |
| Uninstall | The built-in `dialog`, shown only when the `launcher.confirmUninstall` setting is on |
| Feedback | The built-in `toast`, with a progress bar animated while a game "starts" |
| Stats | Bar, area and line charts from `examples/shared/components/charts.json`, stat cards (a local component) and a sessions `list` |
| Settings | Declared `settings` (theme, language, cover size, sort, download speed, uninstalled games, confirmations) and the built-in `settingsPage` |
| Look | Design tokens with dark and light variants switched by the theme setting; Lucide icons as a fallback font; rich text runs with bold, colour, a link and an icon |

## How it was built

1. Read `README.md` and `SCREEN_FORMAT.md`, then the kanban template and its scenarios for the shape of a real
   screen.
2. Wrote the screen in one go, then iterated with `affrescoui check` (fast, structural) and `affrescoui render -o`
   (a picture). Some errors only show at render time, such as a token where a length is expected.
3. Found the parameters of the built-in components (`dialog`, `toast`, `quickPick`, `contextMenu`, `settingsPage`,
   `menu`) from the checker's hints on a probe screen, since the docs name them but don't list their parameters.
4. Wrote nine scenarios (library, filter, table, play, uninstall, context menu, quick pick, stats, settings) and
   played them with `affrescoui test --out`, which also gives the captures. A second pass added the game page,
   friends, downloads, languages and themes, and grew them to 33.
5. Wrote `e2e_test.py`, the agent loop: the host runs the launcher, an agent reads it and proposes a change through
   the MCP server, renders the proposal, a Studio peer approves it on `studio.sock`, and the change is checked on
   main. It then plays the scenarios. It runs in CI as the `mcp_game_launcher` test.

## How it holds together

- **Permissions:** Studio peers change it directly; an agent's change waits for a Studio reviewer. It keeps nothing with `persist`.
- **Threading and ownership:** it has no C++ of its own. The stock host owns the window and the screen, and runs the document on its UI thread.
- **Extension points:** a game is an entry of `state.games`, a friend of `state.friends`; charts come from `../shared/components/charts.json`; `statCard` and `settingRow` are its local components.
- **Assets:** the pictures in `covers/` are simple vector drawings made for this project, under the repository's Apache-2.0 licence. The fonts' licences are with them in `../shared/fonts`.
