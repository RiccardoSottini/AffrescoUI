# format: the screen format

`format` gives a screen document (`SCREEN_FORMAT.md`) its meaning. `Rules` tells version control what a valid
document is, where a node is, who may change what, and which changes are forbidden for everyone; `Checker` finds what
a document refers to but doesn't define, without running it; `Runtime` builds the document into `render` panels,
keeps them in line with every new version and state, and runs events, actions and steps. Expressions (`{{...}}`) are
parsed into JSON operator trees (what the checker reads), lowered once into typed nodes, and run by a small machine
on `Value`s, which read the state in place: JSON appears only at the edges. The format is general: it describes any application's UI,
and what one application needs that the format can't express comes in through an extension point, not a feature.
Target: `affrescoui::format` (uses `core`, `render` and nlohmann/json).

## Public headers (`include/affresco/format/`)

| Header | Offers |
|---|---|
| `Rules.hpp` | `Rules`, the format's `DocumentRules`: `validate`, `findNode` (`#id`), `permissions`, `checkChange`, `nameNodes` (ids for the nodes a revision adds) and `pathHint` (hints for paths that find nothing); `findings` (the checker's findings as text); `addNativeAction` |
| `NodeIds.hpp` | Node ids (D15): `nodeTrees`, `forEachNode`, `findNodeWithId`, `nodeIds`, `duplicateIds`, `isNodePlace`, `placeOf`, and `NodeNamer`, which gives readable ids (`label-blocked`) to nodes that have none |
| `Runtime.hpp` | `Runtime`: `registerType`, `createWindow` and `build`, `instantiate`, `onAction`, `onCall`, `runAction`, `setHandlersSuspended`, `catalog`, `find`, `color`, `resolveColor`, `font`; the signals `onEvent`, `onError`, `onSend`, `onConnect`, `onUpdated` |

Private headers that tests and benchmarks may include: `src/Expression.hpp` (compile and evaluate), `src/Value.hpp`
(the values expressions compute with), `src/ExpressionFunctions.hpp` (the function registry),
`src/Checker.hpp` (`check`, `introduced`, `Finding`), and the registries (`src/TypeRegistry.hpp`,
`src/StepRegistry.hpp`, `src/SectionRegistry.hpp`).

## The runtime's parts

`Runtime::Impl` is `Engine` (`src/Engine.hpp`), a composition root that owns the registries and eight subsystems.
Each subsystem's header says its role, its invariants and its callers:

| Subsystem | Does |
|---|---|
| `DocumentContext` | colours, fonts (from a bounded `FontCache`), the theme, the globals of expressions, the `computed` cache, the screen an outlet shows |
| `Builder` | compiles each node once per version of the document; a node's children after `use`, `repeat`, `with`, `let`, `check`, styles and `when` |
| `Reconciler` | the record of every panel built; keeps the panels in line with the document; batched updates |
| `Properties` | a compiled node's values on its panel, by slot; the property sets node types registered |
| `Events` | pointer, keys, focus, drag and drop, bubbling, overlays closed by a click outside, widgets' values |
| `Steps` | handlers and actions; each step runs in the group of the step registry that recognises it |
| `Scheduler` | `every`, `after`, `debounce`, `animate`, transitions and deferred tasks, on the window's clock |
| catalog | `Catalog.cpp`: what the format offers, built from the registries |

| Registry | Registers | From |
|---|---|---|
| `TypeRegistry` | a node type: its factory; its properties (name, kind of value, whether states and `transition` may set them); its apply function; a widget's value hooks | `Types.cpp` (the core types), `EditorType.cpp`, `ToggleType.cpp`, `SliderType.cpp`; the application's `registerType` |
| `StepRegistry` | a group of steps: each step's name, keys, and whether it is a control step; the group's runner | `StateSteps.cpp`, `FlowSteps.cpp`, `OutsideSteps.cpp`, `ReviewSteps.cpp`, `NavigationSteps.cpp`, `TimeSteps.cpp`, `OtherSteps.cpp` |
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
error comes again.

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
- **Limits** (untrusted documents): expression nesting 100; `while` 10,000 iterations; `forEach` lists, `range` and
  `pad` 10,000; nested actions 32; nested components 32; computed values reading computed values 32; panels built
  inside each other 256 (`maxNodeDepth`); styles using styles 16; `include` inside `include` 8; a state write at most
  10,000 positions past a list's end (`maxListGap`). The checker gives hints to the first 50 findings of a pass, each
  compared with at most 1,000 names.
- **State writes go through `statePlace()`** (`Expression.hpp`), never `state[pointer]`: it throws `DocumentError`
  where nlohmann would throw a `json::exception`, which no event handler catches, and bounds how far a list grows.

## Threads

Main thread only. The runtime reads and writes the `VersionControl` it was given, and its deferred work, timers and
animations run through the window (`Window::defer`, `setTimer`) and read its clock. When it is destroyed it cancels
its timers, its deferred tasks check that it is still alive, and it disconnects every signal it connected (the
window's and its panels'), so a window can outlive its screen.

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
  `font` read colours and fonts as the document writes them. `examples/CodeEditor.*` registers `code` this way.
- **Native actions and functions:** `onAction(name, action)` for `"onClick": "name"`, and `onCall(name, call)` for
  `{"call": name, "with": ..., "into": "state.x"}` steps. Tell `Rules::addNativeAction` about the actions, so the
  checker accepts them.
