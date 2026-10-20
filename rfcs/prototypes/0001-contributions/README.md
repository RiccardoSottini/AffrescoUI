# Prototype for RFC 0001: contributions

What [RFC 0001](../../0001-contributions.md) claims, tried out:

| File | What it is |
|---|---|
| `today.screen.json` | Runs **today**. A command palette, a menu, a keybinding editor and a settings page, each an ordinary component over a list. The lists (`state.commands`, `state.menus`, `state.settingsSchema`) are kept in the state by hand, because the variables the RFC adds don't exist yet |
| `proposed.screen.json` | The same app in the RFC's syntax: commands are actions with a `title`, `keys` has a chord and a binding with `when`, and `menus` and `settings` are sections. The components read `commands`, `menus`, `keybindings` and `settingsList`. It doesn't load today |
| `edits.json` | What an agent sends to add a command with a shortcut and a menu entry: today (two ways), with the RFC, and with the two alternatives the RFC rejects |

Try it, from the repository root, after building:

```sh
./build/affrescoui check rfcs/prototypes/0001-contributions/today.screen.json
./build/affrescoui render rfcs/prototypes/0001-contributions/today.screen.json -o contributions.png
./build/affrescoui run rfcs/prototypes/0001-contributions/today.screen.json --socket out/s
```

The first frame shows the settings page (a toggle, a slider and a row of choices, picked by each setting's type), the
keybinding table, an open menu with a separator between its groups, and the palette. In the running app, typing in
the palette filters it, Enter or a click runs the command, and the controls change the settings.

Count the tokens (tiktoken, as `tests/perf/tokens.py` does):

```sh
pip install -r tests/perf/requirements.txt
python3 rfcs/prototypes/tokens.py rfcs/prototypes/0001-contributions
```

What it shows: the four surfaces need nothing beyond today's components, `repeat`, `when` and computed values. What
the RFC removes is the hand-kept lists (a command written in three to four places), and what it adds is chords,
`when`, contributed menu items and validated settings, which the state-based version can't express.
