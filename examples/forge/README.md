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
| `editor` | File buffers kept natively, never in the state (`Buffers.*`, `Editor.*`), shared by every group; preview tabs (a click in the explorer, replaced by the next), dirty markers, Save (`mod+s`), Save All (`mod+alt+s`), Revert File and Keep Editor; Problems read from the buffers. Files of a million lines open in about 0.1 s |
| `search` | Find in Files (`mod+shift+f`): an editor page that searches the open folder with the services' search engine (`Search.*`) as you submit, plain, whole word, case-sensitive or a regular expression, with include and exclude globs and the `.gitignore` files followed; the matches stream into a tree by file, a match opens at its line, and Replace All writes every file after a confirm, skipping those with unsaved edits and reloading the open ones |
| `explorer` | The side bar's tree, kept current by the file watcher (`Explorer.*`, a `DirectoryModel`), with new file and folder, rename, delete (after a confirm dialog), copy and paste, move by drag and drop, and reveal of the active file, from its context menu, the File menu and the palette |
| `terminal` | The panel's Terminal tab: up to 8 shells as tabs (`Terminal.*`, a VT terminal on libvterm in `TerminalModel.*`, on the `pty` provider), with bounded scrollback, the pty resized with the node, and no title or clipboard writes unless allowed; New Terminal is `mod+alt+t` |
| `debug` | Run and Debug (`mod+shift+d`): launch configurations from `.vscode/launch.json`, sessions with lldb-dap or debugpy on the `dap` provider, a floating toolbar and F5, Shift+F5, Cmd+Shift+F5, F6, F9, F10, F11, Shift+F11; gutter breakpoints (a click toggles one, Shift+click edits its condition, hit count or log message, Alt+click disables it) marked verified or not, the Breakpoints list with the exception filters, the call stack per thread, variables as a lazy tree paged by 100, Set Value, watches, the debug console with history and completion, and a value on hover while paused; `debug.scenarios.json` |
| `scm` | Source control (Cmd+Shift+G): git's status in staged changes and changes, stage and unstage, commit, branches, a change's diff against HEAD, and its gutter marks; git's output is read strictly in C++ (`GitStatus.*`, `LineDiff.*`) |
| `agent` | The agent pane (Cmd+Alt+I): a request in plain words goes to the connected agents, and their change comes back as a proposal, reviewed here with its changes, evidence (screenshots, scenario results) and a live preview, then accepted or rejected; and Bottega, the Features view (Cmd+Shift+X): the host's catalogue, install, enable and disable |

The shell has four natives (`Forge.hpp`), for what JSON can't do on its own: the file system and the OS's dialogs.
- `forge.index`: the open folder as a tree and a flat file list, with file icons.
- `forge.openFolder`: asks the OS for a folder (`Window::chooseFiles`).
- `forge.read`: reads a file inside the open folder.
- `forge.importTheme`: asks for a VS Code theme and returns it as tokens.

The editor feature adds `forge.open`, `forge.save`, `forge.saveAll`, `forge.revert`, `forge.close` and `forge.todos`
(`features/editor/Editor.hpp`) for the buffers, and the `code` type's `"buffer": id`. A save writes a file beside the
old one, then renames it over it.

A native's arguments never name a path outside the open folder.

Source control runs git through the `process` service, not a native (`features/scm/Scm.cpp`). The host grants one
program and Forge's own commands only (`GitArguments.*`): status, add and restore with paths after `--`, `commit -F -`
(the message goes to git's input), the branch list, `switch` to a checked branch name, and `show HEAD:<path>`. Each run
is given as an array, without a shell, against the open folder's repository (`--git-dir`, `--work-tree`). Its output is
bounded and parsed by `forge.scm.parseStatus`, `forge.scm.parseBranches` and `forge.scm.diff`.

The debugger (`features/debug/Debug.*`) grants `lldb-dap` and `debugpy` (`python3 -m debugpy.adapter`) when they are on
PATH, so either one works without a setting:
- lldb-dap, for C and C++: on macOS, `xcrun -f lldb-dap` names it (put its folder on PATH); elsewhere it comes with
  LLVM. A configuration of type `lldb` (or `lldb-dap`) launches `program` or attaches to `pid`.
- debugpy, for Python: `python3 -m pip install debugpy`. A configuration of type `python` (or `debugpy`) launches
  `program` or attaches to `port` on 127.0.0.1.

Forge reads this subset of `launch.json` (comments allowed): `name`, `type`, `request` (`launch` or `attach`),
`program`, `args`, `cwd`, `env`, `stopOnEntry`, `console`, `pid` and `port`; `${workspaceFolder}` is the open folder.
Other keys are left out, and configurations of other types are counted as skipped. For example:

```json
{ "version": "0.2.0", "configurations": [
	{ "name": "Run main", "type": "python", "request": "launch", "program": "${workspaceFolder}/main.py" },
	{ "name": "Debug app", "type": "lldb", "request": "launch", "program": "${workspaceFolder}/build/app", "args": ["--verbose"] }
] }
```

A frame outside the open folder opens only after a confirm, read-only. `debug.scenarios.json` plays what
`tests/dap/fake_adapter.py` answers through the `debugApply*` actions, so it runs without an adapter.

The agent pane talks to the agents through Forge's own host: a channel to `studio.sock` in Forge's socket folder
(`--socket`), relayed by the host to the peers on `agent.sock`. Agents hold `ask` permission there, so what one changes
is a proposal: the pane never applies it, and accepting it names the head the person reviewed, so a proposal that
changed since is refused. A reply, a proposal's changes and the catalogue are bounded before they are kept. Enabling or
disabling a feature is a revision on `/features/<name>/enabled` (a name holding `/` or `~` is dropped), so it is
reviewable and can be undone; installing one is the host's `features` service, which grants what a feature may do.

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

## Language servers

`features/lsp/` talks to one language server per language over a `jsonrpc` service: `lsp-cpp` (clangd),
`lsp-python` (pyright) and `lsp-typescript` (typescript-language-server). `main.cpp` grants each one it finds on
PATH; a server that isn't installed isn't granted, and the editor works without it. A server starts with the first
file of its language, and gets incremental changes in the encoding it chose. It gives diagnostics (squiggles, and rows
in Problems), hover, completion (fuzzy, Ctrl+Space) and signature help, Go to Definition (F12), Find References
(Shift+F12, a peek list), Rename (F2) and Format Document (Shift+Alt+F) as a list of edits applied only after you
confirm it, symbols (Cmd+Shift+O for the file with `@`, Cmd+T for the folder with `#`) and semantic colours.

A server is untrusted: its answers are bounded (`LspProtocol.hpp`), locations and edits outside the open folder are
dropped or refused, and one that crashes is started again at most 3 times.

To try it: install clangd (`brew install llvm`, or your system's `clangd` package) or pyright (`npm install -g
pyright`), check that `clangd --version` or `pyright-langserver --version` runs, then open a folder with C++ or Python
files in `affrescoui_forge`. A C++ folder needs a `compile_commands.json` (CMake writes one with
`-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`). `tests/lsp/fake_server.py` is a small server with fixed answers; its
`--print METHOD` shows the answer that `lsp.scenarios.json` plays without a process.

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
