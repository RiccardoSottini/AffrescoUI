# Calamaio's parity

Calamaio follows the reference app, a classic extensible text editor in its graphical frame. This is the list of what
a daily user reaches for, and how far Calamaio goes: **done** (it behaves the same), **partial** (it is there, with
the difference in the note) or **missing**. The words, the sample texts and the pictures are Calamaio's own.

| Area | Feature | Status | Note |
|---|---|---|---|
| Frame | Menu bar | done | Drawn by the document, also on macOS; F10 opens the first menu |
| Frame | Tool bar | done | Lucide icons, a tip on each |
| Frame | Fringes | done | |
| Frame | Scroll bar | done | Shows where the window is; dragging it scrolls, point following as it does when the view leaves it |
| Frame | Splash screen | done | Its links run commands |
| Buffers | `*scratch*` | done | Runs what you type: `C-j`, `C-x C-e` |
| Lisp | `C-M-x`, `eval-region`, `eval-buffer`, init file, hooks | done | Calamaio's own Lisp, not the model's: a definition you typed lands in the history as yours; anything else is a proposal |
| Buffers | `*Messages*` | done | Every echo is appended |
| Buffers | `*Help*` | done | Shown in the other window; split if there is one |
| Buffers | `*Completions*` | done | In columns, the typed part dimmed, a click chooses |
| Buffers | `*Buffer List*` | done | `C-x C-b`; RET shows the line's buffer, `g` lists again, `q` leaves |
| Editing | Region case, `M-;`, `C-t`, `C-o`, sort lines | done | |
| Windows | `C-x 2`, `C-x 3` | done | Each window keeps its own point |
| Windows | `C-x 0`, `C-x 1`, `C-x o` | done | |
| Mode line | Flags, name, position, line, mode | done | `**` modified, `%%` read-only, All/Top/Bot/percent |
| Mode line | Inactive windows' look | done | |
| Echo area | Messages, a pending prefix (`C-x-`) | done | |
| Minibuffer | `M-x`, TAB completion | done | The common prefix, then the list; notes when nothing or only one matches |
| Minibuffer | History `M-p`, `M-n` | done | One history per kind of question, also on the arrows |
| Minibuffer | `C-g` | done | |
| Motion | `C-f C-b C-n C-p M-f M-b C-a C-e M-< M-> C-v M-v` | done | |
| Motion | `C-l` | done | Middle, then top, then bottom when pressed again at once |
| Mark | `C-SPC`, region, `C-x h`, `C-x C-x` | done | |
| Kill ring | `C-k M-d M-DEL C-w M-w C-y M-y` | done | Successive kills join; also on the clipboard |
| Undo | `C-/`, `C-_`, `C-M-_` | done | Redo is the editor's own |
| Search | isearch with lazy highlight | done | `C-s`, `C-r`, RET, `C-g` |
| Search | `M-%` | done | Asks at each match |
| Arguments | `C-u`, `M-<digit>` | done | Counts motions, deletions and typed characters (`C-u 3 x`, `C-u x`), and replays of a macro |
| Macros | `F3`, `F4` | done | The keys typed in a window, `Def` in the mode line while recording; `C-g` stops it. A listing's keys (a folder's `d`, `u`, `RET`...) and electric pairs are recorded too; the minibuffer's keys aren't |
| Rectangles | `C-x r k`, `C-x r y`, `C-x r t` | done | Between the mark's column and point's; short lines are padded with spaces |
| Registers | `C-x r s`, `C-x r i`, `C-x r SPC`, `C-x r j` | done | Text or a position, in its buffer; kept while Calamaio runs |
| Help | `C-h k`, `C-h f`, `C-h v`, `C-h b`, `C-h m`, `C-h e` | done | From the document's commands, keys and settings |
| Files | `C-x b`, `C-x C-f`, `C-x C-s`, `C-x k` | partial | Files are a sample folder in the document |
| Files | `C-x C-c` with a save prompt | partial | Asks, but the frame stays |
| Major modes | Fundamental, Text, Lisp, Python, C, Markdown, Org | partial | Names in the mode line; colours where the host has a grammar |
| Minor modes | Line numbers, visual line, show-paren, electric pair, auto-fill, whitespace, fill column indicator, menu bar, tool bar | done | From Options and `M-x`; auto-fill breaks at the last space before `fill-column` as a space is typed past it (`Fill` in the mode line); whitespace dots each space; `C-h m` lists the ones on |
| Dired | Listing, RET, `g`, `q`, `d`, `u`, `x`, `+`, `R`, `C` | done | `C-x d`; `x` asks yes or no; a buffer visiting a renamed file follows it. The files are the sample folder's |
| Shell | A shell buffer | missing | |
| Customize | A buffer of the variables, set and reset | done | Each variable's state, STANDARD or SAVED and set, and Reset to Standard; changes apply at once and are kept for the next session |
| Themes | `M-x load-theme` | done | default, inchiostro, pergamena |
| Tutorial | `C-h t` | done | Calamaio's own text, a buffer to practise in |
| Assistant | A binding changed through a reviewed revision | missing | |
