# Arcadia, a game launcher

A game launcher built only from what an outsider has: the public docs (`README.md`, `SCREEN_FORMAT.md`,
`VERSION_CONTROL.md`, `CHANGELOG.md`), the public headers, the templates, the CLI and the MCP server. It is a
single screen document, with no C++: the stock host runs it.

```sh
./build/affrescoui_host examples/game-launcher/game-launcher.screen.json
./build/affrescoui test examples/game-launcher/game-launcher.scenarios.json --out out/shots
python3 examples/game-launcher/e2e_test.py build
```

## What is in it

| Part | How it is made |
|---|---|
| Library, covers, details | A `split` (sidebar, covers, details) with a `tree` of collections, a repeated grid of cards with `image` covers (SVG files in `covers/`), and a `table` view with sorting |
| Top bar | `tabs` bound to `state.view`, a search field, a menu button (built-in `menu`) and the window menu bar |
| Game actions | `commands` with titles, categories and icons; the same commands back the buttons, the menus, the context menu (built-in `contextMenu`) and the command palette |
| Quick pick | The built-in `quickPick` with two providers: games by name, and commands after `>` |
| Keys | `mod+p`, `mod+shift+p`, `mod+1..3`, `mod+enter`, `mod+d` and the chord `mod+k mod+t` |
| Uninstall | The built-in `dialog`, shown only when the `launcher.confirmUninstall` setting is on |
| Feedback | The built-in `toast`, with a progress bar animated while a game "starts" |
| Stats | Bar, area and line charts from `examples/assets/components/charts.json`, stat cards (a local component) and a sessions `list` |
| Settings | Declared `settings` (theme, cover size, sort, uninstalled games, confirmations) and the built-in `settingsPage` |
| Look | Design tokens with dark and light variants switched by the theme setting; Lucide icons as a fallback font; rich text runs with bold, colour, a link and an icon |

## How it was built

1. Read `README.md` and `SCREEN_FORMAT.md`, then the kanban template and its scenarios for the shape of a real
   screen.
2. Wrote the screen in one go, then iterated with `affrescoui check` (fast, structural) and `affrescoui render -o`
   (a picture). Some errors only show at render time, such as a token where a length is expected.
3. Found the parameters of the built-in components (`dialog`, `toast`, `quickPick`, `contextMenu`, `settingsPage`,
   `menu`) from the checker's hints on a probe screen, since the docs name them but don't list their parameters.
4. Wrote nine scenarios (library, filter, table, play, uninstall, context menu, quick pick, stats, settings) and
   played them with `affrescoui test --out`, which also gives the captures.
5. Wrote `e2e_test.py`, the agent loop: the host runs the launcher, an agent reads it and proposes a change through
   the MCP server, renders the proposal, a Studio peer approves it on `studio.sock`, and the change is checked on
   main. It then plays the scenarios. It runs in CI as the `mcp_game_launcher` test.
