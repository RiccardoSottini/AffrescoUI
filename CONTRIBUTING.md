# Contributing to AffrescoUI

How to build, check and test a change. The project's overview is `README.md`; the rules every change follows are in
`CLAUDE.md`.

## Build and check

```sh
cmake -S . -B build -G Ninja -DCMAKE_COMPILE_WARNING_AS_ERROR=ON
cmake --build build
ctest --test-dir build -j 3 --timeout 900
python3 tests/render/check.py          # every template and scenario, pixel for pixel
python3 tests/smoke/open_apps.py       # every app opens, every way
```

## Tests run without windows on screen

`AFFRESCOUI_HIDDEN=1` (any value but empty or `0`) opens every window of an AffrescoUI program hidden: it never comes
on screen, never takes the focus, and on macOS the program shows in neither the Dock nor the app switcher. Windows
still draw offscreen, so `AFFRESCOUI_SCREENSHOT`, `AFFRESCOUI_SCREENSHOT_AFTER` and the scenario runners capture the
same pixels as from a shown window.

Every harness sets it by default: `ctest` (each test's environment, from CMake), `tests/render/check.py`,
`tests/smoke/open_apps.py`, `tests/smoke/first_frames.py`, the end-to-end tests, `tools/affrescoui/check.py`,
`tools/media/capture.py` and `tests/perf/run.py`. To watch a run, set `AFFRESCOUI_HIDDEN=0`:

```sh
AFFRESCOUI_HIDDEN=0 python3 tests/render/check.py --only <scenario>
```

A program run by hand (`./build/affrescoui_launcher`) shows its windows unless the variable is set.

## Live tests

Give the host, the MCP server and the channels inside screens the same short socket folder of your own, so that
parallel sessions never meet: `--socket out/s` and `AFFRESCOUI_SOCKET_DIR=out/s`.
