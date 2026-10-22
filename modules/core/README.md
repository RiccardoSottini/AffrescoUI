# core: the contracts between modules

`core` is the small set of interfaces the other modules talk through, so that none of them depends on another's
implementation. The rendering engine posts work through an `Executor`, the live protocol reads and writes documents
through `VersionControl`, version control learns what a document means through `DocumentRules`, and peers arrive on a
`Channel`. It also holds what every module shares: `Signal`, the JSON type with its safe parser and typed readers,
and the log. It is headers only (`affrescoui::core`, an `INTERFACE` target) and has no dependency of its own: the JSON
headers come with nlohmann/json, which only the modules that use documents link. The one exception is
`TextPattern`, compiled with PCRE2 into `affrescoui::regex` (`cmake/AffrescoUIRegex.cmake`), which the modules that
match text (render, services) link privately.

## Public headers (`include/affresco/core/`)

| Header | Offers |
|---|---|
| `Signal.hpp` | `Signal<Args...>` and `Connection`: any number of slots on one event source, in connection order |
| `DataModel.hpp` | `DataModel` (rows the application owns: `size`, `row`, and `onChange` with a `ModelChange` range) and `ListModel`, one kept in memory |
| `Executor.hpp` | `Executor::post`: run a task on the main thread, from any thread |
| `CallProgress.hpp` | `CallProgress`: what a slow native call on a worker thread reads (`cancelled`) and reports (`progress`, `partial`); thread-safe |
| `Channel.hpp` | `Channel`: a two-way stream of one-line messages; `onMessage`, `onClosed`; `maxMessageBytes` (64 MiB) |
| `Json.hpp` | `json`, `DocumentError`, `parseUntrusted` (nesting limit `maxNesting` = 256), `toText`, the `expect::` readers |
| `DocumentRules.hpp` | `DocumentRules` (validate, find a node, read permissions, `checkChange`; and, with default bodies, `nameNodes`, ids for the nodes a revision adds, `pathHint`, a hint for a path that finds nothing, and `checkRevision`, a revision's size checked before it is applied, and `reviewLines`, what a change does that always needs a reviewer), `PermissionRule`, `Permissions` |
| `VersionControl.hpp` | `VersionControl` and `VersionObserver`; `RevisionResult`, `CommitInfo`, `ProposalInfo`, `Grant`, `Approval`, `Decision`, `Mode`; their protocol JSON |
| `ScreenImages.hpp` | `ScreenImages::render`: a picture of the screen or of a proposal (`ImageRequest`, `Image`) |
| `Network.hpp` | `network::Network` (`allow`, `check`, `start`, `connect`): what the `net` module offers to the others; `Request`, `Response`, `Failure`, `StreamEvent`, `Handlers`, `Transfer`, `WebSocket`, `SocketHandlers` |
| `Services.hpp` | `services::Services` (`load`, `method`, `invoke`, `subscribe`, `status`, `onStatus`, `onRequest`): what the `services` module offers to the others; `Call` (one call or subscription: `arguments`, `stopToken`, `progress`, `event`, `finish`, `fail`), `MethodSpec`, `Effect`, `RequestHandler` |
| `Log.hpp` | `LogLevel`, `log`, `setLogSink`: every console message of the library goes through one sink |
| `TextPattern.hpp` | `TextPattern` (`compile`, `find`, `expand`), `PatternOptions` (regex, whole word, case), `PatternMatch`, `PatternFind`: plain, whole-word or PCRE2 matching under limits (pattern 4 KiB, 1,000,000 steps, depth 10,000, 8 MiB heap per match); const matching is thread-safe |

## Invariants

- **Interfaces only.** A module may depend on `core`, and on another module only as `CMakeLists.txt` at the root
  shows (`format` uses `render`; `screen` uses all). Anything else that crosses a module boundary is an interface here.
- **`Signal` never allocates while it emits,** and a slot may connect or disconnect (itself too) during an emission:
  slots connected during an emission run from the next one, and a slot that disconnects itself is destroyed once the
  emission ends, never while it runs. Its slots are a vector: a signal with one slot keeps one small allocation.
- **`Channel` signals fire on the main thread,** whatever thread reads the transport.
- **Untrusted JSON goes through `parseUntrusted`:** nothing deeper than 256 levels is built, and numbers too large
  for a double are refused, so the recursive work done on a value can't overflow the stack. It takes time in
  proportion to the text: `json::parse` finds each key of an `ordered_json` object by looking at the members before
  it, so an object of 100,000 keys took 8.5 s; `detail::UntrustedBuilder` finds them in a hash table.
- **The log stays one line per message:** `log` escapes control characters, so text from peers can't forge a line
  or reach the terminal as an escape sequence.

## Threads

Everything here is used on the main thread, except two entry points:
- `Executor::post` may be called from any thread. It is how other threads hand work to the main thread.
- `log` may be called from any thread; the sink must cope with that. `setLogSink` is not synchronised: call it before
  the application starts.

## Errors

- `DocumentError` (a `std::runtime_error`) is the error of every document module. Its message names the place, e.g.
  `root.children[1].anchors: unknown edge 'lefft'`. The `expect::` readers throw it with that place.
- Interfaces that can refuse return the reason: `std::optional<std::string>` (empty when it worked), or a result
  struct with an `error` (`RevisionResult`, `Decision`).
- `log` is `noexcept`. If the application's sink throws, the message goes to the default sink instead.

## Extension points

- **A new transport:** derive from `Channel` (send a line; emit `onMessage` and `onClosed` on the main thread).
  `channel::Session` then speaks the live protocol over it.
- **Another version-control backend or document format:** implement `VersionControl` or `DocumentRules`.
  `format::Runtime` and `channel::Session` only see the interfaces.
- **The log:** `setLogSink` sends the library's messages to the application's log.
- **Registries:** those for node types, steps and document sections live in `format` (`modules/format/README.md`),
  not here.

## Files

```
include/affresco/core/
  Channel.hpp         the Channel interface and the message size limit
  DataModel.hpp       DataModel, ModelChange and ListModel: rows a document shows
  DocumentRules.hpp   what version control needs to know about a document format
  Executor.hpp        run work on the main thread
  CallProgress.hpp    a slow call's cancellation, progress and partial results
  Json.hpp            json, DocumentError, parseUntrusted, toText, expect::
  Log.hpp             levels, the sink, log()
  TextPattern.hpp     plain, whole-word and PCRE2 patterns, bounded
  ScreenImages.hpp    pictures of the screen for peers
  Signal.hpp          Signal and Connection
  VersionControl.hpp  VersionControl, VersionObserver and their value types
src/TextPattern.cpp   TextPattern over PCRE2 (built into affrescoui::regex)
CMakeLists.txt        the affrescoui_core INTERFACE target
```

## Recipes

**Read a value from a document, with an error that names the place.**

```cpp
const json& window = affresco::expect::object(document["window"], "window");
const float width = affresco::expect::number(window["width"], "window.width"); // DocumentError if it isn't one
```

**Parse what a peer sent.** Use `parseUntrusted(text)`, never `json::parse`: it caps the nesting and the numbers.
Catch `json::parse_error` and `DocumentError`, and answer with the message.

**Add an interface between two modules.** This changes module boundaries, so ask the maintainer first. Then: a
header here with an abstract class (a virtual destructor, Doxygen on every member, and the thread it runs on), the
implementation in one module, and its user in another, wired together in `modules/screen`.
