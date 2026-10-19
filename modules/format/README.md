# format: the screen format

`format` gives a screen document (`SCREEN_FORMAT.md`) its meaning. `Rules` tells version control what a valid
document is, where a node is, who may change what, and which changes are forbidden for everyone; `Checker` finds what
a document refers to but doesn't define, without running it; `Runtime` builds the document into `render` panels,
keeps them in line with every new version and state, and runs events, actions and steps. Expressions (`{{...}}`) are
compiled to JSON operator trees and evaluated on `json`. The format is general: it describes any application's UI,
and what one application needs that the format can't express comes in through an extension point, not a feature.
Target: `affrescoui::format` (uses `core`, `render` and nlohmann/json).

## Public headers (`include/affresco/format/`)

| Header | Offers |
|---|---|
| `Rules.hpp` | `Rules`, the format's `DocumentRules`: `validate`, `findNode` (`#id`), `permissions`, `checkChange`; `findings` (the checker's findings as text); `addNativeAction` |
| `Runtime.hpp` | `Runtime`: `registerType`, `createWindow` and `build`, `instantiate`, `onAction`, `onCall`, `runAction`, `catalog`, `find`, `color`, `resolveColor`, `font`; the signals `onEvent`, `onError`, `onSend`, `onConnect`, `onUpdated` |

Private headers that tests and benchmarks may include: `src/Expression.hpp` (compile and evaluate) and
`src/Checker.hpp` (`check`, `introduced`, `Finding`).

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
  `pad` 10,000; nested actions 32; nested components 32; styles using styles 16; `include` inside `include` 8.

## Threads

Main thread only. The runtime reads and writes the `VersionControl` it was given, and its deferred work, timers and
animations run through the window (`Window::defer`, `setTimer`). It cancels its timers when it is destroyed, and its
deferred tasks check that it is still alive.

## Errors

- A malformed document or a refused change throws `DocumentError`, whose message names the place:
  `root.children[1].anchors: unknown edge 'lefft'`.
- An error while an event runs (a step that fails, a deferred change that is refused) doesn't throw out of the event
  loop: it is reported through `onError` and the log. A step's `onError` steps can handle it.
- A native `Call` that throws `std::exception` runs the step's `onError` steps, with `error`.

## Extension points

- **Application types:** `registerType(name, factory, properties, apply)`. The type has the properties of the
  built-in type its panel derives from, plus its own, applied by `apply` whenever they change. `resolveColor` and
  `font` read colours and fonts as the document writes them. `examples/CodeEditor.*` registers `code` this way.
- **Native actions and functions:** `onAction(name, action)` for `"onClick": "name"`, and `onCall(name, call)` for
  `{"call": name, "with": ..., "into": "state.x"}` steps. Tell `Rules::addNativeAction` about the actions, so the
  checker accepts them.
- **Events out:** `onEvent` (`emit` steps), `onSend` (`send` steps to a channel), `onConnect`, `onUpdated`.
- **Coming:** registries for node types, steps and document sections, and a runtime split into subsystems (from
  7.12); a bytecode VM for expressions (from 7.5). Today a built-in node type, property or step is an edit to the
  runtime's files (see the recipes).

## Files

```
include/affresco/format/   Rules.hpp, Runtime.hpp
src/
  Rules.cpp                validation, findNode, permissions, checkChange (control steps, capability sections)
  Checker.hpp / .cpp       the static checker: its passes, findings and hints ("did you mean ...")
  CheckerImpl.hpp          the checker's private parts
  CheckerNodes.cpp         the checker's walk of nodes: components, styles, repeat, variables, references
  CheckerValues.cpp        the checker's reading of values: expressions, operators, paths
  Expression.hpp / .cpp    expressions: the parser, the compile cache, the operators, evaluation
  RuntimeImpl.hpp          Runtime::Impl, shared by the runtime's files
  Runtime.cpp              the public API, colours, fonts, styles, the catalog
  Build.cpp                building panels from nodes, and reconciling them with each new version
  Properties.cpp           a node's properties on its panel: layout, look, shapes, states and transitions
  Events.cpp               input: pointer, keys, focus, drag and drop, bubbling, overlay dismissal
  Steps.cpp                actions and their steps
CMakeLists.txt             the affrescoui_format target and its sources
```

## Recipes

**Add a property to every node.** Until the registries (7.12):
1. apply it in `Properties.cpp` (`commonProperty`, or a group such as `shapeProperty`), with a reset in `reset`;
2. list it in the catalog (`Runtime::catalog` in `Runtime.cpp`);
3. add it to the schema (`schema/affrescoui-screen.schema.json`) and to `SCREEN_FORMAT.md`, in the same change;
4. if states (`hover`, `pressed`, ...) or `transition` may set it, say so in all three places;
5. add a rendering-check scenario (`tests/render/scenarios/<area>.json`) that shows it.

**Add a step.** Until the registries: handle its key in `Steps.cpp`, add the key to the checker's `stepNames`
(`CheckerNodes.cpp`) and to the catalog, then the schema and `SCREEN_FORMAT.md`. If the step reaches outside the
document (a program, a file, the network, history), it is a control step: add it to `controlSteps` in `Rules.cpp`,
so a revision can't add it, and test that refusal in `tests/unit/rules.cpp`.

**Add a node type for one application.** Don't add it here: register it from the application with
`Screen::registerType` (or `Runtime::registerType`), as `examples/CodeEditor.cpp` does.
