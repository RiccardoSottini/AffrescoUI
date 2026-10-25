# Forge parity

Forge is a desktop code workbench, in its own words, icons and sample data. Each area below is `done`, `partial` or
`missing`, with its gap in one line. Each round takes the gaps a daily user would notice first.

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
| Status bar | partial | Files are read and written as UTF-8 only; the encoding item is disabled and says so |
| Notifications centre | done | |
| Sashes | done | |
| Welcome page | partial | No recent folders: the host's folder picker gives a document a name, never a path to reopen |

## Workflows

| Workflow | Status | Gap |
|---|---|---|
| Open Folder | done | |
| Quick Open (Cmd+P) | done | |
| Command palette (Cmd+Shift+P) | done | |
| Toggle side bar, panel, layout | done | |
| Explorer: open, create, rename, delete, move, reveal | done | |
| Editing, find and replace in a file | done | |
| Search across files, replace all | partial | No search editor (results open in the side bar only) |
| Source control: stage, commit, branches, stash, remotes | partial | No commit graph or merge editor (Commits lists the branch's last hundred, file by file); a push or pull that asks for a password prompts in the terminal that started the host |
| Run and debug | done | |
| Terminal | done | |
| Problems and language features | partial | Through the `lsp` service, one language server (clangd) |
| Outline | partial | A language's symbols only where the language server runs (C and C++); other files list headings or the lines that open a block |
| Features (extensions) view | done | |
| Settings editor | done | |
| Themes | done | |
| Split editors | done | |
| Agent view | done | |
| Assistant chat | missing | Waits for the shared chat add-on |

## Not planned

| Area | Why |
|---|---|
| Extension marketplace | Forge's features are part of its document, not downloaded |
| Remote windows and accounts sync | They need network services the stock host does not grant |