- **Events out:** `onEvent` (`emit` steps), `onSend` (`send` steps to a channel), `onConnect`, `onUpdated`.
- **Showing a document for a moment:** `setHandlersSuspended(true)` keeps the panels in line without running
  `onShow`, `onHide` or `watch`, taking focus or animating colours (pictures of proposals use it).
- **Expression functions:** a new function is a new row of the table in its group's file
  (`src/ExpressionFunctions<Group>.cpp`): its name, arguments, one line of documentation, arity and implementation.
  The parser, the catalog's `operators` and SCREEN_FORMAT.md's function list (a unit test checks it against
  `functionGuide()`) come from the registry. A new group is a new file whose table `functions()` lists.

## Files

```
include/affresco/format/   Rules.hpp, NodeIds.hpp, Runtime.hpp
src/
  Rules.cpp                validation, findNode, permissions, checkChange (control steps, capability sections)
  RulesNodes.cpp           Rules for revisions' nodes and paths: ids for the nodes they add, hints for paths
  NodeIds.cpp              where nodes are, their ids, duplicates, and readable ids for nodes without one
  Checker.hpp / .cpp       the static checker: its passes, findings and hints ("did you mean ...")
  CheckerImpl.hpp          the checker's private parts
  CheckerNodes.cpp         the checker's walk of nodes: components, styles, repeat, variables, references
  CheckerValues.cpp        the checker's reading of values: expressions, operators, paths
  Expression.hpp / .cpp    expressions: the API, and the caches of compiled expressions and split templates
  ExpressionParser.*       the parser: an expression's text into its JSON operator tree
  ExpressionLower.cpp      lowering an operator tree once into typed nodes
  ExpressionMachine.*      the typed nodes and the machine that runs them: calls, paths, variables, templates
  ExpressionFunctions.*    the function registry; ExpressionFunctions<Group>.cpp hold its tables
  Value.hpp / .cpp         the values expressions compute with: inline short texts, shared lists and objects
  Runtime.cpp              the public API, forwarding to the engine
  Engine.hpp / .cpp        the composition root: registries and subsystems
  DocumentContext.*        colours, fonts, theme, globals, computed values, the screen shown
  Builder.*                use, repeat, with, let, check, styles, when; the compiled nodes of a version
  CompiledNode.*           a node compiled: slots, bindings, scoping keys
  Binding.*                a value compiled once: templates split, operators lowered
  Reconciler.*             records, building and reconciling panels
  Properties.*             a node's values on its panel; PropertyValues.* reads lengths, anchors, radii
  Events.*                 input; Shortcuts.* names keys and shortcuts
  Steps.*                  handlers and actions; StepRegistry.* and the *Steps.cpp groups
  Scheduler.*              timers, animations, debounces, transitions, deferred tasks
  Catalog.*                the catalog, from the registries
  TypeRegistry.*           node types; Types.cpp, EditorType.cpp, ToggleType.cpp, SliderType.cpp register them
  SectionRegistry.hpp      sections; Sections.cpp, CapabilitySections.cpp register them
  Record.hpp, NodeKeys.hpp what the runtime keeps per panel, and what a node's keys mean
CMakeLists.txt             the affrescoui_format target and its sources
```

## Recipes

**Add a widget type.** A new file that registers it, like `ToggleType.cpp`: a `PropertySet` with its properties
(`PropertySpec`), an apply function that sets each from the values it is given (defaults for the missing ones), and,
if it has a value, its `ValueHooks`; then `registry.add({name, about, factory, set, false})`, called from
`registerBuiltInTypes` in `Types.cpp`. The catalog lists it from the registry. Add it to the schema and to
`SCREEN_FORMAT.md` in the same change, and a scenario that shows it.

**Add a property to every node.**
1. give it a `Slot` (`CompiledNode.hpp`, and its name in `namedSlots` in `CompiledNode.cpp`), and apply it in
   `Properties.cpp` (`commonProperty`, or a group such as `shapeProperty`), with a reset in `reset`;
2. list it in the panel's set in `Types.cpp` (the catalog reads it), with `transition` if it can animate;
3. add it to the schema (`schema/affrescoui-screen.schema.json`) and to `SCREEN_FORMAT.md`, in the same change;
4. if states (`hover`, `pressed`, ...) or `transition` may set it, say so in all three places;
5. add a rendering-check scenario (`tests/render/scenarios/<area>.json`) that shows it.

**Add a step.** Add it to a group (a `*Steps.cpp` file: its `StepType` and the code in the group's runner), or a
new group in a new file registered from `registerBuiltInSteps` (the order there is the order groups are tried). The
catalog lists it from the registry. Add the key to the checker's `stepNames` (`CheckerNodes.cpp`), the schema and
`SCREEN_FORMAT.md`. If the step reaches outside the document (a program, a file, the network, history), it is a
control step: mark it `control`, add it to `controlSteps` in `Rules.cpp` so a revision can't add it, and test that
refusal in `tests/unit/rules.cpp`.

**Add a document section.** A `SectionType` with its validator, added in `registerBuiltInSections` (`Sections.cpp`);
a section that grants a capability goes in `CapabilitySections.cpp` with `capability = true`, which `checkChange`
then guards.

**Add a node type for one application.** Don't add it here: register it from the application with
`Screen::registerType` (or `Runtime::registerType`), as `examples/CodeEditor.cpp` does.
