# Forge parity

Forge is laid out and works like the code editors people use every day. Each area below is `done`, `partial` or
`missing`, with its gap in one line. Each round takes the gaps a daily user would notice first.

## Layout and chrome

| Area | Status | Gap |
|---|---|---|
| Title bar, command center | done | |
| Activity bar | partial | No badge counts on Source Control |
| Side bar views, title rows | partial | No view actions (new file, refresh, collapse) in the title rows |
| Editor tabs | partial | No file-type icons; no preview (italic) tabs |
| Breadcrumbs | partial | Shows the path; no symbol segments |
| Minimap and overview ruler | missing | No minimap; no scrollbar markers |
| Panel (Problems, Output, Debug Console, Terminal) | partial | Opens with the window; the Terminal tab appears only after a terminal starts |
| Status bar | partial | No branch, encoding, line endings or indentation items |
| Sashes | done | |
| Welcome page | partial | Few start links, no recent folders |

## Workflows

| Workflow | Status | Gap |
|---|---|---|
| Quick Open (Cmd+P) | done | |
| Command palette (Cmd+Shift+P) | done | |
| Toggle side bar, panel, layout | done | |
| Explorer: open, create, rename, delete | partial | Native code; moves to the files service |
| Editing, find and replace in a file | partial | Native code; moves to the files service |
| Search across files, replace all | partial | Search lives in the side bar; the results come from native code |
| Source control: stage, commit, branches | partial | No repository in the sample folder; native code, moves to git through the process service |
| Run and debug | partial | No section headers; no floating debug toolbar |
| Terminal | partial | The cursor is an outlined box |
| Problems and language features | partial | Native code; moves to the RPC service |
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
