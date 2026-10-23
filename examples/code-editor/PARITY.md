# Quaderno parity

Quaderno is a desktop text editor. This matrix lists the areas and workflows a daily user of such an editor
expects, each `done`, `partial` or `missing`, with the gap in one line. The design and the workflows are the
reference; Quaderno keeps its own name, palette, icons, text and sample project.

| Area | Workflow | Status | Gap |
|---|---|---|---|
| Look | Dark theme, warm accent, muted chrome | done | |
| Look | Menu bar: File, Edit, Selection, Find, View, Goto, Tools, Help | done | no Project or Window menus, no submenus |
| Look | Syntax colours by role (keywords, strings, comments, types, numbers, functions) | done | method calls stay plain |
| Look | Tab strip: active tab joins the editor, a dot on unsaved tabs (× on hover) | partial | tabs are flat, not the sloped shape |
| Look | Side bar with OPEN FILES and FOLDERS | done | no indent guide lines, as in the reference |
| Look | Minimap | done | no viewport box drag |
| Look | Status bar: panel switcher, line and column, branch, indentation, syntax | done | also the problem counts, kept for the scenarios |
| Look | Gutter with line numbers and fold markers | partial | no fold markers shown |
| Look | Indent guides, bracket matching | done | the paired brackets are underlined |
| Look | Rulers | done | off by default; View, Toggle Ruler shows one at column 80 |
| Navigation | Goto Anything (Cmd+P) by file name | done | two-line rows: name, then its path |
| Navigation | Goto Anything with `@` symbol, `:` line, `#` search | done | also after a file name: `file@symbol`, `file:line` |
| Navigation | Command Palette (Cmd+Shift+P) | done | "Category: Title" rows in Title Case, no placeholder |
| Navigation | Goto Symbol in Project | done | symbols by name with a kind badge, from a heuristic: classes, functions, Markdown headings |
| Navigation | Next and previous tab | done | |
| Editing | Multiple selections | done | |
| Editing | Find panel (Cmd+F), replace | done | regex, case, whole word and in-selection toggles |
| Editing | Find in Files | partial | Find, Where and Replace rows; a results buffer with its matches marked, F4 to step through; no regex, replace is plain text |
| Editing | Layouts and split editing | missing | |
| Editing | Distraction Free mode | missing | |
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
| Settings | Settings as JSON | missing | |
| Settings | Key bindings as JSON | missing | |
| Settings | Theme selector with live preview | missing | |
| Extra | Problems list (from the sample build) | done | not in the reference; kept for the scenarios |

Counts: 24 done, 3 partial, 7 missing.
