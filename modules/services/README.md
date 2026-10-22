# services: local programs and folders a screen may use

`services` gives screens what lives on the machine (RFC 0002): a language server or any JSON-RPC program, a command
the host allows, a folder the host grants. `ServiceRegistry` implements `core`'s `Services` (`Services.hpp`), which
is all the format sees: the `services` section declares what a screen uses, and the `invoke`, `subscribe` and
`respond` steps call it, with results and stream events written to the state. Target: `affrescoui::services` (uses
`core`; POSIX processes, or Win32 processes on Windows; FSEvents, inotify or ReadDirectoryChangesW to watch
folders).

## Public headers (`include/affresco/services/`)

| Header | Offers |
|---|---|
| `ServiceRegistry.hpp` | `ServiceRegistry` (the `Services`: `add` a provider, `addRoot`, `allowProcess`, `setExecutor`, `catalog`) |
| `Provider.hpp` | `Provider` and `Service` (the extension point), `ServiceContext`, `Grant`, `ProcessGrant`, `ServiceState` |
| `DirectoryModel.hpp` | `DirectoryModel`: every file and folder under a folder as a `DataModel` (rows `{path, name, kind, depth, size}`, folders first, by name), listed on a worker thread and kept current by the watcher, with each change as a range of rows. `Screen::setModel` lets a screen read it (`repeat: {"in": "models.<name>"}`) |
| `Schema.hpp` | Checks a declaration's configuration against a provider's schema (a small subset of JSON Schema) |

The method, call and request types are `core`'s (`affresco/core/Services.hpp`), so the format depends on the
interface, not on this module.

## Providers

| Provider | Offers |
|---|---|
| `jsonrpc` | A JSON-RPC 2.0 program the host allowed, over its stdin and stdout, framed with `Content-Length` headers (LSP's way) or one message per line. Calls, notifications as streams, cancellation (`$/cancelRequest`), and requests from the program answered by `respond` |
| `process` | A command the host allowed, run once per `invoke`: its arguments and stdin from `with`, its exit code and output as the result, its lines as events while it runs |
| `fs` | A folder the host granted: `read` and `list`, and `write` unless the declaration is read-only, with UTF-8 text files and paths inside it. A write goes through a file beside it, so a reader never sees half of it |
| `watch` | A folder the host granted, watched: the `changes` stream sends `{changes: [{path, kind}], rescan}` batches, debounced (`debounce`, 20 to 2,000 ms, 100 by default), without the paths an `ignore` glob matches (`*` and `?` within a name, `**` across names; a glob without `/` matches any name). `rescan` means too much changed to list: read the folder again |

## Invariants

- **The host grants, the document asks.** A screen names a service and a grant; it can't name a program or a folder.
  `allowProcess` takes an absolute program path and the arguments it starts with; `addRoot` a folder that exists.
  A declaration naming anything else is refused at load.
- **Revisions can't widen it.** The `services` section is set by the screen file: a revision that changes it is
  refused, and a revision that adds a step using a service is reviewed (it is never applied without a person's
  approval). A revision may only use a declared service's methods that the provider marks revisable.
- **Paths stay inside the root:** `fs` refuses absolute paths, and resolves every other one (symbolic links and
  `..` included) before it is used: a path that ends outside the folder is refused.
- **Every byte read is bounded:** a frame's headers 8 KiB, a body or a line 8 MiB, an error message from a program
  1,000 bytes; a file read or written 8 MiB, a listing 10,000 entries; a `process` line 64 KiB (longer ones arrive in
  pieces) and its input 1 MiB.
- **Watching is bounded:** a batch names at most 1,000 paths (past them it is a `rescan`), and batches come at most
  once per debounce interval, so a subscriber hears at most 1,000 / debounce batches a second; inotify watches at most
  100,000 folders (past them, or past the system's own limit, `DirectoryModel::watching()` is false); a
  `DirectoryModel` holds at most 200,000 rows (past them `truncated()` is true). A `watch` service has at most 4
  subscriptions. Symbolic links are listed but never followed, so nothing outside the root is listed or watched.
- **Concurrency is bounded:** per JSON-RPC service 256 calls waiting and 256 subscriptions, and 16 MiB waiting to be
  written; 16 runs at once per `process` or `fs` service; per screen 1,024 calls running, 256 subscriptions, 256
  requests waiting for `respond`; a stream keeps at most 10,000 events.
- **No sandbox.** A program runs with the user's rights, like anything the host starts: grant only programs you trust.

## Threads

Each running program has a thread reading its output; `fs` and `process` calls run on worker threads; each watcher
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
