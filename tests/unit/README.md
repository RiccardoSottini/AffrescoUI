# Unit tests

Fast tests for what needs no window: `core`, `vcs`, `format` (rules, expressions and the checker), `channel`, and
loading a `Screen`. They use [doctest](https://github.com/doctest/doctest) 2.4.12, fetched by CMake, and run in well
under a second. The rendering check (`tests/render`) covers what is drawn; these tests cover the logic underneath.

```sh
cmake -S . -B build -G Ninja && cmake --build build   # AFFRESCOUI_BUILD_TESTS is on in a top-level build
./build/affrescoui_unit_tests                         # or: ctest --test-dir build -R unit
./build/affrescoui_unit_tests --test-case="vcs*"      # some of them (doctest's --help lists the options)
./build/affrescoui_unit_tests --no-skip               # also the skipped ones (known bugs)
./build/affrescoui_window_tests                       # the few that need a window (or: ctest --test-dir build -R window)
```

A window's logic needs no display: the tests of what happens on a window (layout, input, focus, timers, review
steps, a screen's properties) use a **headless window** and a clock they move by hand, from
`modules/render/src/Headless.hpp` (`detail::createHeadlessWindow`, `detail::ManualClock`), so they run in every CI
job and never sleep. Only drawing needs an OpenGL context: the tests of pictures are a separate program,
`affrescoui_window_tests`, made of the files in `window/`, which runs where the rendering check runs: on a desktop, or under Xvfb on Linux, as in CI.

## Where they are

| File | Tests |
|---|---|
| `core.cpp` | `parseUntrusted`: the nesting limit, comments |
| `vcs.cpp` | `vcs::Repository`: revert, deny rules, outdated approvals, self-review, change sets, merges that no longer apply |
| `history.cpp` | `vcs::Repository`'s history on disk: save and load, a changed screen file, damaged files, the lock |
| `expressions.cpp` | `format` expressions: a table of values and their results or errors, `truthy` |
| `channel.cpp` | `channel::Session` over an in-memory channel: hello, unknown messages, who may review, the log, `render`; `SocketChannel` over a socket pair: lines, the length limit (POSIX); `ProcessChannel` with small shell commands: output lines, stdin, closing ends the child's programs, no inherited pipes |
| `checker.cpp` | `format` checker: tables of screens and their findings, the revision policy, the templates (none) |
| `painter.cpp` | What a `Panel` asks its `Painter` to draw (shadow, rounded fill, border), and a `Painter`'s fallbacks |
| `png.cpp` | The PNG encoder behind `render` (`modules/screen/src/Png.hpp`): each image decoded again and compared |
| `rules.cpp` | `format::Rules::checkChange`: control steps, and the actions, handlers and `run`s that reach them |
| `sockets.cpp` | `channel::SocketServer` and `SocketChannel` over a real local socket in a temporary folder: lines both ways, a peer going away, the file's owner-only access, a busy or stale socket file |
| `screen.cpp` | `Screen`'s paths: the default socket directory, where `persist` state is read from; the nesting limit on screen files |
| `text.cpp` | UTF-8 decoding, `Font` glyphs rasterised on demand into atlas pages, fallback fonts, the atlas limit, `fonts` with `fallback` (reads the examples' font files) |
| `headless.cpp` | A headless `Window`: anchors, percentages, minimum sizes, rows and columns; clicks, mouse capture, hover, scrolling, focus; the tooltip delay, timers and deferred tasks on a manual clock |
| `timers.cpp` | `Window` timers, and a screen's timers, deferred tasks and animations, and what is left when it is destroyed (headless) |
| `review.cpp` | `accept` steps name the reviewed head; control steps while a proposal is previewed (headless) |
| `shapes.cpp` | `radius`, `borderWidth`, `borderColor` and `shadow` on a screen: applied, reset, errors, the border colour's transition (headless) |
| `runtime.cpp` | The runtime on a headless window: the shipped templates build, and the Kanban template answers a click |
| `window/images.cpp` | `affrescoui_window_tests`: pictures of main and of a proposal, with the person's view left as it was |
| `Helpers.hpp` | What tests share: a screen with permissions, `MemoryChannel` |
| `main.cpp` | doctest's `main`, for both programs |
| `CMakeLists.txt` | The two programs: every `*.cpp` here, and every `window/*.cpp` with `main.cpp` |

## Adding tests

- Add a `TEST_CASE` to the file of the module it tests, named `"<module>: <what it shows>"`. A new module or area
  gets its own `.cpp` in this folder, or in `window/` if it needs a window. There is no list to edit: `CMakeLists.txt`
  finds the files with a glob (`CONFIGURE_DEPENDS`), so the next build compiles a new one. Parallel changes that add
  test files don't touch a shared file.
- A new expression is one more row in the table in `expressions.cpp`: the value, and its result or the error text.
- Tests pin behaviour through the public headers. Private ones are fine when a module's logic lives there:
  `modules/format/src` is on the include path for `Expression.hpp`. Don't make a header public for a test.
- Tests don't need a display or a network, so they run anywhere, CI included. Use a headless window for anything
  that happens on a window; only the files in `window/` need OpenGL. A test that needs files makes them in a temporary folder of its own and removes it
  (`history.cpp`, `sockets.cpp`, `screen.cpp`); `text.cpp` reads the fonts in `examples/assets/fonts`, and `checker.cpp` the
  templates in `examples/assets/screens`, which must have no findings (both from the checkout). The `ProcessChannel`
  tests start small programs through the system shell (`/bin/sh`, or `cmd.exe` on Windows).
- Don't change library code to make a test pass. If a test finds a bug, keep it, mark it `* doctest::skip(true)`
  with a comment that describes the bug, and report it.
