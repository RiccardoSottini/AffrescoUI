# Forge: a demo IDE in JSON

Forge is a demo of what the library can carry: a code workbench whose whole user interface is one screen document,
which people and agents change while it runs. It is JSON only, with no C++ of its own: an app (`app.json`) that the
stock host opens, from the Launcher or from the command line. It is an example, not part of the library.

```
./build/affrescoui_launcher examples/forge/json/app.json [--socket DIR] [--data DIR]
```

`app.json` asks for what Forge uses, and the host asks the person before granting any of it: the folder to work in
(`workspace`, read and written), the programs its services run (the login shell, sh for the debugger's
terminal, git, clangd, lldb-dap and debugpy's adapter, each one found on PATH), the sign-in and sync addresses, the open dialog (a colour theme's import)
and the assistant. Without the folder it shows the welcome page. The layout, the theme and the settings are kept in
`--data DIR`.

## How it is built

`index.screen.json` names the window, the fonts and the root, and includes one file per feature. Each feature lives in
its own folder, `features/<name>/`: its JSON and its scenarios.

| Feature | What it adds |
|---|---|
| `shell` | The title bar with the search box, the view rail (explorer, go to file, problems, settings), the side bar with the explorer, editor groups with tabs, the panel area, the status bar, zen mode, and the welcome page |
| `palette` | The command palette (`>`), quick open over the file index, go to line (`:`), and the settings and keybinding pages, generated from the contributions |
| `themes` | Themes as design tokens (Forge Dark, Forge Light), the colour theme import (`openFile` and `themeSources`), and the file icon theme |
| `accounts` | Signing in to the document server (`apps/server`) with a device code, settings synced through the person's `~settings` document, and who is on a shared document, and where, in the status bar ; `accounts.scenarios.json` |
| `panels` | Problems, output, notifications (toasts and a list), and progress |
| `editor` | Files in the library's `editor` node, read and written through the `fs` service and shared by every group; preview tabs (a click in the explorer, replaced by the next), dirty markers, Save (`mod+s`), Save All (`mod+alt+s`), Revert File and Keep Editor |
| `search` | Find in Files (`mod+shift+f`): an editor page that searches the open folder with the `search` service as you submit, plain, whole word, case-sensitive or a regular expression, with include and exclude globs and the `.gitignore` files followed; the matches stream into a tree by file, a match opens at its line, and Replace All writes every file after a confirm, skipping those with unsaved edits and reloading the open ones |
| `explorer` | The side bar's tree, kept current by the `watch` service, with new file and folder, rename, delete (after a confirm dialog), copy and paste, move by drag and drop, and reveal of the active file, from its context menu, the File menu and the palette |
| `terminal` | The panel's Terminal tab: up to 8 shells as tabs, each the library's `terminal` node on the `pty` provider, run, cleared and closed by the `runInTerminal`, `clearTerminal` and `closeTerminal` steps, with bounded scrollback, the pty resized with the node, and no title or clipboard writes unless allowed; New Terminal is `mod+alt+t` |
| `debug` | The debugger (`mod+shift+d`): launch configurations from `.forge/launch.json`, sessions with lldb-dap or debugpy on the `dap` provider, a floating toolbar and F5, Shift+F5, Cmd+Shift+F5, F6, F9, F10, F11, Shift+F11; gutter breakpoints (a click toggles one, Shift+click edits its condition, hit count or log message, Alt+click disables it) marked verified or not, the Breakpoints list with the exception filters, the call stack per thread, variables as a lazy tree paged by 100, Set Value, watches, the debug console with history and completion, and a value on hover while paused; `debug.scenarios.json` |
| `scm` | Source control (Cmd+Shift+G): git's status in staged changes and changes, stage and unstage, commit, branches, a change's diff against HEAD, and its gutter marks; git's output is read strictly by the `gitStatus` and `gitBranches` expression functions, and `diffLines` compares a file with HEAD |
| `agent` | The agent pane (Cmd+Alt+I): a request in plain words goes to the connected agents, and their change comes back as a proposal, reviewed here with its changes, evidence (screenshots, scenario results) and a live preview, then accepted or rejected; and the Features view (Cmd+Shift+X): the host's catalogue, install, enable and disable |
| `editing` | The find and replace bar (`mod+f`, `mod+alt+f`): plain, whole word, case-sensitive or a regular expression, in the file or in the selection (`alt+l`), with the match count; word wrap (`alt+z`), bracket matching and indentation guides as settings; `editing.scenarios.json` |
| `workbench` | The secondary side bar with the outline (`mod+alt+b`), breadcrumbs, the minimap, the inline or side-by-side diff, Open Recent (`ctrl+r`), pinned tabs, moving an editor between groups, and settings and keybindings as JSON; `workbench.scenarios.json`, `layout.scenarios.json` |

