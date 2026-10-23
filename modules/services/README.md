# services: local programs and folders a screen may use

`services` gives screens what lives on the machine (RFC 0002): a language server or any JSON-RPC program, a command
the host allows, a folder the host grants. `ServiceRegistry` implements `core`'s `Services` (`Services.hpp`), which
is all the format sees: the `services` section declares what a screen uses, and the `invoke`, `subscribe` and
`respond` steps call it, with results and stream events written to the state. Target: `affrescoui::services` (uses
`core`; POSIX processes and `forkpty`, or Win32 processes and pseudo consoles on Windows; FSEvents, inotify or ReadDirectoryChangesW to watch
folders).

## Public headers (`include/affresco/services/`)

| Header | Offers |
|---|---|
| `ServiceRegistry.hpp` | `ServiceRegistry` (the `Services`: `add` a provider, `addRoot`, `allowProcess`, `setExecutor`, `catalog`) |
| `Provider.hpp` | `Provider` and `Service` (the extension point), `ServiceContext`, `Grant`, `ProcessGrant`, `ServiceState` |
| `DirectoryModel.hpp` | `DirectoryModel`: every file and folder under a folder as a `DataModel` (rows `{path, name, kind, depth, size}`, folders first, by name), listed on a worker thread and kept current by the watcher, with each change as a range of rows. `Screen::setModel` lets a screen read it (`repeat: {"in": "models.<name>"}`) |
| `Schema.hpp` | Checks a declaration's configuration against a provider's schema (a small subset of JSON Schema) |
| `Search.hpp` | The search engine behind the `search` provider, for natives that search a folder themselves: `searchFiles` (streamed, cancellable, capped) and `replaceAll` |
| `Syntax.hpp` | The syntax engine behind the `syntax` provider, for natives that colour text themselves: `SyntaxDocument` (tree-sitter, with `edit` then `parse` re-reading only what changed, and the rows it changed), `highlight` (runs of `@syntax.*` styles), `fold`, `brackets`; bounded by `SyntaxLimits` (size, parse time, query matches, captures, depth). Languages: C, C++, JSON, Python, JavaScript, TypeScript, Markdown; their queries are in `syntax/queries/` |

The method, call and request types are `core`'s (`affresco/core/Services.hpp`), so the format depends on the
interface, not on this module.

## Providers

