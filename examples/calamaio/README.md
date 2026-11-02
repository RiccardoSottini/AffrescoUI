# Calamaio, a text editor

A keyboard-driven text editor in the tradition of the classic extensible editors: a frame with a menu bar and a tool
bar, windows that split, mode lines, an echo area with a minibuffer, and the keys a daily user types. The `calamaio`
template: a screen document that the stock host runs, with no C++ of its own.

```sh
./build/affrescoui_launcher examples/calamaio/lisp
./build/affrescoui test examples/calamaio/lisp/calamaio.scenarios.json --out out/calamaio
```

| File | What it is |
|---|---|
| `calamaio.screen.lisp` | The screen document: the window, the fonts, the permissions and the layout of the windows |
| `calamaio.policy.lisp` | Its approval rules' example revisions (`affrescoui policy test`) |
| `calamaio.scenarios.json` | Its scenarios (`affrescoui test`) |
| `parts/look.lisp` | The themes: the faces as colours, the syntax colours, the kit's tokens for the menus |
| `parts/settings.lisp` | The variables (`fill-column`, `kill-ring-max`...): what `C-h v` describes and Customize sets |
| `parts/state.lisp` | The buffers, the windows and their layout, the minibuffer, the mark, the modes |
| `parts/files.lisp` | The sample home folder that `C-x C-f` visits |
| `parts/frame.lisp` | A window with its fringes and mode line, the splash screen, the menu bar, the tool bar, the echo area |
| `parts/commands.lisp` | Every command, by its name: what `M-x` runs and what a key, a menu or a link names |
| `parts/keys.lisp` | The key bindings |
| `parts/menus.lisp` | The menus |
| `killring.lisp` | The kill ring, as a program: `C-k`, `M-d`, `C-w`, `M-w`, `C-y`, `M-y` |
| `loop.lisp` | Extending it from inside: `C-M-x`, `C-x C-e`, `C-j`, your init file, hook points, asking what is defined |
| `parts/editing.lisp` | The region's case, comments, transposing, the minor modes Options toggles, Customize |
| `parts/listings.lisp` | A folder's files (`C-x d`) and the buffer list (`C-x C-b`) |
| `parts/guides.lisp` | The tutorial (`C-h t`) and the tour |
| `PARITY.md` | What it does like its model, and what not yet |

`lisp/` is its only source, a real program (RFC 0005): `killring.lisp` and `loop.lisp` are functions, and the other parts stay data where data reads naturally.

## The loop

Lisp you type runs in the running editor, and lands in its history.

```lisp
(+ 1 2)                                  ; C-j here: 3 appears on the next line. C-x C-e: in the echo area

(defcommand kill-line "Says so."         ; C-M-x here: the running command is replaced
  :title "Kill Line" :category "Killing"
  []
  (message "kill-line, as you just redefined it"))
                                         ; C-k now does that. C-x C-z takes it back, exactly
```

- **`M-x find-init`** opens your init file, your own definitions, or a start for them. `M-x eval-buffer` evaluates it, as a proposal, since you didn't type it: `M-x accept-proposal` takes it, then try the `say-hello` it holds with `C-c h`, and save any file to see its hook on `after-save`.
- **`M-x load-init`** treats the buffer as a file from elsewhere: a proposal in its own name, which `M-x accept-proposal` accepts and which never gets your authority. **`M-x export-init`** gives what you evaluated as a change set.
- **`M-x what-is-here`**, `(describe-function "kill-line")`, `(describe-key "ctrl+k")`, `(apropos "yank")`: ask what is defined.
- **What you type has your authority, and nothing else's does:** never from a timer or a handler. Text you didn't type here, whoever put it in the buffer, however you copied it, gets yours only when you say so: `C-x C-e`, `C-M-x`, `C-j` and `M-x eval-region` on it ask first, in a question the window draws itself, above anything the editor draws, with the whole text; **Run as Me** (or Enter) runs it as you, once, and **Don't Run** (or Escape) runs nothing. `M-x eval-buffer` doesn't ask: what it holds is a proposal (SECURITY.md, "The Lisp language").

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
