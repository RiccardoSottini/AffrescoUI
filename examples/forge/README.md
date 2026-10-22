# Forge: a demo IDE in JSON

Forge is a demo of what the library can carry: a code workbench whose whole user interface is one screen document,
which people and agents change while it runs. It uses only the library's public API. It is an example, not part of
the library.

```
./build/affrescoui_forge [folder] [--socket DIR] [--data DIR]
```

With a folder, Forge opens it; without one, it shows the welcome page and its Open Folder button. Like
`affrescoui_host`, it listens for the Studio and an agent in `DIR`. The layout, the theme and the settings are kept in
`--data DIR`.

## How it is built

`forge.screen.json` names the window, the fonts and the root, and includes one file per feature. Each feature lives in
its own folder, `features/<name>/`: its JSON, plus any C++ it needs.

| Feature | What it adds |
|---|---|
| `shell` | The title bar with the command centre, the activity bar (explorer, go to file, problems, settings), the side bar with the explorer, editor groups with tabs, the panel area, the status bar, zen mode, and the welcome page |
| `palette` | The command palette (`>`), quick open over the file index, go to line (`:`), and the settings and keybinding pages, generated from the contributions |
| `themes` | Themes as design tokens (Forge Dark, Forge Light), the VS Code colour theme importer (`ThemeImport.*`), and the file icon theme (`IconTheme.*`) |
| `accounts` | Signing in to the document server (`examples/server`) with a device code, settings synced through the person's `~settings` document, and who is on a shared document, and where, in the status bar (`Accounts.*` keeps the sign-in in the system's secret store); `accounts.scenarios.json` |
| `panels` | Problems, output, notifications (toasts and a list), and progress |

There are only four natives (`Forge.hpp`), for what JSON can't do on its own: the file system and the OS's dialogs.
- `forge.index`: the open folder as a tree and a flat file list, with file icons.
- `forge.openFolder`: asks the OS for a folder (`Window::chooseFiles`).
- `forge.read`: reads a file inside the open folder.
- `forge.importTheme`: asks for a VS Code theme and returns it as tokens.

A native's arguments never name a path outside the open folder.

## Contribution points

A feature adds itself to the workbench through the sections below. `include` merges them, and the including file wins
on a clash.

| Section | Contributes | Read by |
|---|---|---|
| `actions` | Commands: `{ "title", "category", "icon", "enablement", "description", "do" }` | The palette (`>`), menus, keys, and the keybindings page |
| `keys` | Key bindings, `"mod+shift+p": "showCommands"`; chords such as `"mod+k z"` | The keybindings page, the menus' shortcut labels |
| `menus` | Menu items, `menu → { key → { "run", "group", "order", "when" } }`; menu by menu, so several features add to `view` | The native menu bar (macOS, Windows), or the in-window one |
| `settings` | Typed settings with defaults and descriptions, read as `settings.workbench.fileIcons` | The settings page, which is generated from them |
| `tokens` | Colours and sizes per theme (`"@color.accent"`) | Every node; a theme switch restyles the workbench |
| `components` | Parts of the workbench, used as `{ "use": "forgePanel" }` | The root and other components |
| `state`, `computed`, `persist` | The feature's data, and what survives a restart | Its own components |

To add a view, write `features/<name>/<name>.json` with these sections and add it to `include` in
`forge.screen.json`. The palette, the menus and the settings and keybinding pages list it with no other change.

## Themes

A theme is a set of variants of the same tokens (`themes.json`), switched by `"theme": "{{state.theme}}"`.
- **Preferences > Import Color Theme...** reads a VS Code colour theme (JSON with comments). It maps the theme's
  `colors` onto Forge's colour tokens (a colour it lacks comes from its `type`), keeps it in `state.themes`, and switches to it.
- **The file icon theme** maps file names and extensions to Lucide glyphs and colours. Turn it off in the settings
  (`workbench.fileIcons`).

## Features from a catalogue

The four features above are also described as packages: each folder has a `feature.json` (RFC 0004) with its title,
release and requirements. Two more features ship only as packages: `features/word-count/` (the words of the text being
edited, in the status bar) and `features/todo-tree/` (the TODO and FIXME comments of the open files; it requires
`wordCount`).

`catalog/` is a signed catalogue of those two: `index.json` and one signed manifest per release. Forge trusts its
publisher, `examplePublisher` in `Forge.hpp`, and offers it through the `features` service, so the `installFeature`
and `upgradeFeature` actions install from it and enabling, disabling or removing one is a revision on
`/features/<name>`. The catalogue was signed by a throwaway key whose private half was discarded: only its public key
is in the repository, and only Forge trusts it. To change a manifest, sign the manifests and then the index again with
a key of your own, `affrescoui sign <file> --key KEYFILE` (make one with `--new-key`), and put its public key in
`examplePublisher`. `.gitattributes` keeps the signed files byte for byte. `features.scenarios.json` plays the catalogue,
an install, enabling and disabling, removal, and a feature that requires another.
