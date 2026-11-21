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
| Editor tabs | done | Next and Previous Editor, Close Editors to the Right, Close All, Reopen Closed Editor; a head per group with Split Editor Right and More Actions (the editor's menu, also on a right click of the tabs) |
| Breadcrumbs | done | |
| Minimap and overview ruler | done | The minimap is a setting (off by default); the scroll bar marks problems, breakpoints, matches and the caret |
| Panel (Problems, Output, Debug Console, Terminal) | done | Problems is a tree: each file with its count, its problems under it with their severity icons, errors first; a file folds |
| Status bar | partial | P3. Files are read and written as UTF-8 only; the encoding item is disabled and says so. Needs a library feature: the `fs` service reading and writing other encodings |
| Notifications centre | done | |
| Sashes | done | |
| Welcome page | partial | Start (New File, Open Folder, Clone Git Repository, Go to File, Run a Command, each with its key), recent files with their folders, a four-step walkthrough checked as it is done, the daily keys. P2: no recent folders: the host's folder picker gives a document a name, never a path to reopen. Needs a host feature: a granted folder the person can reopen by name |

## Workflows

| Workflow | Status | Gap |
|---|---|---|
| Open Folder | done | |
| Clone a repository | done | The address in the palette, then an empty folder; git clones in a terminal of its own (the git grant runs no clone), where a password prompt can be answered |
| Quick Open (Cmd+P) | done | |
| Command palette (Cmd+Shift+P) | done | |
| Toggle side bar, panel, layout | done | |
| Explorer: open, create, rename, delete, move, reveal | done | |
| Editing, find and replace in a file | done | |
| Search across files, replace all, search editor | done | |
| Source control: stage, commit, branches, stash, remotes, commit graph | partial | Conflicts are resolved in the file (each side tinted, a bar accepts the current, incoming or both), not in a three-way merge editor (P2: JSON, three editors over git's stages 1 to 3, or a library merge view); a push or pull that asks for a password prompts in the terminal that started the host (P2: a host feature, an askpass prompt in the window) |
| Run and debug | done | |
| Terminal | done | |
| Problems and language features | done | A server per language, each its own `lsp` service routed by the file's extension: clangd (C, C++), pyright (Python), typescript-language-server (TypeScript, JavaScript), gopls (Go), rust-analyzer (Rust) |
| Outline | done | A language's symbols from its server where one runs; without one, Python's classes and functions and Markdown's headings by their text, other files the lines that open a block |
| Features (extensions) view | done | |
| Settings editor | done | A boolean setting is a check box with a tick |
| Themes | done | |
| Split editors | done | |
| Agent view | done | |
| Assistant chat | done | The shared assistant add-on, over the workbench, in the workbench's colours |

## Not planned

| Area | Why |
|---|---|
| Extension marketplace | Forge's features are part of its document, not downloaded |
| Remote windows and accounts sync | They need network services the stock host does not grant |
