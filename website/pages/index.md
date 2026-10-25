# AffrescoUI

AffrescoUI is a C++23 / OpenGL 4.1 GUI library. Its user interfaces are **JSON screen documents**: layout, styles,
state, expressions and behaviour in one file, which people and AI agents change **while the app runs**. Every change
is a revision under version control, with permission rules deciding what applies, what waits for review and what is
refused. The aim is large apps such as IDEs and creative tools.

```json
--8<-- "recipes/counter.screen.json"
```

That file is a whole app: `affrescoui run website/recipes/counter.screen.json` shows it, and
`affrescoui check` checks it without opening a window. Every screen on this site is a file in
[`website/recipes`](https://github.com/RiccardoSottini/AffrescoUI/tree/master/website/recipes), checked by the build.

## Where to start

| If you want to | Read |
|---|---|
| See it run | [Try it in a minute](tutorials/try-it.md) |
| Write a screen | [A first screen](tutorials/first-screen.md), then [state, expressions and actions](tutorials/state-expressions-actions.md) |
| Let an agent change a running app | [An agent changes a running app](tutorials/agent-review.md) and [version control and permissions](tutorials/version-control.md) |
| Embed it in a program | [From C++](tutorials/from-cpp.md) |
| Copy a pattern | [The cookbook](cookbook.md) |
| Look something up | [The reference](reference/index.md) |
| Know what it protects against | [The threat model](security.md) |
| Decide whether it fits | [Comparisons](comparisons.md) |

## What it is today

- Runs on macOS, Linux and Windows, and in the browser through a WebAssembly build. CI builds and tests it on Linux
  and Windows and builds it on macOS, where it is developed.
- Built from source with CMake 3.25 or later and a C++23 compiler; CMake fetches the pinned dependencies. The library
  is used from C++, from C through `affresco/affresco.h`, and from Python through `bindings/python`
  ([the C API and Python](tutorials/bindings.md)).
- Screen readers read and drive its windows through the platform's accessibility API (VoiceOver, Narrator, Orca).
- Example apps, from a Kanban board to an IDE, run on one host, `affrescoui_launcher`, with no C++ of their own.
- Licensed under the Apache License 2.0.
