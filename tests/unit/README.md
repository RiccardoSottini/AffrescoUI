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

A `Window` needs an OpenGL context, so tests of what happens on a window (timers, review steps, a screen's
properties, pictures) are a separate program, `affrescoui_window_tests`. It runs where the rendering check runs:
on a desktop, or under Xvfb on Linux, as in CI.

## Where they are

| File | Tests |
|---|---|
| `core.cpp` | `parseUntrusted`: the nesting limit, comments; the log's escapes; the portable C library calls (`Portable.hpp`) |
| `nesting.cpp` | The depth limit on what revisions and steps build (`maxDocumentNesting`): revisions refused, `set`, `push` and deep places stopped |
| `vcs.cpp` | `vcs::Repository`: revert, deny rules, outdated approvals, self-review, change sets, merges that no longer apply |
| `vcs_properties.cpp` | Properties of `vcs::Repository` over random revisions from seeded generators: revert, restore, change sets, permissions by effect |
| `history.cpp` | `vcs::Repository`'s history on disk: save and load, a changed screen file, damaged files, the lock |
| `expressions.cpp` | `format` expressions: a table of values and their results or errors, `truthy` |
| `channel.cpp` | `channel::Session` over an in-memory channel: hello, unknown messages, who may review, the log, `render`; `SocketChannel` over a socket pair: lines, the length limit (POSIX); `ProcessChannel` with small shell commands: output lines, stdin, closing ends the child's programs, no inherited pipes |
| `protocol.cpp` | The live protocol as a table: every message `SCREEN_FORMAT.md` lists, and its answer or error |
| `peers.cpp` | Limits on peers: the token bucket, a `Session`'s size and rate limits, the user check on accepted sockets, a `SocketChannel`'s bounded send queue |
| `checker.cpp` | `format` checker: tables of screens and their findings, the revision policy, the templates (none) |
| `painter.cpp` | What a `Panel` asks its `Painter` to draw (shadow, rounded fill, border), and a `Painter`'s fallbacks |
| `png.cpp` | The PNG encoder behind `render` (`modules/screen/src/Png.hpp`): each image decoded again and compared |
| `rules.cpp` | `format::Rules::checkChange`: control steps, and the actions, handlers and `run`s that reach them |
| `rules_sections.cpp` | `format::Rules` on the capability sections (`permissions`, `channels`, `persist`, `include`): `validate` and `checkChange`, as tables |
| `sockets.cpp` | `channel::SocketServer` and `SocketChannel` over a real local socket in a temporary folder: lines both ways, a peer going away, the file's owner-only access, a busy or stale socket file |
| `screen.cpp` | `Screen`'s paths: the default socket directory, where `persist` state is read from; the nesting limit on screen files |
| `text.cpp` | UTF-8 decoding, `Font` glyphs rasterised on demand into atlas pages, fallback fonts, the atlas limit, `fonts` with `fallback` (reads the examples' font files) |
| `timers.cpp` | `affrescoui_window_tests`: `Window` timers, and a screen's timers and deferred tasks when it is destroyed |
| `review.cpp` | `affrescoui_window_tests`: `accept` steps name the reviewed head; control steps while a proposal is previewed |
| `shapes.cpp` | `affrescoui_window_tests`: `radius`, `borderWidth`, `borderColor` and `shadow` on a screen: applied, reset, errors, the border colour's transition |
| `images.cpp` | `affrescoui_window_tests`: pictures of main and of a proposal, with the person's view left as it was |
| `Helpers.hpp` | What tests share: a screen with permissions, `MemoryChannel` |
| `main.cpp` | doctest's `main` |

## Adding tests

- Add a `TEST_CASE` to the file of the module it tests, named `"<module>: <what it shows>"`. A new module or area
  gets its own `.cpp`, listed in `affrescoui_unit_tests` in `CMakeLists.txt`.
- A new expression is one more row in the table in `expressions.cpp`: the value, and its result or the error text.
- Tests pin behaviour through the public headers. Private ones are fine when a module's logic lives there:
  `modules/format/src` is on the include path for `Expression.hpp`. Don't make a header public for a test.
- Tests don't need a window or a network, so they run anywhere, CI included. Only `timers.cpp`, `review.cpp` and
  `shapes.cpp` need a window. A test that needs files makes them in a temporary folder of its own and removes it
  (`history.cpp`, `sockets.cpp`, `peers.cpp`, `screen.cpp`); `protocol.cpp` reads `SCREEN_FORMAT.md`, `text.cpp`
  the fonts in `examples/assets/fonts`, and `checker.cpp` the templates in `examples/assets/screens`, which must have
  no findings (all from the checkout). The `ProcessChannel`
  tests start small programs through the system shell (`/bin/sh`, or `cmd.exe` on Windows).
- Property tests (`vcs_properties.cpp`) draw from `std::mt19937` with a fixed seed and read it with `%`, never
  through a `std::uniform_*_distribution`, whose numbers differ between standard libraries: a seed then gives the
  same cases on every platform. `CAPTURE` the seed and the step, so a failure says how to repeat it.
- CI holds a coverage floor for `core`, `vcs`, `channel` and `format/Rules.cpp`
  (`tools/quality/coverage-baseline.json`): a change that lowers their line coverage below it fails `Quality report`.
- Don't change library code to make a test pass. If a test finds a bug, keep it, mark it `* doctest::skip(true)`
  with a comment that describes the bug, and report it.
