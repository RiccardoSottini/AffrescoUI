# RFC 0002: Services and data models

- **Issue:** none yet (written for roadmap item 8.5, wave 3; decides D2)
- **Author:** the `3-rfcs` fleet agent (Claude), for the maintainer
- **Changes:** the screen format; the security model; the modules (a new `services` module, two interfaces in
  `core`)
- **Prototype:** [`prototypes/0002-services/`](prototypes/0002-services/README.md): a JSON-RPC service with LSP's
  framing and a reference client whose 18 checks pin down requests, streams, progress, cancellation, timeouts,
  requests from the service and framing limits; the proposed syntax; the edits and reads measured below

## The problem

Large apps do heavy work that doesn't belong in a document: searching 100,000 files, reading and writing them,
watching them change, talking to a language server or a debug adapter, running git, driving a terminal, filtering an
image, querying a database. Documents must stay declarative (no scripts inside documents: decision 3), so that work
has to live in native code or in other processes, and the document has to be able to **declare it, call it, show its
progress, stream its results and cancel it**, without turning into code and without widening what an agent can do.

What exists today, and why it isn't enough:

- **`call`** runs a native function the host registered (`Screen::onCall`). It is synchronous on the main thread (a
  slow call freezes the frame), returns one value (no streaming, no progress), can't be cancelled or time out, and is
  a control step: no revision may add one, because a native function runs "with the host's full rights" and has no
  contract the checker or a reviewer can read. So an agent can't build anything that searches, reads a file or asks
  a language server.
- **`channels`** connect to local sockets with one JSON object per line. Each message runs an action, but nothing
  matches an answer to its request, there is no framing for LSP or DAP (`Content-Length` headers), and starting a
  program is host code only (`ProcessChannel` takes a shell command).
- **State** is JSON inside the document. A file tree of a million entries or a search with 50,000 hits copied into
  state makes every read, update and diff grow with the data: 10,000 file entries in the state are about 208,000
  tokens for an agent's first read (measured below).

**Who needs it:** an IDE (files, search, git, language servers, terminals), an image editor (filters, tiles, a
colour engine), a music tool (an audio engine), a dashboard (a database), any app whose data is larger than a
screen.

**Out of scope here:** the individual providers' contracts beyond the general ones listed (8.5a, b, c and the app
layer define them); the network (RFC 0003); packaging services with features (RFC 0004); the WASM sandbox (open
question 1, a later RFC).

## The design

Four parts:

