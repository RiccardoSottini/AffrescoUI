# format: the screen format

`format` gives a screen document (`SCREEN_FORMAT.md`) its meaning. `Rules` tells version control what a valid
document is, where a node is, who may change what, and which changes are forbidden for everyone; `Checker` finds what
a document refers to but doesn't define, without running it; `Runtime` builds the document into `render` panels,
keeps them in line with every new version and state, and runs events, actions and steps. Expressions (`{{...}}`) are
parsed into JSON operator trees (what the checker reads), lowered once into typed nodes, and run by a small machine
on `Value`s, which read the state in place: JSON appears only at the edges. The format is general: it describes any
application's UI, and what one application needs that the format can't express comes in through an extension point,
not a feature.
Target: `affrescoui::format` (uses `core`, `render` and nlohmann/json).

## Public headers (`include/affresco/format/`)

| Header | Offers |
|---|---|
| `Rules.hpp` | `Rules`, the format's `DocumentRules`: `validate`, `findNode` (`#id`), `permissions`, `checkChange`, `nameNodes` (ids for the nodes a revision adds), `pathHint` (hints for paths that find nothing), `reviewLines` (the service steps a change adds, which always need a reviewer) and `behaviourLines` (what a change does to behaviour, for review policies and evidence); `findings` (the checker's findings as text); `addNativeAction` |
| `NodeIds.hpp` | Node ids (D15): `nodeTrees`, `forEachNode`, `findNodeWithId`, `nodeIds`, `duplicateIds`, `isNodePlace`, `placeOf`, and `NodeNamer`, which gives readable ids (`label-blocked`) to nodes that have none |
| `Runtime.hpp` | `Runtime`: `registerType`, `createWindow` and `build`, `instantiate`, `onAction`, `onCall`, `onAsyncCall`, `setExecutor`, `setServices`, `setNetwork`, `setIdentity`, `setModel`, `runAction`, `setHandlersSuspended`, `catalog`, `find`, `color`, `resolveColor`, `font`; images: `setImage`, `allowImageDirectory`, `waitForImages`; the signals `onEvent`, `onError`, `onSend`, `onConnect`, `onUpdated`; for the inspector, `inspect` (a panel's node, bindings as written and handlers, never a value) and `lastUpdate` (`UpdateStatistics`) |
| `Usage.hpp` | `Usage` (`Runtime::usage`): usage summaries, off by default: `setEnabled` with the person's `giveConsent`, `ran`, `visited`, `summary`, `clear`; only ids, counts and times, bounded |
| `Locale.hpp` | `systemLocale()`: the locale the computer's user chose, for `Runtime::setLocale` |
| `Features.hpp` | `Features` (RFC 0004): the publishers the host trusts (`trustPublisher`), its signed catalogues (`addCatalog`), what they offer (`catalog`), and the revisions that install and upgrade a feature (`installRevision`, `upgradeRevision`) |
| `Adapters.hpp` | Other formats (SCREEN_FORMAT.md): `detect`, `fromA2ui` (A2UI 0.8), `fromAdaptiveCard` (Adaptive Cards 1.6) and `convertForeign`, pure functions that give a screen document, its warnings and A2UI's later updates as revisions (`src/adapters/`) |
| `SyntaxHighlighter.hpp` | `SyntaxHighlighter`: a `TextEditor`'s `EditorHighlighter` on the services' syntax engine: follows the model's edits incrementally, colours a block of lines at a time through a palette (style name to colour, a dotted style falling back to its parent), and gives the editor folds and bracket pairs. The `editor` type's `grammar` key installs one, coloured by `@syntax.*` tokens |

Private headers that tests and benchmarks may include: `src/expression/Expression.hpp` (compile and evaluate),
`src/expression/Value.hpp` (the values expressions compute with), `src/functions/ExpressionFunctions.hpp` (the
function registry), `src/checker/Checker.hpp` (`check`, `introduced`, `Finding`), and the registries
(`src/types/TypeRegistry.hpp`, `src/steps/StepRegistry.hpp`, `src/sections/SectionRegistry.hpp`). Each source
folder is on the target's include path, so a source includes another folder's header by its name alone.

## The runtime's parts

`Runtime::Impl` is `Engine` (`src/runtime/Engine.hpp`), a composition root that owns the registries and eight
subsystems.
Each subsystem's header says its role, its invariants and its callers:

| Subsystem | Does |
|---|---|
| `DocumentContext` | colours, fonts (from a bounded `FontCache`), images (an `ImageCache`: files of the allowed folders, the host's named images), the theme, the globals of expressions, the `computed` cache, the screen an outlet shows |
| `Builder` | compiles each node once per version of the document; a node's children after `use`, `repeat`, `with`, `let`, `check`, styles and `when` |
| `Reconciler` | the record of every panel built; keeps the panels in line with the document; batched updates |
| `Properties` | a compiled node's values on its panel, by slot; the property sets node types registered |
| `Events` | pointer, keys (one walk resolves bindings and chords, `resolve`), focus (tab order, focus scopes, arrows, mnemonics), drag and drop, bubbling, overlays closed by a click outside, widgets' values |
| `Steps` | handlers and actions; each step runs in the group of the step registry that recognises it |
| `Scheduler` | `every`, `after`, `debounce`, `animate`, transitions and deferred tasks, on the window's clock |
| catalog | `Catalog.cpp`: what the format offers, built from the registries |

| Registry | Registers | From |
|---|---|---|
| `TypeRegistry` | a node type: its factory; its properties (name, kind of value, whether states and `transition` may set them); its apply function; a widget's value hooks, its own events (`WidgetEvents`: signals run as handlers, such as `onClose`) and its part in drag and drop (`DragHooks`) | `Types.cpp` (the core types), `LayoutProperties.cpp` (the sets of rows, columns, stacks and grids), `EditorType.cpp`, `ToggleType.cpp`, `SliderType.cpp`, `ImageType.cpp`, `SplitType.cpp`, `TabsType.cpp`, `ListType.cpp`, `PathType.cpp`, `ColorPickerType.cpp`, `GradientType.cpp`, `CurvesType.cpp`; properties other types take (`addProperties`): `IconProperty.cpp`, `RunsProperty.cpp`; the application's `registerType` |
| `StepRegistry` | a group of steps: each step's name, keys, and whether it is a control step; the group's runner | `steps/`: `StateSteps.cpp`, `FlowSteps.cpp`, `OutsideSteps.cpp`, `ServiceSteps.cpp`, `ReviewSteps.cpp`, `NavigationSteps.cpp`, `TimeSteps.cpp`, `OtherSteps.cpp`; `AuthSteps.cpp` |
| `SectionRegistry` | a top-level section: its validation, whether it is required, and whether it is a capability | `Sections.cpp`, `CapabilitySections.cpp` |

The type registry belongs to each runtime, since applications add to it; the built-in steps and sections never
change, so one read-only registry of each is shared.

## Compiled nodes

Each node of a version of the document is compiled once (`CompiledNode.hpp`, 7.6) and shared by every update and
every panel built from it:
- **Resolved:** its component instantiated and its static styles merged. A node with a dynamic `style` is compiled
  once for each style it chooses.
- **Slots:** each property is a `Slot` (`color`, `Layout`, `Font`, a handler, a state block, ...), so applying it
  dispatches on the slot instead of comparing text.
- **Bindings:** each value is a `Binding` (`Binding.hpp`): templates split and operators lowered once. A value that
  can't change is used as written; the others are evaluated by each update into the record's `values`. `with`,
  `let`, `when`, `repeat` and a dynamic `style` are bindings too.
- **Children** are not copied: they stay in the document, or in the component instance the compiled node keeps.

The `Builder` keeps the compiled nodes of the current version and drops them when the document changes. An update
evaluates each node's dynamic values; when they, the compiled node, the theme and the colours (`DocumentContext::
appearance()`) are what was applied last, `Properties` only shows the widget's value again. A node whose application
reads more than its values (an overlay's `open` written as a state path, a computed `bind` or `caret`, a computed
`every` period) is applied on every update. A node that failed to apply is applied again at the next update, so its
error comes again. A consequence for applications: a change code makes to a built panel directly stays until its
node is applied again (its values, the theme, the colours, the fonts or the document changed); before, every update
put the document's values back. `Runtime::registerType` counts as a change of appearance, so panels of a type
registered again apply its new function.

## Typed state

`stateTypes` (`StateTypes.hpp`, 4.4b) declares a JSON Schema per state key; the subset the library reads is checked by
the section's validator. `Rules::validate` refuses a document whose own `state` doesn't match; `checkChange` refuses a
revision that brings a mismatch into the live state (mismatches already there don't block it); `findings` warns of a
state kept on disk that no longer matches. The checker (`CheckerTypes.cpp`) learns the declared keys and properties
as readable, and reports writes, `bind`s and enum comparisons that don't fit. Nothing stops a step at run time: the
types are a contract for revisions and a description for agents (the MCP outline shows them). The schema code is
general: a settings page can describe its settings with it.

## Design tokens

`tokens` (`DesignTokens.hpp`) generalises `colors`: numbers (spacing, radii, type sizes) and colours, aliases
(`"@other"`, of a token or of `colors`) and per-theme values. `readTokens` resolves them once per version of the
document, merges the colour tokens into the colour table (so `@name` works the same for both) and keeps the number
tokens apart. `Properties::applyVisual` replaces a property value written as `"@name"` with its number through
`DocumentContext::resolveToken`; a font's `size` reads the token's base value. Since the theme and the colours are
part of `appearance()`, a theme switch or an edit to `tokens` applies every panel again. Limits: 4,096 tokens, 32
themes, aliases 16 deep (deeper, or circular, is an error). A token is a number in every theme or a colour in every
theme, and a name can't be both a colour and a token.

## Invariants

- **A revision can't widen a capability.** `checkChange` refuses a revision that changes `permissions`, `channels` or
  `persist`, or adds, changes or moves a control step (`call`, `send`, `connect`, `patch`, `accept`, `reject`,
  `preview`, `endPreview`, `restore`, `revert`, `undo`), or a way to run one.
- **A revision can't bring new findings.** What it refers to must exist (`introduced(before, after)` is empty);
  findings that were there before don't block it.
- **Documents stay declarative:** no scripts. Expressions are pure: they read the state and never change it. Native
  work runs through `onCall` and `onAction`, which only the application registers.
- **The panels follow the document.** When a version lands, the runtime reconciles the panels in place, so focus,
  typing, scroll and drags survive. If a version can't be built, the runtime refuses it and version control rolls
  it back.
- **State changes during one event become one update,** run once the event has been delivered.
- **Cost follows the change** (4.1). Each record keeps what its values, its list of children and its subtree read
  (`ReadSet`, `src/updates/Reads.hpp`: JSON Pointers, a key read as any key `~*` when there are too many, and `~#` for a
  repeat's reading of a list's shape only). A state step records the places it wrote (`StateChanges`); the update
  after it visits only the records whose subtree reads one, applies again the values that do and builds again the
  lists of children that do. A repeat over a place of the state gives each item that place (`Scope::origin`), so its
  bindings read `/items/5/title`, not the whole list. A long list (64 children or more) looks the changed places up
  by item (`ItemIndex`) instead of visiting every child. A revision, a resize, a new theme or screen, and a change no
  place describes update everything. A subtree's reads may say more than it reads, never less: an update that
  visits only part of the panels gives the panels a full build gives (`tests/unit/updates.cpp`).
- **Data models** (4.3). `setModel(name, model)` registers rows the application owns (`DataModel`), which
  `"repeat": {"in": "models.<name>"}` goes over (`src/updates/Models.*`). Their rows lie at `/~m/<name>/<row>` among the
  places values read (`~m` is no JSON Pointer key, so it is never one of the state), and an item's bindings read its
  row there, live. A changed row marks its own place, so an update applies again only what reads it; an insertion, a
  removal, a reset or many rows at once mark the whole model, so the repeats over it build again. The catalog's
  `models` gives agents each model's count, the shape of a row and one sample row, never every row; the checker
  accepts any `models.<name>` (models exist only while the application runs), and building fails, naming the models
  there are, when the name is none of them (`tests/unit/models.cpp`).
- **Async calls** (4.7). `onAsyncCall(name, call)` registers a slow native function: a `call` step runs it on a
  worker pool (2 to 4 threads, started on first use), so no frame waits for it (`src/calls/AsyncCalls.*`). The step's
  `loading` place is true while it runs, `progress` goes from 0 to 1, partial results stream into `into`, `timeout`
  (ms) ends it with the error `timeout`, `then` runs with `result` and `onError` with `error`; `{"cancel": name}`
  stops its calls. A plain `onCall` function runs `then` with `result` too, right after it returns. Reports are
  coalesced: at most one delivery per call waits on the `Executor` (the Application, or `setExecutor`), and it
  carries the latest values. At most 64 calls run at once. A call ended (cancelled, timed
  out, or its runtime destroyed) changes nothing more; destroying the runtime waits for the functions running, so
  they should check `cancelled()`. Without an executor the call runs at once (`tests/unit/async.cpp`).
- **Services** (8.5a, RFC 0002). `setServices` gives the runtime a `services::Services` (the `services` module's
  `ServiceRegistry`, through `Screen::services()`), which loads the `services` section once the window is built.
  `{"invoke": "svc.method", "with": ..., "into": "state.x"}` calls a method: `loading`, `progress`, `timeout` (ms),
  `id` (so `{"cancel": id}` stops it), `onDone` with `result`, `onError` with `error` (`{code, message, data}`);
  a method that streams appends each event to `into` (`append`, `keep` at most 10,000) and runs `onEvent` with
  `event`. `{"subscribe": "svc.stream", "do": [...]}` runs `do` with each `event` until cancelled. A request a service
  sends runs the action `<service>:<method>` with its params and `request`, whose `respond` step answers it (within
  30 s; at most 256 wait). The `services` variable is each service's status. At most
  1,024 calls run and 256 subscriptions are open per screen; results come back through the `Executor`, and a call
  ended changes nothing more (`src/calls/ServiceCalls.*`, `tests/unit/services.cpp`). A revision can't change the
  section, may use only the declared services' revisable methods (`checkServiceUses`), and a step it adds that writes
  or runs something is always reviewed (`reviewLines`, `src/rules/ServiceRules.*`).
- **Connections** (8.6b, RFC 0003). `setNetwork` gives the runtime a `network::Network` (the `net` module's
  `Client`, through `Screen::network()`). The `connections` section declares servers and their operations (method,
  `path` with `{name}` segments, `query`, `body`, `headers`, `effect`, `stream`, `result`, `timeout`, `mock`), or a
  WebSocket's `messages`. `{"request": "api.getItem", "with": {...}, "into": "state.item"}` takes the keys of
  `invoke`; its `error` is `{code, message, status, body}`. `subscribe` reaches a connection when one has the name,
  else a service. A WebSocket runs the actions `<name>:open`, `<name>:<type>`, `<name>:error` and `<name>:closed`;
  the `connections` variable is each one's `{online, pending, state, error}`. At most 64 connections, 256
  operations each, 50 requests a second each. The section is a capability: a revision uses declared operations
  only (`checkServiceUses`), and one that writes is reviewed. While a proposal is previewed, only a read without a
  `mock` reaches the server (`src/connections/ConnectionSpec.*`, `src/connections/Connections.*`,
  `tests/unit/connections.cpp`). A connection's `auth` declares an OAuth provider (or names another connection's):
  `setIdentity` gives the runtime an `identity::Identity` (the `net` module's `SignIn`, through `Screen`), which adds
  the bearer header to each request; without one, a request fails with `signedOut`. The `signIn` and `signOut` steps
  are control steps, and the `auth` variable is each provider's `{state, account, expires, ...}`, never a token
  (`src/connections/ConnectionAuth.*`, `src/connections/AuthSteps.cpp`, `tests/unit/auth.cpp`).
- **Image files come only from allowed folders:** the screen's own (its base directory) and those the host allows
  (`allowImageDirectory`). An `image` node's `file` elsewhere is a `DocumentError` naming its place, so a revision
  naming one is refused. A screen loaded from a string without a base directory allows none.
- **`cache` is only a hint:** a node with `"cache": true` is drawn from a texture of its own (`Panel::setCached`).
  It looks the same and grants nothing, so `checkChange` has nothing to check; the renderer keeps its textures within
  a budget and draws a subtree directly beyond it.
- **Limits** (untrusted documents): expression nesting 100; `while` 10,000 iterations; `forEach` lists, `range` and
  `pad` 10,000; nested actions 32; nested components 32; computed values reading computed values 32; panels built
  inside each other 256 (`maxNodeDepth`); styles using styles 16; `include` inside `include` 8; a state write at most
  10,000 positions past a list's end (`maxListGap`); a text an expression builds (a template, `join`, `+`, `t`, a
  message) 64 MiB (`textLimit`); 250,000 panels per screen (`maxPanels`); a timer's delay about 24.8 days, as in web
  browsers, and 10,000 `after` steps waiting at once. The checker gives hints to the first 50 findings of a pass,
  each compared with at most 1,000 names. It gives up on a document that makes it visit more than 1,000,000 nodes,
  steps and values, or copy more than 4,000,000 shape nodes (a step that writes the state into the state doubles
  what it may hold): `checkChange` then compares only the node ids before and after. `checkChange` expands
  components looking for a way to run a control step until it has expanded as many bytes as a document may hold;
  past that, it refuses the revision.
