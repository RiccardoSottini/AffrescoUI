# Unit tests

Fast tests for the modules that need no window: `core`, `vcs`, `format` (rules and expressions) and `channel`. They
use [doctest](https://github.com/doctest/doctest) 2.4.12, fetched by CMake, and run in well under a second. The
rendering check (`tests/render`) covers what is drawn; these tests cover the logic underneath.

```sh
cmake -S . -B build -G Ninja && cmake --build build   # AFFRESCOUI_BUILD_TESTS is on in a top-level build
./build/affrescoui_unit_tests                         # or: ctest --test-dir build -R unit
./build/affrescoui_unit_tests --test-case="vcs*"      # some of them (doctest's --help lists the options)
./build/affrescoui_unit_tests --no-skip               # also the skipped ones (known bugs)
./build/affrescoui_window_tests                       # the few that need a window (or: ctest --test-dir build -R window)
```

A `Window` needs an OpenGL context, so tests of what happens on a window (timers) are a separate program,
`affrescoui_window_tests`. It runs where the rendering check runs: on a desktop, or under Xvfb on Linux.

## Where they are

| File | Tests |
|---|---|
| `core.cpp` | `parseUntrusted`: the nesting limit, comments |
| `vcs.cpp` | `vcs::Repository`: revert, deny rules, outdated approvals, self-review, change sets, merges that no longer apply |
| `history.cpp` | `vcs::Repository`'s history on disk: save and load, a changed screen file, damaged files, the lock |
| `expressions.cpp` | `format` expressions: a table of values and their results or errors, `truthy` |
| `channel.cpp` | `channel::Session` over an in-memory channel: hello, unknown messages, who may review |
| `timers.cpp` | `affrescoui_window_tests`: `Window` timers, and a screen's timers when it is destroyed |
| `Helpers.hpp` | What tests share: a screen with permissions, `MemoryChannel` |
| `main.cpp` | doctest's `main` |

## Adding tests

- Add a `TEST_CASE` to the file of the module it tests, named `"<module>: <what it shows>"`. A new module or area
  gets its own `.cpp`, listed in `affrescoui_unit_tests` in `CMakeLists.txt`.
- A new expression is one more row in the table in `expressions.cpp`: the value, and its result or the error text.
- Tests pin behaviour through the public headers. Private ones are fine when a module's logic lives there:
  `modules/format/src` is on the include path for `Expression.hpp`. Don't make a header public for a test.
- Tests don't need a window or a network, so they run anywhere, CI included. Only `timers.cpp` needs a window. Tests
  that need files (`history.cpp`) make their own temporary folder and remove it at the end.
- Don't change library code to make a test pass. If a test finds a bug, keep it, mark it `* doctest::skip(true)`
  with a comment that describes the bug, and report it.