| Part | What it is |
|---|---|
| **Providers** | C++ classes the host registers in a `ServiceRegistry`, each offering a kind of service (`fs`, `process`, `jsonrpc`, `search`, an app's own `git`). Each declares its configuration and its methods, with each method's **effect** |
| **The `services` section** | The services this screen uses: a name, a provider and its configuration. A **capability**: the host grants it, and no revision may change it |
| **Steps** | `invoke` (call a method; results, progress and streamed events go into state), `subscribe` (a stream of events), `cancel`, and `respond` (answer a request from a service) |
| **Data models** | Large data that stays native (`models.files.tree`): views read the rows they show, agents see the shape and a sample |

### Providers and the registry (C++)

A **provider** is a kind of service. The host registers the providers it trusts and says what each may touch:

```cpp
affresco::Screen screen = affresco::Screen::fromFile("ide.screen.json");
affresco::services::ServiceRegistry& services = screen.services();
services.add(std::make_unique<affresco::services::FileProvider>());      // "fs"
services.add(std::make_unique<affresco::services::JsonRpcProvider>());   // "jsonrpc"
services.add(std::make_unique<MySearchProvider>());                      // "search", the application's own
services.addRoot("workspace", chosenFolder);                              // what "root": "workspace" means
services.allowProcess("clangd", "/usr/local/bin/clangd", {"--background-index"});
screen.createWindow(app);                                                 // starts nothing yet: services start on first use
```

The interfaces, in a new `services` module (with two small interfaces in `core`, below):

```cpp
namespace affresco::services {

/// What a method does to the world outside the document: the capability boundary reads it.
enum class Effect { read, write, run };

/// One method of a provider's contract.
struct MethodSpec {
	std::string name;          ///< "find", "read", "textDocument/hover".
	Effect effect;             ///< read: no lasting change; write: changes files or data; run: starts or controls programs.
	bool revisable = true;     ///< false: only the screen file may invoke it, never a revision (like a control step).
	bool streams = false;      ///< It sends events before its result (search hits, progress, terminal output).
	json arguments;            ///< A JSON Schema of its arguments: checked before the call, and read by agents.
	std::string about;         ///< One sentence for the catalog.
};

/// One call in progress. The service may finish it from any thread; the runtime delivers on the main thread.
class Call {
public:
	[[nodiscard]] const json& arguments() const noexcept;
	[[nodiscard]] std::stop_token stopToken() const noexcept; ///< Requested on cancel, timeout or teardown.
	void progress(json value);   ///< { "done", "total", "message" }: coalesced to one update per frame.
	void event(json value);      ///< A streamed item (only for methods that stream).
	void finish(json result);    ///< Once. Ignored after a cancellation.
	void fail(std::string message, json data = nullptr);
};

/// One service a screen declared. Created and destroyed on the main thread; does its work wherever it likes.
class Service {
public:
	virtual ~Service() = default; ///< Cancels every call and subscription, and stops its processes.
	virtual void invoke(std::string_view method, std::shared_ptr<Call> call) = 0;
	virtual std::unique_ptr<Subscription> subscribe(std::string_view stream, const json& arguments, EventSink sink);
	[[nodiscard]] virtual std::vector<std::string> models() const { return {}; }
	[[nodiscard]] virtual DataModel* model(std::string_view name) { return nullptr; }
};

/// A kind of service: its configuration, its contract, and a factory.
class Provider {
public:
	virtual ~Provider() = default;
	[[nodiscard]] virtual std::string_view name() const = 0;
	[[nodiscard]] virtual json configurationSchema() const = 0;
	[[nodiscard]] virtual std::vector<MethodSpec> methods(const json& configuration) const = 0;
	[[nodiscard]] virtual std::unique_ptr<Service> create(const json& configuration, const Grant& grant) = 0;
};

class ServiceRegistry { /* add, find, addRoot, allowProcess, catalog */ };

} // namespace affresco::services
```

- **Registration, as for types and steps.** Built-in providers register from their own files; an application's
  register through the same `add`. The catalog lists every provider, its configuration and its methods, so an
  agent reads the contract instead of guessing.
- **`Grant`** is what the host allowed this service: the roots it may use, the processes it may start (by name,
  resolved to an absolute path by the host), limits. A provider can't reach beyond its grant, and the document can
  only name things the grant contains: `"root": "workspace"` names a root, never a path.
- **The general providers** (8.5a, b, c): `fs` (read, write, list, stat inside a root; its directory tree as a
  model), `watch` (file changes, debounced; 8.5b), `process` (an allowlisted program with streamed stdio), `pty`
  (8.5c), `jsonrpc` and `dap` (below). Search, git, a database or an image engine are applications' providers, or
  later general ones when two unrelated apps want the same contract.

### The `services` section

```json
"services": {
	"files": { "provider": "fs", "root": "workspace" },
	"search": { "provider": "search", "root": "workspace" },
	"cpp": {
		"provider": "jsonrpc",
		"process": "clangd",
		"framing": "content-length",
		"methods": { "textDocument/hover": "read", "textDocument/definition": "read", "workspace/executeCommand": "write" },
		"restart": { "after": 2000, "times": 3 }
	}
}
```

| Key | Meaning |
|---|---|
| service name | A word (`files`, `cpp`), used in steps as the first part of `"cpp.textDocument/hover"` |
| `provider` | A registered provider; an unknown one is an error that lists the registered ones |
| `restart` | After the service stops unexpectedly: try again `after` ms, at most `times` times. Default: no restart |
| `start` | `"onDemand"` (the default: at the first `invoke` or `subscribe`) or `"onLoad"` |
| other keys | The provider's configuration, checked against its `configurationSchema` (`root`, `process`, `framing`, `methods`, ...) |

- **A capability.** Like `channels` and `persist`, the section is registered with `capability = true`
  (`CapabilitySections.cpp`), so `checkChange` refuses any revision that adds, removes or changes a service.
- **Configuration is literal.** No bindings (`{{ }}`) in a service's configuration: otherwise a revision could change
  the state a binding reads and so widen the service (point `root` somewhere else). Things that vary are arguments of
  methods, checked by the method's schema and bounded by the grant.
- **Granted at load.** When the screen loads, each declaration is checked against the registry and the grant: an
  unknown provider, a root the host didn't name, a process it didn't allow, or a configuration the schema refuses is
  an error naming the place (`services.cpp.process: 'clangd' isn't a program the host allows (it allows:
  rust-analyzer)`). A service that fails isn't started; the rest of the screen loads, and `services.cpp.state` is
  `"refused"`.
- **`services.<name>`** is a variable: `{ "state": "stopped" | "starting" | "ready" | "failed" | "refused",
  "error", "restarts" }`, so a status bar can show `clangd: {{services.cpp.state}}`.

### `invoke`: call a method

```json
{ "invoke": "search.find", "with": { "query": "{{state.query}}", "include": "**/*.cpp" },
  "id": "find", "append": "state.matches", "keep": 2000,
  "into": "state.summary", "loading": "state.finding", "progress": "state.findProgress", "timeout": 60000,
  "onDone": [{ "focus": "#results" }], "onError": [{ "set": "state.findError", "to": "{{error.message}}" }] }
```

| Key | Meaning |
|---|---|
| `invoke` | `"service.method"`, split at the first dot (`"cpp.textDocument/hover"`). A literal: never computed |
| `with` | The arguments, evaluated, then checked against the method's schema before anything is sent |
| `into` | A state path for the result |
| `append`, `keep` | For a method that streams: each event is pushed onto that list, which keeps at most `keep` items (default 1,000, at most 10,000), dropping the oldest |
| `onEvent` | For a method that streams: steps run per event, with `event`. Events that arrive between two frames are run in one batch, so a fast stream costs one update per frame |
| `loading` | A state path set to `true` while the call runs, `false` when it ends however it ends |
| `progress` | A state path for `{ "done", "total", "message" }`, or `null` when the call ends |
| `timeout` | Milliseconds (default 30,000; 1 to 3,600,000). Then the call is cancelled and `onError` runs with `error.code` `"timeout"` |
| `id` | Names the call, for `cancel`. **Starting a call with an `id` that is still running cancels the older one**, so search-as-you-type needs no extra step |
| `onDone`, `onError` | Steps when it ends, with `result` or `error` (`{ "code", "message", "data" }`). Without `onError`, the error is reported like any step's |

**Async, always.** `invoke` never blocks: the step starts the call and the action goes on to its next step. The
result, events and progress arrive later on the main thread, each as its own batch of state changes and one update.
Code that must run after the result goes in `onDone`.

### `subscribe` and `cancel`

```json
"onLoad": [{ "subscribe": "cpp.textDocument/publishDiagnostics", "id": "diagnostics",
             "do": { "set": "state.diagnostics[event.uri]", "to": "{{event.diagnostics}}" } }]
```

- **`subscribe`** listens to a stream a service offers (a notification method for `jsonrpc`; `changes` for `watch`;
  `output` for `pty`): `do` runs per event (batched per frame as for `onEvent`), or `append`/`keep` collect them,
  or `into` keeps the latest. `with` passes arguments when the stream takes some.
- **Lifetime.** A subscription started from a node's handler (`onShow`, `onClick`, ...) belongs to that node and ends
  when the node is removed, like `every`. One started from the document (`onLoad`, `keys`, `watch`, an action run by a
  channel) lasts until it is cancelled or the screen closes. The same rule cancels a node's running `invoke`s when
  the node goes away.
- **`cancel`**: `{ "cancel": "find" }` ends the call or subscription with that `id`. A cancelled call runs `onError`
  with `error.code` `"cancelled"`, sets `loading` to false, and drops anything that arrives for it afterwards.

### `respond`: answering a service

A JSON-RPC service can ask the client things (LSP's `workspace/configuration`, `window/showMessageRequest`). Such a
request runs the action `"<service>:<method>"`, as a channel's messages run `"<channel>:<type>"`, with the request's
parameters as `event`. A `respond` step in it answers: `{ "respond": [{ "tabSize": 4 }] }`, or
`{ "respond": null, "error": "not supported" }`. A request with no action, or whose action doesn't respond within
the timeout, gets JSON-RPC's MethodNotFound or an error.

### JSON-RPC over processes and channels: LSP and DAP

Two providers share one framing layer:

| | `jsonrpc` | `dap` |
|---|---|---|
| Messages | JSON-RPC 2.0: requests with ids, responses, notifications, requests from the service | The Debug Adapter Protocol: `request`, `response` and `event` with `seq` and `request_seq` |
| Framing | `"content-length"` (LSP and DAP: `Content-Length: N\r\n\r\n{...}`) or `"lines"` (one object per line, like today's channels) | `"content-length"` |
| Transport | `process` (an allowlisted program's stdin and stdout) or `socket` (a local socket in the socket folder, like a channel) | the same |
| `invoke` maps to | a request; `$/cancelRequest` on cancel or timeout; `$/progress` with the call's token feeds `progress`, and LSP partial results feed `onEvent`/`append` | a request; DAP's `cancel` request |
| `subscribe` maps to | a notification method | an event name |

- **The transport is bytes, not lines.** `core::Channel` is line-based, so the framing layer sits under it: the
  process and socket transports gain a byte-stream mode (`ByteChannel`) and the framing turns bytes into messages
  on the reader thread. A process is started from an argument vector without a shell (`ProcessChannel` takes a shell
  command today, which is fine for host code but not for a document's declaration).
- **Limits** (from the prototype's reference client): headers at most 8 KiB; a message at most 8 MiB (results bigger
  than that belong in a model); at most 256 calls waiting per service; a malformed frame (no or bad
  `Content-Length`, a short body, a body that isn't a JSON object) closes the service with the reason, and `restart`
  decides what follows.
- **Effects of a generic server's methods** can't be known by a generic provider, so the declaration lists them:
  `"methods": { "textDocument/hover": "read", ... }`. A method the declaration doesn't list may still be invoked by
  the screen file, but never by a revision (it is treated as not `revisable`).

### Host data models

Large data stays native. A provider (or the host directly, `screen.addModel("files", model)`) offers a **data
model**, a `core` interface so `format` can read it without depending on `services`:

```cpp
namespace affresco {
/// Rows of data that stay in native memory; read on the main thread, a row at a time.
class DataModel {
public:
	virtual ~DataModel() = default;
	[[nodiscard]] virtual std::size_t count() const = 0;           ///< Cheap: O(1).
	[[nodiscard]] virtual json row(std::size_t index) const = 0;   ///< Cheap: O(1) or O(log n); only visible rows are read.
	[[nodiscard]] virtual json shape() const = 0;                   ///< A JSON Schema of a row, for the checker and agents.
	Signal<std::size_t, std::size_t, std::size_t> onChanged;       ///< (first, removed, inserted), on the main thread.
};
}
```

- **Documents read models like lists:** `"repeat": { "for": "entry", "in": "models.files.tree", "key":
  "{{entry.path}}" }`, `{{models.search.results.count}}`, `{{models.files.tree[0].name}}`. With virtualised lists
  (4.2), a `repeat` over a model builds only the visible rows; without them, a `repeat` over a model of more than
  10,000 rows is an error naming the place, as `forEach` is limited today.
- **Models are read-only to documents.** Changing data is a method of the service (`files.write`), whose model then
  reports the change through `onChanged`, and the views that show the changed range update.
- **List functions over a model** (`filter`, `sort`, `map`) are refused above 10,000 rows: filtering a million rows
  is the provider's work (an `invoke` that publishes a new model, `models.search.results`).
- **Agents see a summary,** never the rows: `get_screen` shows `"models": { "files.tree": { "count": 1204331,
  "shape": {...}, "sample": [3 rows] } }`, about 93 tokens, where 10,000 rows of state are about 208,000.
- **Snapshots for readers.** A provider that fills a model from another thread publishes immutable chunks and posts
  the change; the main thread reads the last published version, so `row(i)` never takes a lock held by a worker.

### Threads

| Thread | Does | Never |
|---|---|---|
| **Main** (the Executor) | Documents, steps, expressions; `invoke` validation; delivering results, events, progress and model changes; reading models; creating and destroying services | Wait on a service |
| **A service's own threads**, or a shared worker pool owned by the registry | The provider's work: file I/O, searching, a process's reader thread, the framing | Touch the document, the state or a panel |

- Everything crosses to the main thread through `Executor::post`, and lands as one batch of state changes per frame.
- Each `Call` is shared between the runtime and the service; when it is cancelled (by `cancel`, a newer call with the
  same `id`, a timeout, its node going away, or the screen closing), its `stop_token` is requested, the service is
  told (`$/cancelRequest` for JSON-RPC), and anything it delivers afterwards is dropped.
- Destroying the screen destroys its services, which cancel their calls, end their processes (a process group on
  POSIX, a job object on Windows, as `ProcessChannel` does today) and join their threads. Nothing posted after that
  runs: deliveries check a liveness guard, as channels do today.
- No exceptions cross threads: a provider reports failure with `Call::fail`.

### `call` (4.7)

`call` stays what it is (a small native function, a control step), and 4.7 gives it what `invoke` has (`loading`,
`timeout`, and a worker pool for functions the host registers as asynchronous) so that a slow native function never
blocks a frame either. New native work with a contract should be a provider: it gets a catalog entry, effects the
capability boundary can read, cancellation and streams.

### What a revision may and may not do

| A revision may | A revision may not |
|---|---|
| add, change or remove `invoke`, `subscribe` and `cancel` steps naming a declared service and a `revisable` method | add, remove or change anything in `services` (a capability) |
| change the `with` arguments, the state paths results go to, timeouts and handlers | invoke a method that isn't `revisable`, or one a `jsonrpc` declaration doesn't list |
| read `models.*` and `services.*` anywhere | give `invoke` a computed method name |
| | add or change a `respond` step (it speaks for the app to a program), nor a way to run one |

**Effects decide review.** A revision that adds or changes an `invoke` of a `write` or `run` method always goes to
review, whatever the `allow` rules say (it is judged as if an `ask` rule matched), and the reviewer sees a capability
line next to the change: `uses files.write (writes files in 'workspace')`. A `read` method follows the screen's
permission rules like any other change. While a proposal is previewed, `write` and `run` invokes are refused with
`error.code` `"preview"`, so previews have no side effects; `read` invokes run.

This is the line RFC 0003 draws for connections too: **a revision may use what the host granted and the screen file
declared, never declare more.** It is a deliberate change from `call`, which no revision may add (open question 2).

### What it looks like for an agent

Adding a "Find" button that streams results, in one edit (61 tokens):

```json
{ "type": "revision", "message": "A button that searches the workspace",
  "add": { "#searchPanel/children/-": { "type": "label", "text": "Find",
    "onClick": { "invoke": "search.find", "with": { "query": "{{state.query}}" }, "id": "find",
                 "append": "state.matches", "loading": "state.finding" } } } }
```

The reviewer sees `add #searchPanel/children/- ...` with `uses search.find (read)`. Adding a new service is refused
before any reviewer sees it: `revision.set[0] (set /services/shell): 'services' is a capability: a revision can't
change it`.

## The alternatives

**D2: how heavy app logic plugs in.**

| Option | For | Against |
|---|---|---|
| **A. Native providers plus out-of-process JSON-RPC services** (recommended) | Native speed where it matters; any language for services (most language servers already speak LSP); a crashing service doesn't take the app down; contracts with effects the capability boundary can read; no new runtime dependency | A service runs with the user's rights (no sandbox by default); native providers are per-platform code |
| B. A WASM sandbox for feature logic (wasmtime, WAMR or wasm3) | Portable, sandboxed by construction (memory, no I/O without imports), deterministic; feature packages could carry logic across apps | A large dependency (wasmtime is several MB; WAMR and wasm3 are smaller but slower); a second programming model; tooling for authors; easy to drift into "scripts in documents" by another name |
| C. An embedded script engine (Lua, QuickJS) | Small, quick to write logic | Contradicts decision 3 (no scripts inside documents); scripts become code paths the reviewer must read; sandboxing an interpreter is a long tail |
| D. `call` and `channels` only (do nothing) | No new concepts | Synchronous calls freeze frames; no streaming, cancellation or progress; no LSP/DAP framing; agents can't use native work at all |

**Recommended: A now, and keep the service contract transport-neutral** (methods, effects, schemas, streams), so B
can arrive later as one more provider (`"provider": "wasm"`, a signed module from a feature package) without changing
the format. B is a later RFC when a portable feature needs logic that no general provider offers.

**Other choices inside A:**
- *Services as channels with request ids* (extend `channels` instead of a new section): channels are untyped message
  pipes whose every message runs an action; they have no contract, no effects and no framing for LSP. Keeping them
  separate keeps channels simple (peers, the Studio) and services typed.
- *Effects declared per service rather than per method:* simpler, but "clangd" would be all-`write` because one
  method is, and agents couldn't add a hover.
- *Results always into state, no models:* simple, but it is what makes a million-row tree impossible.

## The cost in tokens

Measured with `python3 rfcs/prototypes/tokens.py rfcs/prototypes/0002-services` and `model_read.py` (tiktoken
`o200k_base`, minified, notes left out):

| Agent edit or read | Today | With this RFC |
|---|---|---|
| A button that searches the workspace and streams results | 52 tokens, refused (`call` is a control step) | 61, allowed (reviewed if the method writes) |
| The same button running an existing command (RFC 0001) | refused | 34 |
| A hover from the language server | not expressible | 56 |
| The problem count in a status bar | not expressible (no diagnostics) | 33 |
| Adding a service | not expressible | 18, refused with a message |
| Reading a list of 10,000 files | about 208,000 (in the state) | about 93 for a model of 1,204,331 (count, shape, 3 samples) |

## Performance

- **Nothing on the hot path until used.** A screen without `services` pays nothing; services start on demand.
- **No frame waits on a service.** `invoke` posts and returns; 8.5a measures the main thread's time per invoke
  (target: under 20 µs to validate and send) and that a 2-second call never delays a frame (4.7's test).
- **Streams cost one update per frame**, however fast they arrive: events between frames are batched, `keep` caps the
  list, and progress is coalesced to its latest value.
- **Models cost what is visible:** with 4.2, a `repeat` over a model reads only the visible rows (about 50 `row()`
  calls a frame while scrolling, none while idle), and a change updates only the views showing the changed range.
- **Memory:** a model's rows stay in the provider's own structures; the document holds none of them.
- **What 8.5a must measure:** invoke-to-result round trip through the `jsonrpc` provider against the prototype's
  service (target under 1 ms on a local process for a small result); 10,000 streamed events per second staying at
  one update per frame; the `update/400` and `load` benchmarks unchanged for screens without services.

## Security review

- **New input, from documents:** the `services` section (trusted: only the screen file has it), `invoke` and
  `subscribe` steps (untrusted: revisions add them). Arguments are checked against the method's schema before they
  are sent; `keep` is capped at 10,000; at most 256 calls waiting per service and 256 subscriptions per screen.
- **New input, from services:** every message is untrusted (a language server is someone else's code, and can be
  wrong or hostile): framing limits (8 KiB of headers, 8 MiB a message), `parseUntrusted` (nesting 256), per-frame
  batching so a flood costs one update a frame, a cap on queued deliveries per service (a service that floods past it
  is stopped, and `restart` decides). New fuzz targets: `content-length` framing, JSON-RPC dispatch, DAP dispatch.
- **New capabilities: processes, files and native work.** The host grants them in C++ (`addRoot`, `allowProcess`,
  registering a provider). The screen file declares which it uses (`services`, a capability section), and no revision
  may change that: `checkChange` already guards capability sections, and 8.5a adds unit tests for `services`.
- **Processes:** started only from the host's allowlist, by name, resolved to an absolute path by the host; from an
  argument vector, never a shell; with the arguments the host fixed (a document may add arguments only where the host
  allows a pattern); in a working directory that is a granted root; with an environment the host chooses (not the
  app's whole environment); ended with everything they started when the service ends.
- **Sandboxing:** out-of-process services run as the user, like peers today: version control stops unapproved changes
  to the app, not a hostile program. An OS sandbox (App Sandbox or `sandbox-exec` profiles on macOS, Landlock or
  seccomp on Linux, AppContainer on Windows) can be added by the host per process later; a document can never turn
  it off. `SECURITY.md` must say so plainly.
- **What a revision can do:** use declared, `revisable` methods, which the reviewer sees with their effect; `write`
  and `run` methods are always reviewed, and refused during previews. This widens what a revision can *use* compared
  with `call` (open question 2), never what the app can *reach*. A remote peer allowed to change the screen could read
  files inside a granted root through a new `read` invoke; a host that serves remote peers should mark such methods
  not `revisable`, or deny agents the nodes and actions that hold invokes.
- **Self-approval:** unchanged. `respond` is a control step, so a revision can't answer a service on the app's behalf.
- **Secrets:** a service's configuration never holds a secret (it is in the document, visible to every peer).
  Providers that need tokens get them from the `SecretStore` of RFC 0003, by reference.
- **Callbacks and timers:** every call and subscription is owned by the runtime (and by its node, when started from
  one) and cancelled when its owner goes away; timeouts are scheduler timers cancelled with the call; every delivery
  checks a liveness guard before touching the runtime.

## Compatibility

- **Additive within format version 1:** a new section, four new steps, two new variables (`services`, `models`).
  Existing screens are unchanged. `services`, `models`, `result` (in `onDone`) and `error` become reserved names.
- **History** stays RFC 6902; steps are ordinary document values. Change sets that use services apply only to a
  screen that declares those services (the checker reports an unknown service).
- **The schema, `SCREEN_FORMAT.md` and the catalog** change together; the catalog also lists each registered provider's
  configuration and methods.
- **Modules:** a new `services` module (the registry, the general providers, the framing) that uses `core` and
  `channel`; `core` gains `DataModel.hpp` and `Services.hpp` (the interface `format` calls to invoke, subscribe and
  cancel, implemented by `services` and wired by `screen`), so `format` doesn't depend on `services`, as it doesn't
  on `vcs`. `channel` gains a byte-stream mode and argument-vector process start. Public API: additions only
  (`Screen::services()`, `Screen::addModel`).

## What it costs

| | |
|---|---|
| **Tokens** | Using native work costs 33 to 61 tokens an edit, where today it can't be done by a revision at all. Large data stops costing tokens: a model is summarised in about 93 |
| **Speed** | Nothing until used; no frame ever waits on a service; streams and progress cost one update per frame; models cost what is visible |
| **Security** | A new capability section (processes, files, native work), granted in C++, never changed by a revision. Revisions may *use* declared methods: `read` under the permission rules, `write` and `run` always reviewed and refused during previews. Two new fuzz targets |
| **`SCREEN_FORMAT.md`** | A "Services" section (the section, the four steps, `services.*`, `models.*`), rows in the steps table, the control-step list gains `respond`, and the capability table in `ARCHITECTURE.md` and the threat model in `SECURITY.md` gain services |

## Open questions

1. **D2.** *Recommendation:* native providers plus out-of-process JSON-RPC services now (option A); a WASM provider
   later, in its own RFC, when a portable feature needs logic no general provider offers.
2. **May a revision add an `invoke` at all?** Today no revision may add a `call`. This RFC lets revisions *use*
   declared, `revisable` methods, with `write` and `run` always reviewed. The alternative is to make `invoke` a
   control step like `call`, which keeps today's line but means agents can never build a feature that searches,
   reads a file or asks a language server. *Recommendation:* allow it as designed. It is the line RFC 0003 draws for
   connections ("use declared operations, never add them"), the effects make the risk visible to the reviewer, and
   the host can still mark any method not `revisable`.
3. **A new `services` module, and two interfaces in `core`?** *Recommendation:* yes. It keeps `format` free of
   threads and processes, and mirrors how `vcs` sits behind `core::VersionControl`.
4. **Default `keep` and `timeout`.** *Recommendation:* 1,000 events and 30 seconds; both set per step.
5. **An OS sandbox for service processes.** *Recommendation:* not in 8.5a; a host option later (per process), with
   `SECURITY.md` saying plainly that services run with the user's rights until then.
6. **Who declares a generic server's method effects?** *Recommendation:* the screen file (`methods` in the
   declaration), because the `services` section is trusted and a generic provider can't know them; undeclared methods
   stay usable by the screen file only.
7. **DAP as its own provider, or a mode of `jsonrpc`?** *Recommendation:* its own provider on the shared framing
   (the message shapes differ), built when the debugger is (it is "later" in the plan).
