# Forge parity

Forge is a desktop code workbench, in its own words, icons and sample data. Each area below is `done`, `partial` or
`missing`, with its gap in one line. Each round takes the gaps a daily user would notice first.

## Layout and chrome

| Area | Status | Gap |
|---|---|---|
| Title bar, search box | done | |
| Back and forward navigation | done | |
| View rail | done | |
| Side bar views, title rows | partial | No view actions (new file, refresh, collapse) in the title rows |
| Editor tabs | partial | No file-type icons; no preview (italic) tabs |
| Breadcrumbs | partial | Shows the path; no symbol segments |
| Minimap and overview ruler | missing | No minimap; no scrollbar markers |
| Panel (Problems, Output, Debug Console, Terminal) | done | |
| Status bar | partial | Indentation, encoding and line endings are fixed texts, not pickers |
| Notifications centre | done | |
| Sashes | done | |
| Welcome page | partial | Few start links, no recent folders |

## Workflows

| Workflow | Status | Gap |
|---|---|---|
| Open Folder | done | |
| Quick Open (Cmd+P) | done | |
| Command palette (Cmd+Shift+P) | done | |
| Toggle side bar, panel, layout | done | |
| Explorer: open, create, rename, delete, move, reveal | done | |
| Editing, find and replace in a file | done | |
| Search across files, replace all | partial | Search lives in the side bar; the results come from native code |
| Source control: stage, commit, branches | partial | Native code; moves to git through the process service |
| Run and debug | partial | Sections don't collapse; the debugger glue is native code |
| Terminal | partial | The cursor is an outlined box |
| Problems and language features | partial | Native code; moves to the RPC service |
| Outline | partial | Headings and the lines that open a block, not a language's symbols |
| Features (extensions) view | partial | Single-line items; no header |
| Settings editor | partial | No table of contents; large checkboxes |
| Themes | done | |
| Split editors | done | |
| Agent view | partial | The body is empty without an agent |
| Assistant chat | missing | Waits for the shared chat add-on |

## Not planned

| Area | Why |
|---|---|
| Extension marketplace | Forge's features are part of its document, not downloaded |
| Remote windows and accounts sync | They need network services the stock host does not grant |