- **State writes go through `statePlace()`** (`src/expression/Expression.hpp`), never `state[pointer]`: it throws
  `DocumentError` where nlohmann would throw a `json::exception`, which no event handler catches, and bounds how far a
  list grows.
- **Every event and every update has a work budget** (1.8c, `WorkBudget` in `Expression.hpp`, started by
  `DocumentContext::BudgetScope` in `Steps::handle` and `Reconciler::reconcile`, and in `Events` where a drag's
  data, a caret's place, a `bind` target or an overlay's dismissal is evaluated): 100,000 steps
  (`Steps::runStep`), 10,000,000 expression operations (`Machine::run`) and 64 MiB written into the state (`set`,
  `push`). Nested handlers share the outermost one's. Past one, what runs stops with an error that names its place.
- **Sizes** (`Limits.hpp`, in `Rules`): a revision of at most 100,000 operations and 16 MiB (`checkRevision`, called
  by version control before applying it), a document of 32 MiB without its state, a state of 32 MiB.

## Threads and ownership

Main thread only, except the workers named here. The runtime reads and writes the `VersionControl` it was given, and its
deferred work, timers and animations run through the window (`Window::defer`, `setTimer`) and read its clock. When it is
destroyed it cancels its timers, its deferred tasks check that it is still alive, and it disconnects every signal it
connected (the window's and its panels'), so a window can outlive its screen. Image files are decoded on the
`ImageCache`'s worker thread; from `createWindow(app)` on, the decoded images come back through the `Application` (an
`Executor`), and before it (`build` on a window of the caller's) they are decoded when asked for. `waitForImages` waits
for the worker and shows what it decoded. `onAsyncCall` functions run on a pool of 2 to 4 threads
(`src/calls/AsyncCalls.*`), which share only a call's cancellation and its latest report with the main thread; every
result comes back through the `Executor`, and destroying the runtime joins them.

`Runtime` owns its `Engine`, which owns the registries and the subsystems, declared in the order they are built in
(and destroyed in the reverse order: the async calls first, so no worker outlives what it reports to). The panels
belong to the window's panel tree (`render`); the `Reconciler` keeps one `Record` per panel it built, and drops a
subtree's records before the subtree is destroyed. A compiled node belongs to the `Builder` for one version of the
document, and its bindings point into that version (or into the component instance the compiled node keeps), which
outlives them. The checker reads the document where it is and copies only shapes, within its budget. Callers own the
`VersionControl` and the executor they give the runtime, which outlive it, and the services, network and identity,
which outlive it or are replaced first.

## Errors

- A malformed document or a refused change throws `DocumentError`, whose message names the place:
  `root.children[1].anchors: unknown edge 'lefft'`.
- An error while an event runs (a step that fails, a deferred change that is refused) doesn't throw out of the event
  loop: it is reported through `onError` and the log. A step's `onError` steps can handle it.
- A native `Call` that throws `std::exception` runs the step's `onError` steps, with `error`.

## Extension points

- **Application types:** `registerType(name, factory, properties, apply)`, through the type registry. The type has
  the properties of the built-in widget its panel derives from, plus its own, applied by `apply` whenever they change.
  A name the format already has replaces that type. `resolveColor` and
  `font` read colours and fonts as the document writes them. `examples/shared/CodeEditor.*` registers `code` this way.
- **Native actions and functions:** `onAction(name, action)` for `"onClick": "name"`, and `onCall(name, call)` for
  `{"call": name, "with": ..., "into": "state.x"}` steps. Tell `Rules::addNativeAction` about the actions, so the
  checker accepts them.
- **Events out:** `onEvent` (`emit` steps), `onSend` (`send` steps to a channel), `onConnect`, `onUpdated`.
- **Showing a document for a moment:** `setHandlersSuspended(true)` keeps the panels in line without running
  `onShow`, `onHide` or `watch`, taking focus or animating colours (pictures of proposals use it).
- **Item icons:** a list, tree or table item may carry `icon` (a glyph name) and `iconColor`; an application's
  model gives them through `ListItem`, so file icon themes stay in the application (`examples/forge`).
- **Expression functions:** a new function is a new row of the table in its group's file
  (`src/functions/ExpressionFunctions<Group>.cpp`): its name, arguments, one line of documentation, arity and
  implementation.
  The parser, the catalog's `operators` and SCREEN_FORMAT.md's function list (a unit test checks it against
  `functionGuide()`) come from the registry. A new group is a new file whose table `functions()` lists.

## Files

```
include/affresco/format/   Rules.hpp, NodeIds.hpp, Migrate.hpp, Runtime.hpp, Features.hpp, Usage.hpp, Locale.hpp,
                           Adapters.hpp, SyntaxHighlighter.hpp
src/
  runtime/                 the runtime and what it is made of
    Runtime.cpp            the public API, forwarding to the engine
    Engine.hpp / .cpp      the composition root: registries and subsystems
    DocumentContext.*      colours, fonts, theme, globals, computed values, the screen shown
    Scheduler.*            timers, animations, debounces, transitions, deferred tasks (instant when
                           window.reducedMotion)
    Settings.*             the settings section: defaults, checked writes to settings.x, the settings and
                           settingsList variables
    Usage.cpp              usage summaries (Usage.hpp): counts per node id and action and per screen, the controls
                           never used, bounded, over 30 days, in a file beside the "persist" state
  expression/              the expression language
    Expression.hpp / .cpp  the API, and the caches of compiled expressions and split templates
    ExpressionParser.*     the parser: an expression's text into its JSON operator tree
    ExpressionLower.cpp    lowering an operator tree once into typed nodes
    ExpressionMachine.*    the typed nodes and the machine that runs them: calls, paths, variables, templates
    Value.hpp / .cpp       the values expressions compute with: inline short texts, shared lists and objects
  functions/               ExpressionFunctions.*: the function registry; ExpressionFunctions<Group>.cpp hold its
                           tables (Color: curve() and gradientColor(); Locale: formatNumber, formatPercent,
                           formatCurrency, formatDate, formatTime, formatRelative, formatList, compareText, sortText)
  locale/                  translations and the CLDR subset
    Strings.*              the "strings" section (6.9): translations per locale, the fallback chain (fr-CA, fr,
                           the default), t() and its checks (unknown keys, missing and extra parameters)
    StringsMessage.cpp     ICU MessageFormat's subset: {name}, plural (=0, #), select, number, date and time
    Locale.hpp, Locale*.cpp  the compact CLDR subset (LocaleData.cpp, generated by tools/i18n/): plural rules,
                           numbers, currencies, dates, relative times, lists, sorting; LocaleSystem.cpp the OS's locale
  nodes/                   a version's nodes, compiled
    Builder.*              use, repeat, with, let, check, styles, when; the compiled nodes of a version
    CompiledNode.*         a node compiled: slots, bindings, scoping keys
    Binding.*              a value compiled once: templates split, operators lowered
    Record.hpp, NodeKeys.hpp  what the runtime keeps per panel, and what a node's keys mean
  updates/                 building panels, and updating only what reads a change
    Reconciler.*           records, building and reconciling panels; updates of only what reads a change
    Reads.*                what values read and what changed in the state, and when they meet
    ItemIndex.*            the children of a long list by the item of the state they read
    ListItems.*            a list's and a tree's items, read within limits, without recursion
    Models.*               the application's data models, their rows' places and their outline for agents
  events/                  Events.*: input (EventsFocus.cpp, EventsKeys.cpp, EventsPointer.cpp); Shortcuts.* names
                           keys, chords and shortcuts
  steps/                   Steps.*: handlers and actions; StepRegistry.* and the *Steps.cpp groups (AuthSteps.cpp,
                           the signIn and signOut steps, is in connections/)
  calls/                   calls outside the document
    AsyncCalls.*           slow native calls on a worker pool: loading, progress, partial results, timeouts, cancel
    ServiceCalls.*         invoke, subscribe and respond: calls to the services, their results and events in the state
  checker/                 the static checker
    Checker.hpp / .cpp     its passes, findings and hints ("did you mean ...")
    CheckerImpl.hpp        its private parts; CheckerShapes.hpp what it knows about values, and its limits
    CheckerNodes.cpp       its walk of nodes: components, styles, repeat, variables, references
    CheckerValues.cpp      its reading of values: expressions, operators, paths
    StateTypes.*           typed state: the stateTypes subset of JSON Schema, matching values, describing types
    CheckerTypes.cpp       the typed-state checks
  rules/                   the format as rules for version control
    Rules.cpp              validation, findNode, permissions, checkChange (capability sections, controls added)
    RulesControl.*         the control scan: control steps, and the actions and components that may run them
    BehaviourLines.cpp     behaviourLines: the actions, handlers, steps and bindings a change adds, removes or changes
    RulesNodes.cpp         Rules for revisions' nodes and paths: ids for the nodes they add, hints for paths
    NodeIds.cpp            where nodes are, their ids, duplicates, and readable ids for nodes without one
    Limits.*               how large a revision, a document and a state may get, and how a value's size is measured
    Migrate.cpp            version 1 documents migrated to version 2 (Migrate.hpp)
    ServiceRules.*         what revisions may do with services: declared ones only, and reviewed when they write or run
  adapters/                other formats converted to screens (Adapters.hpp): A2UI 0.8 (A2ui.*, A2uiComponents.cpp)
                           and Adaptive Cards 1.6 (AdaptiveCards.*, AdaptiveCardActions.cpp); Layout.* what they share;
                           AdapterMapping.* how a feature from another app gets what it needs from its host
  features/                features (RFC 0004)
    Features.cpp           features (RFC 0004): trusted publishers, catalogues, the install and upgrade revisions
    FeatureManifest.*      a feature's manifest: releases and ranges, its shape, features, exports and imports
    FeatureCatalog.*       catalogue v0: a signed index and its releases' manifests, with every check
    FeatureSignatures.*    Ed25519 signatures over canonical JSON (RFC 8785), base64url, SHA-512 digests
  composition/             the enabled features merged into what the runtime builds
    Composition.*          the enabled features merged into the screen the runtime builds, with RFC 0004's checks;
                           CompositionProof.* the proof that a feature from another app fits a host
    ComposedVersions.*     the document's version control as the runtime sees it, with the enabled features merged in
    ContributionPoints.*   the "points" section: where another app's views may go, and the contracts of exports
    Contributions.*        commands, keybindings and menus: what is one, enablement, the context, the commands,
                           keybindings and menus variables (built when read, DocumentContext::addVariable), and
                           the window's native menu bar (window.menuBar, shown after each update when it changed)
                           (Reconciler.cpp shows window.focusRing the same way; role, label and description are
                           properties, in Properties.cpp)
  connections/             the "connections" section and sign-in
    ConnectionSpec.*       the "connections" section, read and checked: connections, operations, messages
    Connections.*          request and subscribe on connections: requests built from declarations, WebSockets, previews
    ConnectionAuth.*       a connection's "auth": its provider, and the bearer header the host adds
    AuthSteps.cpp          the signIn and signOut steps
  sections/                top-level sections
    SectionRegistry.hpp    sections; Sections.cpp, CapabilitySections.cpp register them
    DesignTokens.*         the "tokens" section: aliases, per-theme values, colour and number tables
  components/              the built-in components
    BuiltInComponents.*    menus, the menu bar, dialogs, toasts, hover cards, the settings page and the quick
                           pick, written in the format; BuiltInFormComponents.cpp comboBox and passwordMeter;
                           BuiltInChatComponents.cpp the chat bubble, typing indicator, proposal card, composer
                           and launcher
  properties/              node properties
    Properties.*           a node's values on its panel; CommonProperties.cpp those every panel has and the core
                           types'; PropertyValues.* reads lengths, anchors, radii;
                           AccessibilityProperties.cpp isOpen, selected and live; TextFieldProperties.cpp secret,
                           maxLength and accept; TransformProperties.cpp rotate, scale, skew, translate, transformOrigin
    LayoutProperties.*     the flex and grid keys: those every node has (maxWidth, aspectRatio, shrink, basis,
                           alignSelf, cell, applied from layoutProperty()), the property sets of rows, columns,
                           stacks and grids (justify, alignItems, the gaps, rows, justifyItems), grid tracks
  reference/               what the format offers, from the registries
    Catalog.*              the catalog, from the registries
    Reference*.*           the JSON Schema and SCREEN_FORMAT.md's tables, from the registries and each key's entry
                           (ReferenceNodes.cpp, ReferenceNodeTypes.cpp, ReferenceNodeEvents.cpp, ReferenceSections.cpp,
                           ReferenceTables.cpp); `affrescoui schema` writes them
  types/, controls/,       node types: types/ the registry and the core types, controls/ calendar, carousel, codeInput,
  creative/                slider and toggle, creative/ canvas, colorPicker, curves, gradient, graph, path, timeline
    TypeRegistry.*         node types; Types.cpp registers them, each in its <Name>Type.cpp: editor, toggle, slider,
                           image, split, tabs, list (list and tree), path ("path": d, viewBox, fill and stroke, the
                           data parsed only when it changes), calendar, carousel, codeInput, diff, surface (a GPU
                           surface the host registered), and the creative controls colorPicker, gradient and curves
                           (CreativeValues.* reads and writes their values); canvas (its items within limits, levels
                           of detail, and its view bound both ways); graph (nodes, ports and links read within
                           limits, the edits as events for its handlers); timeline (tracks, keyframes, clips and
                           markers read within limits, scrubs and edits as events for its handlers); IconProperty.cpp
                           the labels' "icon", and RunsProperty.cpp their "runs", "selectable" and "selectionColor"
                           (within limits: 256 runs, 64 Ki characters)
    SyntaxHighlighter.cpp  a text editor's text kept parsed by the syntax engine, coloured a block of lines at a time
CMakeLists.txt             the affrescoui_format target and its sources
```

## Recipes

**Add a widget type.** A new file that registers it, like `ToggleType.cpp`: a `PropertySet` with its properties
(`PropertySpec`), an apply function that sets each from the values it is given (defaults for the missing ones), and,
if it has a value, its `ValueHooks`; then `registry.add({name, about, factory, set, false})`, called from
`registerBuiltInTypes` in `Types.cpp`. The catalog, the schema and `SCREEN_FORMAT.md`'s tables list it from the
registry: give its properties their entries in `ReferenceNodes.cpp` and a table (see below), run `affrescoui schema
-o schema/affrescoui-screen.schema.json --doc SCREEN_FORMAT.md`, and add a scenario that shows it.

**Add a property to every node.**
1. give it a `Slot` (`CompiledNode.hpp`, and its name in `namedSlots` in `CompiledNode.cpp`), and apply it in
   `CommonProperties.cpp` (`commonProperty`, or a group such as `shapeProperty`), with a reset in `reset`; a layout key
   goes in `LayoutProperties.cpp` instead (`layoutItemProperty`, reset in `resetLayoutItem`, listed in
   `layoutItemProperties`);
2. list it in the panel's set in `Types.cpp` (the catalog reads it), with `transition` if it can animate;
3. give it an entry in `ReferenceNodes.cpp` (its schema and its row in SCREEN_FORMAT.md's tables), then regenerate
   both with `affrescoui schema -o schema/affrescoui-screen.schema.json --doc SCREEN_FORMAT.md` (ctest
   `cli-reference` fails while they are out of date, and the `reference` unit tests while a key has no entry);
4. if states (`hover`, `pressed`, ...) or `transition` may set it, say so in its entry;
5. add a rendering-check scenario (`tests/render/scenarios/<area>.json`) that shows it.

**Add a step.** Add it to a group (a `*Steps.cpp` file: its `StepType` and the code in the group's runner), or a
new group in a new file registered from `registerBuiltInSteps` (the order there is the order groups are tried). The
catalog lists it from the registry. Add the key to the checker's `stepNames` (`CheckerNodes.cpp`), give it and the
keys it reads entries in `ReferenceSections.cpp`, and regenerate the schema and `SCREEN_FORMAT.md`'s tables with
`affrescoui schema` (as above). If the step reaches outside the document (a program, a file, the network, history), it is a
control step: mark it `control`, add it to `controlSteps` in `RulesControl.cpp` so a revision can't add it, and test that
refusal in `tests/unit/rules.cpp`.

**Add a document section.** A `SectionType` with its validator, added in `registerBuiltInSections` (`Sections.cpp`);
a section that grants a capability goes in `CapabilitySections.cpp` with `capability = true`, which `checkChange`
then guards.

**Add a node type for one application.** Don't add it here: register it from the application with
`Screen::registerType` (or `Runtime::registerType`), as `examples/shared/CodeEditor.cpp` does.
