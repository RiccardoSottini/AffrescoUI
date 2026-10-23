# Quaderno parity

Quaderno is modelled on Sublime Text. This matrix lists the areas and workflows a daily user of such an editor
expects, each `done`, `partial` or `missing`, with the gap in one line. The design and the workflows are the
reference; Quaderno keeps its own name, palette, icons, text and sample project.

| Area | Workflow | Status | Gap |
|---|---|---|---|
| Look | Dark theme, warm accent, muted chrome | done | |
| Look | Syntax colours by role (keywords, strings, comments, types, numbers) | done | |
| Look | Tab strip: flat tabs, active tab joins the editor | partial | tabs are flat, not the sloped shape |
| Look | Side bar with OPEN FILES and FOLDERS | done | |
| Look | Minimap | done | no viewport box drag |
| Look | Status bar: line and column, indentation, syntax | done | |
| Look | Gutter with line numbers and fold markers | partial | no fold markers shown |
| Look | Indent guides, bracket matching | done | |
| Look | Rulers | done | one at column 80 |
| Navigation | Goto Anything (Cmd+P) by file name | done | |
| Navigation | Goto Anything with `@` symbol, `:` line, `#` search | partial | the prefix starts the query; no file name before it |
| Navigation | Command Palette (Cmd+Shift+P) | done | |
| Navigation | Goto Symbol in Project | missing | |
| Navigation | Next and previous tab | done | |
| Editing | Multiple selections | done | |
| Editing | Find panel (Cmd+F), replace | done | regex, case, whole word and in-selection toggles |
| Editing | Find in Files | missing | |
| Editing | Layouts and split editing | missing | |
| Editing | Distraction Free mode | missing | |
| Editing | Projects (folder tree, open files) | done | one sample project |
| Editing | Build (Cmd+B) with an output panel | partial | build is Cmd+Shift+B, output in the Console |
| Editing | Console | done | a command line with build, run, help, clear |
| Editing | Save, save all, close with unsaved changes | done | |
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

Counts: 20 done, 4 partial, 9 missing.
