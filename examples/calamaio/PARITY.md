# Calamaio's parity

Calamaio follows the reference app, a classic extensible text editor in its graphical frame. This is the list of what
a daily user reaches for, and how far Calamaio goes: **done** (it behaves the same), **partial** (it is there, with
the difference in the note) or **missing**. The words, the sample texts and the pictures are Calamaio's own.

| Area | Feature | Status | Note |
|---|---|---|---|
| Frame | Menu bar | done | Drawn by the document, also on macOS; F10 opens the first menu |
| Frame | Tool bar | done | Lucide icons, a tip on each |
| Frame | Fringes | done | |
| Frame | Scroll bar | partial | Shows where the window is; dragging it is not yet wired |
| Frame | Splash screen | done | Its links run commands |
| Buffers | `*scratch*` | done | |
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
| Motion | `C-l` | missing | |
| Mark | `C-SPC`, region, `C-x h`, `C-x C-x` | done | |
| Kill ring | `C-k M-d M-DEL C-w M-w C-y M-y` | done | Successive kills join; also on the clipboard |
| Undo | `C-/`, `C-_`, `C-M-_` | done | Redo is the editor's own |
| Search | isearch with lazy highlight | done | `C-s`, `C-r`, RET, `C-g` |
| Search | `M-%` | done | Asks at each match |
| Arguments | `C-u`, `M-<digit>` | partial | Counts motions and deletions; typed characters aren't repeated yet |
| Macros | `F3`, `F4` | missing | |
| Rectangles | `C-x r k`, `C-x r y`, `C-x r t` | missing | |
| Registers | `C-x r s`, `C-x r i`, `C-x r SPC`, `C-x r j` | missing | |
| Help | `C-h k`, `C-h f`, `C-h v`, `C-h b`, `C-h m`, `C-h e` | done | From the document's commands, keys and settings |
| Files | `C-x b`, `C-x C-f`, `C-x C-s`, `C-x k` | partial | Files are a sample folder in the document |
| Files | `C-x C-c` with a save prompt | partial | Asks, but the frame stays |
| Major modes | Fundamental, Text, Lisp, Python, C, Markdown, Org | partial | Names in the mode line; colours where the host has a grammar |
| Minor modes | Line numbers, visual line, show-paren, menu bar, tool bar | partial | From Options and `M-x`; auto-fill, whitespace and electric pair not yet |
| Dired | Listing, RET, `g`, `q` | partial | `C-x d`; marks, `d`, `x`, `+`, `R`, `C` not yet |
| Shell | A shell buffer | missing | |
| Customize | A buffer of the variables, set and reset | partial | Changes apply at once and are kept for the next session; no SET/SAVED states |
| Themes | `M-x load-theme` | done | default, inchiostro, pergamena |
| Tutorial | `C-h t` | done | Calamaio's own text, a buffer to practise in |
| Assistant | A binding changed through a reviewed revision | missing | |
