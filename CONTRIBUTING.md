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

`AFFRESCOUI_VERIFY_UPDATES=1` checks every partial update against a full one: after each update that ran only the
readers of what changed, a full pass over the same panels must give the same values, or the screen reports the first
node and property that differ. It makes every update cost a full one, so it is for tests. When you change
`modules/format/src/updates/` (or what records reads or marks places), run the unit tests and the scenarios with it:

```sh
AFFRESCOUI_VERIFY_UPDATES=1 ctest --test-dir build -j 4 --timeout 900
AFFRESCOUI_VERIFY_UPDATES=1 python3 tests/render/check.py
```

## Live tests

Give the host, the MCP server and the channels inside screens the same short socket folder of your own, so that
parallel sessions never meet: `--socket out/s` and `AFFRESCOUI_SOCKET_DIR=out/s`.

## Adding to the catalogue

The catalogue (`catalogue/`, its `README.md`) lists the versions of apps and the features that the Launcher installs.
To add an entry, open a pull request that adds its file and its lines in `catalogue/index.json`:

- **A version of an app.** Make it on an app of `examples/`, pack it with `affrescoui pack` and give it a licence
  (`--license`). List one or more scenarios under `proof`: they show what the version changes. Put it at
  `catalogue/<id>/<release>.affresco.json`, and list it under `packages` with its `base` app.
- **A feature.** Put its manifest at `catalogue/<name>/<release>.json` with its licence and its scenarios, and list it
  under `features`.

Then run `./build/affrescoui catalogue refresh catalogue` to write the digests and sizes, and
`./build/affrescoui catalogue check catalogue`. The check prints one line per problem and exits 1 on any. For each
entry, it checks that:

- the file passes the checks of `affrescoui inspect` (a package) or of a feature's manifest;
- the base app is in `examples/`, and its tree is the one the package was made on when the entry names the app's
  current release;
- the proof's scenarios pass, played headless on the package installed over its base;
- it adds no capability (`channels`, `services`, `connections`, `persist`) and no `uses`;
- it passes the scan for local paths, keys and tokens;
- it has a licence;
- its digest and size are the ones `index.json` lists.

A reviewer reads what the person would read before installing: the summary of `affrescoui inspect <file>`, the
definitions it adds, changes and removes, each hook and each piece of advice with the point it joins, and each
capability it asks for. A change that does more than it says is refused. Listing an entry is not a promise that it is
safe. The release key's signature on the index says that the maintainer published it and that it is unchanged since.
An entry still installs as a proposal that the person reviews and accepts.
