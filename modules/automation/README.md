# automation: scenarios, the checks a screen ships with

`automation` plays scenarios: scripted input (click, type, key, drag, scroll, wait, revision, action) and
expectations (a state value, a node's existence, text or property, a picture against a golden image) that a screen
ships next to it as `<name>.scenarios.json` (SCREEN_FORMAT.md, "Scenarios"). Any app, and any agent changing one,
proves a change with the screen's own scenarios: `affrescoui test` and the MCP tool `run_scenarios` play them, and the
rendering check (`tests/render`) is a set of scenarios whose expectations are pictures. Target:
`affrescoui::automation` (uses `affrescoui::affrescoui`, the whole library).

## Public headers (`include/affresco/automation/`)

| Header | Offers |
|---|---|
| `Scenarios.hpp` | `ScenarioFile` (`load`, `parse`: read and checked before anything plays), `Scenario`; `Options` (golden images and `Goldens` policy, output folder, another document, only some scenarios, pictures on or off, `prepare` for the application's types); `run`; `Report` (`passed`, `failed`, `text`, `toJson`), `ScenarioResult`, `Failure`, `Picture` |

## Invariants

- **Checked first.** A scenarios file is read whole and every step checked (one kind per step, known keys, keys,
  targets, state paths, properties, names) before anything plays; an error names the file and the place:
  `AffrescoUI scenarios: kanban.scenarios.json: scenarios[1].steps[3]: unknown step {"clik":"#card"}`.
- **Fresh and alone.** Each scenario loads its screen again: no saved history or `persist` state, channels looking for
  their hosts in a folder with none, even a channel at an absolute path (they show their disconnected state), what
  they post dropped. So a scenario plays the same way on any machine, whatever ran before, and never meets a running
  app.
- **Headless unless it draws.** A scenario plays on a headless window (`render/src/Headless.hpp`) unless a step draws
  (`capture`, `expect image`); then on a hidden window with OpenGL, from an `Application` it owns for its time.
- **Time moves only on `wait`.** The window's clock is a `ManualClock`. A wait runs each timer at its own time, in
  order (rounded up to the microsecond), so a repeating timer runs once per period and an animation draws each frame,
  as in real time. What a handler leaves for the next frame (a `focus`) runs at the next wait.
- **Expectations don't stop a scenario; steps that can't be played do.** A failed expectation is recorded and the
  scenario goes on; a target that isn't there, a refused revision or an action that throws ends it.
- **Pictures are the rendering check's.** A picture is `Window::saveScreenshot` at the scenario's scale, compared pixel
  for pixel with `<goldens>/<scenario>-<name>.png`; one that differs is written with a diff (the golden image faded,
  changed pixels in red).

## Threads

Main thread only, like the windows it drives. One `run` at a time: a scenario that draws owns the process's one
`Application` while it plays.

## Errors

- `ScenarioFile::load` and `parse` throw `DocumentError` naming the file and the place.
- `run` never throws for a scenario: a screen that doesn't load is a failure of step 0 (`could not start: ...`), a
  step that can't be played a failure at its step, a golden image that can't be read a picture that differs.

## Limits

What a scenarios file asks for is bounded, since an agent can write one (`src/Internal.hpp`): 1,000 scenarios a file,
10,000 steps a scenario, 100,000 bytes a `type`, a `wait` of 10 minutes of the clock and 1,000,000 timer runs in one,
100 moves a `drag`, scale 1 to 4, a `resize` side of 16,384. Scenario and picture names are file names: letters,
digits, `-`, `_` and `.`, not starting with `.`, 128 at most. Golden images are any file in the folder, so the PNG
reader refuses what isn't an 8-bit RGB or RGBA PNG and pictures over 32 Mi pixels, and never reads past its bytes.
Fuzz targets: `scenarios` and `png` (`tests/fuzz`).

## Extension points

- `Options::prepare` registers what the application adds to the screen (types, native functions) before it is built,
  as the host does.
- `Options::document` plays on another version of the screen (the running one, a proposal applied), keeping the
  screen file's folder for its fonts and includes.

## Files

```
include/affresco/automation/Scenarios.hpp   the public API
src/
  ScenarioFile.cpp     reads and checks a scenarios file
  Names.cpp            key names, state paths, readable properties, file-safe names
  Internal.hpp         what the files share, and the limits
  Run.cpp              run(): a fresh screen per scenario, a headless or hidden window, the report
  Player.hpp, .cpp     plays the steps: pointer, keyboard, revisions, actions, time, pictures
  Expectations.cpp     state paths and node properties
  Pictures.hpp, .cpp   golden images: record, compare, diff, capture
  PngReader.hpp, .cpp  a PNG decoder with its own inflate, for golden images
```

It reaches two internals of other modules on purpose, privately: the render module's headless window and manual
clock (`src/Headless.hpp`, not public yet) and the screen module's PNG encoder and scratch folder (`src/Png.hpp`,
`src/ScratchFolder.hpp`).

## Recipes

Play a file from C++:

```cpp
#include <affresco/automation/Scenarios.hpp>

affresco::automation::Options options;
options.prepare = [](affresco::Screen& screen) { registerMyTypes(screen); };
const auto report = affresco::automation::run(affresco::automation::ScenarioFile::load("app.scenarios.json"), options);
std::cout << report.text(); // "app.scenarios.json: 5 passed, 0 failed"
```

From the command line: `affrescoui test app.scenarios.json [--json] [--only NAME] [--goldens DIR] [--update]
[--out DIR] [--document FILE]` (`tools/affrescoui/README.md`).
