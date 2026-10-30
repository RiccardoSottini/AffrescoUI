# Calamaio, a text editor

A keyboard-driven text editor in the tradition of the classic extensible editors: a frame with a menu bar and a tool
bar, windows that split, mode lines, an echo area with a minibuffer, and the keys a daily user types. The `calamaio`
template: a screen document that the stock host runs, with no C++ of its own.

```sh
./build/affrescoui_launcher examples/calamaio/json/calamaio.screen.json
./build/affrescoui test examples/calamaio/json/calamaio.scenarios.json
```

| File | What it is |
|---|---|
| `calamaio.screen.json` | The screen document: the window, the fonts, the permissions and the layout of the windows |
| `calamaio.policy.json` | Its approval rules' example revisions (`affrescoui policy test`) |
| `calamaio.scenarios.json` | Its scenarios (`affrescoui test`) |
| `parts/look.json` | The themes: the faces as colours, the syntax colours, the kit's tokens for the menus |
| `parts/settings.json` | The variables (`fill-column`, `kill-ring-max`...): what `C-h v` describes and Customize sets |
| `parts/state.json` | The buffers, the windows and their layout, the minibuffer, the mark, the modes |
| `parts/files.json` | The sample home folder that `C-x C-f` visits |
| `parts/frame.json` | A window with its fringes and mode line, the splash screen, the menu bar, the tool bar, the echo area |
| `parts/commands.json` | Every command, by its name: what `M-x` runs and what a key, a menu or a link names |
| `parts/keys.json` | The key bindings |
| `parts/menus.json` | The menus |
| `parts/killring.json` | The kill ring: `C-k`, `M-d`, `C-w`, `M-w`, `C-y`, `M-y` |
| `parts/editing.json` | The region's case, comments, transposing, the minor modes Options toggles, Customize |
| `parts/listings.json` | A folder's files (`C-x d`) and the buffer list (`C-x C-b`) |
| `parts/guides.json` | The tutorial (`C-h t`) and the tour |
| `PARITY.md` | What it does like its model, and what not yet |

## Configured by its document

Changing how Calamaio works is a revision of its document, as in an init file: a binding is a line of `keys`
(`"ctrl+x ctrl+f": "find-file"`), a command is an entry of `actions` with a title and a description, a menu is an
entry of `menus`, and a theme is a set of colours. The keys are shown in the editor's own notation
(`window.keyLabels: "letters"`, so `ctrl+x ctrl+f` reads `C-x C-f`).

## How it holds together

- **Permissions:** Studio peers change it directly; an agent's change waits for a Studio reviewer, and a change to its
  commands, keys or menus always does. A budget caps how many nodes an agent changes at once (20 added, 5 removed, 50
  changed); no one may change the echo area. Only the settings persist, from one session to the next.
- **Threading and ownership:** the stock host owns the window and the screen, and runs the document on its UI thread.
- **Extension points:** buffers and files are entries of `state`; commands are `actions`, bindings `keys`, menus
  `menus`.
- **Assets:** no pictures of its own; icons are Lucide's, the fonts' licences are with them in `../shared/fonts`.