Forge has no natives: everything outside the screen goes through the library's services, and each one's root is the
folder the person chose, so nothing reaches outside it. A save writes a file beside the old one, then renames it over it.

Source control runs git through the `process` service. The host's git grant runs only the commands a source control
view needs: status, add and restore with paths after `--`, `commit -F -` (the message goes to git's input), the branch
list, `switch` to a checked branch name, and `show HEAD:<path>`. Each run is given as an array, without a shell,
against the folder's own repository, with no hook or configured program of the repository's run.

The debugger runs `lldb-dap` and `debugpy-adapter` on the `dap` provider, when they are on PATH and the person allowed
them:
- lldb-dap, for C and C++: on macOS, `xcrun -f lldb-dap` names it (put its folder on PATH); elsewhere it comes with
  LLVM. A configuration of type `lldb` (or `lldb-dap`) launches `program` or attaches to `pid`.
- debugpy, for Python: `python3 -m pip install debugpy` installs `debugpy-adapter`. A configuration of type `python`
  (or `debugpy`) launches `program` or attaches to `port` on 127.0.0.1.

A theme is a set of variants of the same tokens (`themes.json`), switched by `"theme": "{{state.theme}}"`.
- **Preferences > Import Color Theme...** reads a colour theme (JSON with comments). It maps the theme's
  `colors` onto Forge's colour tokens (a colour it lacks comes from its `type`), keeps it in `state.themes`, and switches to it.
- **The file icon theme** maps file names and extensions to Lucide glyphs and colours. Turn it off in the settings
  (`workbench.fileIcons`).

## Language servers

`features/lsp/` talks to clangd through the `lsp` provider. A server that isn't installed, or that the person didn't
allow, doesn't start, and the editor works without it. A server starts with the first
file of its language, and gets incremental changes in the encoding it chose. It gives diagnostics (squiggles, and rows
in Problems), hover, completion (fuzzy, Ctrl+Space) and signature help, Go to Definition (F12), Find References
(Shift+F12, a peek list), Rename (F2) and Format Document (Shift+Alt+F) as a list of edits applied only after you
confirm it, symbols (Cmd+Shift+O for the file with `@`, Cmd+T for the folder with `#`) and semantic colours.

A server is untrusted: the provider bounds its answers, drops or refuses locations and edits outside the folder, and
starts one that crashes again at most 3 times (`tests/unit/lsp_service.cpp`).

To try it: install clangd (`brew install llvm`, or your system's `clangd` package), check that `clangd --version` runs,
then open a folder with C++ files in Forge. A C++ folder needs a `compile_commands.json` (CMake writes one with
`-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`). `tests/lsp/fake_server.py` is a small server with fixed answers; its
`--print METHOD` shows the answer that `lsp.scenarios.json` plays without a process.

## Features from a catalogue

The four features above are also described as packages: each folder has a `feature.json` (RFC 0004) with its title,
release and requirements. Two more features ship only as packages: `features/word-count/` (the words of the text being
edited, in the status bar) and `features/todo-tree/` (the TODO and FIXME comments of the open files; it requires
`wordCount`).

`catalog/` is a signed catalogue of those two: `index.json` and one signed manifest per release. Forge trusts its
publisher, and offers it through the `features` service, so the `installFeature`
and `upgradeFeature` actions install from it and enabling, disabling or removing one is a revision on
`/features/<name>`. The catalogue was signed by a throwaway key whose private half was discarded: only its public key
is in the repository, and only the rendering check trusts it (`tests/render/runner.cpp`): the host doesn't offer
catalogues yet. To change a manifest, sign the manifests and then the index again with
a key of your own, `affrescoui sign <file> --key KEYFILE` (make one with `--new-key`), and put its public key in
the runner. `.gitattributes` keeps the signed files byte for byte. `features.scenarios.json` plays the catalogue,
an install, enabling and disabling, removal, and a feature that requires another.