| Provider | Offers |
|---|---|
| `jsonrpc` | A JSON-RPC 2.0 program the host allowed, over its stdin and stdout, framed with `Content-Length` headers (LSP's way) or one message per line. Calls, notifications as streams, cancellation (`$/cancelRequest`), and requests from the program answered by `respond`. Methods listed in the declaration's `notifications` (at most 256) are sent without an id, and their call ends once sent |
| `process` | A command the host allowed, run once per `invoke`: its arguments and stdin from `with`, its exit code and output as the result, its lines as events while it runs |
| `fs` | A folder the host granted: `read`, `list` and `stat` (`{exists, kind, size, modified}`, modified in milliseconds since 1970), and unless the declaration is read-only `write`, `mkdir` (the folders missing above it too), `rename {path, to}` (fails with `exists` if `to` is taken), `delete` (a file, or a folder with all it holds; there is no trash) and `copy {path, to, free}` (with `free`, a taken `to` becomes the first free `<name> copy<ext>`, `<name> copy 2<ext>`...; each gives the new `{path}`), with UTF-8 text files and paths inside it. A write goes through a file beside it, so a reader never sees half of it. `rename`, `delete` and `copy` act on a link itself, never on what it points to |
| `watch` | A folder the host granted, watched: the `changes` stream sends `{changes: [{path, kind}], rescan}` batches, debounced (`debounce`, 20 to 2,000 ms, 100 by default), without the paths an `ignore` glob matches (`*` and `?` within a name, `**` across names; a glob without `/` matches any name). `rescan` means too much changed to list: read the folder again |
| `pty` | A command the host allowed, on a terminal of its own (no shell unless the host allows one as the program): `open {cols, rows}` starts it and streams a first event `{session}`, then its output as `{data}` text (UTF-8, a split character waits for the rest, other bytes become U+FFFD), and ends with `{code}`; `write {session, data}` types, `resize {session, cols, rows}` resizes (the program hears `SIGWINCH`), `signal {session, name}` sends `interrupt` (to the foreground group), `terminate`, `hangup` or `kill`. On Windows it needs a pseudo console (Windows 10 1809 or later); there `interrupt` types Ctrl+C and the other signals end the program and what it started. The Windows side isn't built in this repository's local checks |
| `search` | A folder the host granted, searched: `find` matches its text files against a plain, whole-word or case-sensitive pattern or a regular expression (PCRE2, with match limits) on several threads, filtered by `include` and `exclude` globs and the `.gitignore` files, one event `{file, matches}` per file, cancellable and capped at `maxResults`; `replace` returns a file's text with every match replaced (`$1` in an expression), for an `fs` write: it writes nothing |
| `syntax` | Text parsed with tree-sitter on worker threads (at most 4 calls at once, 4 MiB each): `languages`, `highlight {language, text, first, last}` gives `{styles, spans: [[line, start, end, style]], complete}` (columns in bytes, later runs over earlier), `folds` gives `{folds: [[first, last]]}` |

A notes app keeps its notes in a folder the host granted (`notes`): a button makes a folder, another duplicates the
open note beside it, and the list re-reads the folder after either.

```json
"services": { "notes": { "provider": "fs", "root": "notes" } },
"actions": {
  "newFolder": [{ "invoke": "notes.mkdir", "with": { "path": "{{state.folderName}}" }, "onDone": [{ "run": "reload" }],
                  "onError": [{ "set": "state.problem", "to": "{{error.message}}" }] }],
  "duplicate": [{ "invoke": "notes.copy", "with": { "path": "{{state.open}}", "to": "{{state.open}}", "free": true },
                  "onDone": [{ "set": "state.open", "to": "{{result.path}}" }, { "run": "reload" }] }],
  "reload": [{ "invoke": "notes.list", "with": { "path": "." }, "into": "state.entries" }]
}
```

## Invariants

- **The host grants, the document asks.** A screen names a service and a grant; it can't name a program or a folder.
  `allowProcess` takes an absolute program path and the arguments it starts with; `addRoot` a folder that exists.
  A declaration naming anything else is refused at load. A call adds arguments (`with.arguments`) only if the host
  gave the grant a `ProcessShaper`, its own rule that checks them (and may pick the folder the run starts in) on the
  main thread before the run: without one, such a call is refused. They are always an array, never a shell line.
- **Revisions can't widen it.** The `services` section is set by the screen file: a revision that changes it is
  refused, and a revision that adds a step using a service is reviewed (it is never applied without a person's
  approval). A revision may only use a declared service's methods that the provider marks revisable.
- **Paths stay inside the root:** `fs` refuses absolute paths, and resolves every other one (symbolic links and
  `..` included) before it is used: a path that ends outside the folder is refused.
- **Every byte read is bounded:** a frame's headers 8 KiB, a body or a line 8 MiB, an error message from a program
  1,000 bytes; a file read or written 8 MiB, a listing 10,000 entries, a `delete` or a `copy` 10,000 entries and a `copy` 256 MiB (past them nothing is touched); a `process` line 64 KiB (longer ones arrive in
  pieces), its input 1 MiB, its added arguments 64 of 4 KiB each, and with `collect` its whole output 8 MiB of UTF-8
  (a run that writes more is ended).
  A search skips files over 8 MiB, binary files (a NUL byte in the first 8 KiB), symbolic links, `.git` and folders
  over 64 deep; it reports at most 20,000 matches and looks at most at 100,000 files, a pattern is at most 4 KiB, a
  glob list 64 globs, and a `.gitignore` is read up to 64 KiB and 10,000 rules.
- **Watching is bounded:** a batch names at most 1,000 paths (past them it is a `rescan`), and batches come at most
  once per debounce interval, so a subscriber hears at most 1,000 / debounce batches a second; inotify watches at most
  100,000 folders (past them, or past the system's own limit, `DirectoryModel::watching()` is false); a
  `DirectoryModel` holds at most 200,000 rows (past them `truncated()` is true). A `watch` service has at most 4
  subscriptions. Symbolic links are listed but never followed, so nothing outside the root is listed or watched.
- **Terminals are bounded:** 8 open at once per `pty` service, 1,000 columns and rows, a write 64 KiB and 1 MiB
  typed but not yet taken (past it a write is refused as busy); output arrives in events of at most 16 KiB, at most
  4 MiB a second (past it the program waits, as on a slow terminal). Ending the service hangs up every terminal and
  kills what ignores it.
- **Concurrency is bounded:** per JSON-RPC service 256 calls waiting and 256 subscriptions, and 16 MiB waiting to be
  written; 16 runs at once per `process` or `fs` service; per screen 1,024 calls running, 256 subscriptions, 256
  requests waiting for `respond`; a stream keeps at most 10,000 events.
- **No sandbox.** A program runs with the user's rights, like anything the host starts: grant only programs you trust.

## Threads

Each running program has a thread reading its output (and a `pty` session one writing what is typed); `fs`, `process` and `search` calls run on worker threads (a `find` matches on up to 8 more); each watcher
has a thread of its own (and FSEvents a dispatch queue), and each `DirectoryModel` a worker that lists and resolves
changes, applied to the rows on the main thread. None calls the
format: results, events, requests and status changes are posted to the `Executor` set by `setExecutor`, so they run
on the main thread. A `Call` cancelled (or destroyed) gets no more callbacks. A `ServiceRegistry` being destroyed
stops its programs and joins its threads.

## Errors

- A declaration that can't be used (an unknown provider, a configuration the schema refuses, a grant the host didn't
  give) is a load error; `load` returns them all.
- After that, every outcome is a result or an error object `{code, message, data}`: JSON-RPC's codes from the
  program, or the module's own (a method not found, a timeout, cancelled, too large, the service stopped). Nothing
  throws.
- A program that exits makes its service `failed`, with the reason, in the `services` variable; calls waiting on it
  fail.

## Extension points

- **Another provider** (a database, a device): implement `Provider` (its name, configuration schema and methods) and
  `Service`, and `add` it to the registry.

- **Another file-watching backend:** implement `Watcher`'s platform part in a `files/Watcher<Platform>.cpp`, as
  inotify, FSEvents and ReadDirectoryChangesW do, and pick it in `CMakeLists.txt`.
- **Another syntax language:** fetch its grammar in `cmake/AffrescoUISyntax.cmake`, put its queries in
  `syntax/queries/`, and list it in `src/syntax/SyntaxLanguages.cpp`.

## Files

```
include/affresco/services/   the public API (above)
src/
  Providers.hpp        the general providers' factories, worker threads, paths kept under a root, the UTF-8 check
  Common.cpp           what they share: Tasks, underRoot(), isUtf8(), EitherStop
  Registry.cpp         ServiceRegistry: providers, grants, declarations checked, lazy start and restart
  Schema.cpp           checkSchema(): the subset of JSON Schema a provider's configuration is checked with
  rpc/                 the `jsonrpc` provider and its framing
    Framing.hpp, .cpp    Content-Length and line framing, every frame bounded
    JsonRpc.cpp          calls by id, notifications as streams, cancellation, requests answered by `respond`
  process/             programs started without a shell
    Process.hpp, .cpp    ChildProcess: posix_spawn in a process group, or CreateProcessW in a job object
    ProcessProvider.cpp  the `process` provider: one run per call, its lines streamed
    Pty.hpp, .cpp        PseudoTerminal: forkpty, or a pseudo console on Windows
    PtyProvider.cpp      the `pty` provider: sessions, typing, resizing, signals
  files/               folders the host grants
    FileProvider.cpp     the `fs` provider: read, write (through a file beside it) and list
    Watcher.hpp, .cpp    globs and debouncing, shared by the backends
    WatcherMac.cpp, WatcherLinux.cpp, WatcherWindows.cpp, WatcherWeb.cpp
                         FSEvents, inotify, ReadDirectoryChangesW, and none in the browser
    WatchProvider.cpp    the `watch` provider: debounced batches as a stream
    DirectoryModel.cpp   DirectoryModel: listing and rescans on a worker, rows changed on the main thread
  search/              the `search` provider and its engine
    SearchEngine.cpp     one thread walks the folder, several match its files
    SearchGlob.hpp, .cpp include and exclude globs, .gitignore rules
    SearchProvider.hpp, .cpp
  syntax/              the `syntax` provider and its engine (tree-sitter)
    Syntax.cpp           SyntaxDocument: parses, highlights, folds, brackets, each bounded
    SyntaxLanguages.hpp, .cpp  the built-in grammars and queries, compiled once
    SyntaxProvider.cpp   the provider: one parse per call, on worker threads
  dap/                 the `dap` provider
    Dap.cpp              a debug adapter the host granted, over Content-Length frames
    DapProtocol.hpp, .cpp  its messages, checked and bounded
    DapTransport.hpp, .cpp a granted program's pipes, or a loopback TCP connection
syntax/queries/        the highlight and fold queries of each language
```
