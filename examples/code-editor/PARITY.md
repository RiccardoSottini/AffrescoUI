# Quaderno parity

Quaderno is a desktop text editor. This matrix lists the areas and workflows a daily user of such an editor
expects, each `done`, `partial` or `missing`, with the gap in one line. The design and the workflows are the
reference; Quaderno keeps its own name, palette, icons, text and sample project.

| Area | Workflow | Status | Gap |
|---|---|---|---|
| Look | Dark theme, warm accent, muted chrome | done | |
| Look | Menu bar: File, Edit, Selection, Find, View, Go, Tools, Help | done | no Project or Window menus, no submenus |
| Look | Syntax colours by role (keywords, strings, comments, types, numbers, functions) | done | method calls stay plain |
| Look | Tab strip: active tab joins the editor, a dot on unsaved tabs (× on hover) | done | Quaderno's own card tabs (rounded tops, a gap between them, the active one raised); right-click or Shift+F10 for Close, Close Others, Close Tabs to the Right, Reopen |
| Look | Side bar with Open Files and Project | done | no indent guide lines, as in the reference |
| Look | Minimap | done | no viewport box drag |
| Look | Status bar: panel switcher, line and column, branch, indentation, syntax | done | also the problem counts, kept for the scenarios |
| Look | Gutter with line numbers and fold markers | partial | no fold markers shown |
| Look | Indent guides, bracket matching | done | the paired brackets are underlined |
| Look | Rulers | done | off by default; View, Toggle Ruler shows one at column 80 |
| Navigation | Quick Switcher (Cmd+P) by file name | done | two-line rows: name, then its path |
| Navigation | Quick Switcher with `@` symbol, `:` line, `#` search | done | also after a file name: `file@symbol`, `file:line` |
| Navigation | All Commands (Cmd+Shift+P) | done | "Category: Title" rows in Title Case, no placeholder |
| Navigation | Find Symbol in Project | done | symbols by name with a kind badge, from a heuristic: classes, functions, Markdown headings |
| Navigation | Next and previous tab | done | |
| Editing | Multiple selections | done | |
| Editing | Find panel (Cmd+F), replace | done | regex, case, whole word and in-selection toggles |
| Editing | Find in Files | partial | Find, Where and Replace rows; a results buffer with its matches marked, F4 to step through; no regex, replace is plain text |
| Editing | Layouts and split editing | missing | |
| Editing | Distraction Free mode | done | Ctrl+Cmd+Shift+F or View: only the editor, with wide margins |
| Editing | Projects (folder tree, open files) | done | one sample project |
| Editing | Build (Cmd+B) with an output panel | done | a panel without tabs (Escape hides it), ending with the time taken |
| Editing | Console | done | a command line with build, run, help, clear |
| Editing | Save, save all, close with unsaved changes | done | the prompt places Don't Save, Cancel and Save as macOS does |
| Text | Snippets | missing | |
| Text | Toggle comment | done | line comments only |
| Text | Line operations (duplicate, move, delete) | done | also join, sort, reverse, unique, transpose |
| Text | Case conversion | done | |
| Text | Word wrap toggle | done | |
| Text | Hot exit | missing | |
| Settings | Settings as JSON | done | File, Settings opens Settings.json (theme, wordWrap, ruler, minimap, sideBar); saving applies it, a broken file is refused with a line in the console |
| Settings | Key bindings as JSON | partial | File, Key Bindings opens the shortcuts as a read-only JSON file; rebinding isn't available |
| Settings | Theme selector with live preview | done | Cmd+K Cmd+T: Dark, Light and System, previewed as the arrows move; the title bar's switch flips light and dark |
| Extra | Problems list (from the sample build) | done | not in the reference; kept for the scenarios |

Counts: 28 done, 3 partial, 3 missing.
