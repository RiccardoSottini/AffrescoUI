# Forge parity

Forge is a desktop code workbench, in its own words, icons and sample data. Each area below is `done`, `partial` or
`missing`, with its gap in one line, what closing it needs, and a priority: P1 a daily user meets it, P2 now and then,
P3 rarely. Each round takes the gaps a daily user would notice first.

## Layout and chrome

| Area | Status | Gap |
|---|---|---|
| Title bar, search box | done | |
| Back and forward navigation | done | |
| View rail | done | |
| Side bar views, title rows | done | |
| Editor tabs | done | |
| Breadcrumbs | done | |
| Minimap and overview ruler | done | The minimap is a setting (off by default); the scroll bar marks problems, breakpoints, matches and the caret |
| Panel (Problems, Output, Debug Console, Terminal) | done | |
| Status bar | partial | P3. Files are read and written as UTF-8 only; the encoding item is disabled and says so. Needs a library feature: the `fs` service reading and writing other encodings |
| Notifications centre | done | |
| Sashes | done | |
| Welcome page | partial | P2. No recent folders: the host's folder picker gives a document a name, never a path to reopen. Needs a host feature: a granted folder the person can reopen by name |

## Workflows

| Workflow | Status | Gap |
|---|---|---|
| Open Folder | done | |
| Quick Open (Cmd+P) | done | |
| Command palette (Cmd+Shift+P) | done | |
| Toggle side bar, panel, layout | done | |
| Explorer: open, create, rename, delete, move, reveal | done | |
| Editing, find and replace in a file | done | |
| Search across files, replace all, search editor | done | |
| Source control: stage, commit, branches, stash, remotes, commit graph | partial | Conflicts are resolved in the file (each side tinted, a bar accepts the current, incoming or both), not in a three-way merge editor (P2: JSON, three editors over git's stages 1 to 3, or a library merge view); a push or pull that asks for a password prompts in the terminal that started the host (P2: a host feature, an askpass prompt in the window) |
| Run and debug | done | |
| Terminal | done | |
| Problems and language features | partial | P1. Through the `lsp` service, one language server (clangd). JSON only: per server (Python, TypeScript) a program grant in app.json, an `lsp` service entry, and the lsp feature's calls routed by the file's language |
| Outline | partial | P2. A language's symbols where the language server runs (C and C++); Python's classes and functions and Markdown's headings by their text; other files the lines that open a block. Grows with the language servers above (JSON only) |
| Features (extensions) view | done | |
| Settings editor | done | |
| Themes | done | |
| Split editors | done | |
| Agent view | done | |
| Assistant chat | missing | P2. Waits for the shared chat add-on (a shared kit feature, then JSON to place it) |

## Not planned

| Area | Why |
|---|---|
| Extension marketplace | Forge's features are part of its document, not downloaded |
| Remote windows and accounts sync | They need network services the stock host does not grant |
