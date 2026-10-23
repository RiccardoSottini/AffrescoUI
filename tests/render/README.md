# Rendering check

Plays scripted scenarios on the example screens and compares every picture they expect, pixel for pixel, with the
golden images in `golden/`. If a change to the library alters what any screen draws, this catches it.

The golden images are not in git: the first run records them from the code you have. Record them before you change
anything (a clean checkout, then `check.py`), and the runs after compare against that.

```sh
cmake --build build                          # also builds affrescoui_render_runner
python3 tests/render/check.py                # or: ctest --test-dir build
python3 tests/render/check.py --only kanban  # one scenario
python3 tests/render/check.py --update       # rewrite the golden images (only for an intended change)
```

A frame that differs is saved to `build/render-check/` next to a `.diff.png`, with the changed pixels in red.

Captures are drawn the way the window draws its frames: from each panel's retained geometry, only where something
changed since the last capture. Each picture is also drawn from scratch, and the scenario fails on any pixel that
differs ("stale pixels", with the frame drawn from scratch saved as `<picture>.scratch.png` in the output folder),
whatever the golden image says. The `damage` scenario (`scenarios/damage.json`) moves, overlaps, hides, pushes and
scrolls panels through such partial frames.

The scenarios are the format's own (SCREEN_FORMAT.md, "Scenarios"), played by the automation module
(`modules/automation`): `affrescoui_render_runner` plays the files and prints a line per picture (`same`, `golden` when
it recorded one, `FAILED`), then `N frames, M failed`; `check.py` gives it the files, the golden images and the output
folder. Frames don't depend on the machine or on what ran before. The window is hidden and only gets the events the
steps send, and its clock moves only when a step waits. Pictures are drawn offscreen at the scenario's `scale` (1 by
default; `kanban-retina` uses 2, the Retina path). Screens load without a state file, and channels find no host.

The templates' own scenarios (`examples/<folder>/*.scenarios.json`, the folders of `examples/templates.json`) check
behaviour, not pictures: ctest `scenarios` plays them headless with the same runner.

## Scenarios

The scenarios are in `scenarios/`, one file per area: `kanban.json`, `code-editor.json`, `studio.json`, and so on.
`check.py` passes every `.json` there in the order of the file names, so a new file needs no other edit, and parallel
changes that add scenarios to different areas don't touch the same file. A scenario's name must be unique across the
files, because it is the prefix of its pictures' names.

Each file names a `screens` folder, relative to the file, and lists its scenarios: a name, a `screen` in that folder,
an optional `scale` and `note`, and the steps. A picture is an expectation, named after the scenario:

```json
{
	"format": "affrescoui-scenarios",
	"version": 2,
	"screens": "../../../examples/studio",
	"scenarios": [
		{ "name": "studio", "screen": "index.screen.json", "steps": [{ "expect": { "image": "first-frame" } }] }
	]
}
```

The steps (click, type, key, drag, scroll, wait, revision, action, resize, expect) are in SCREEN_FORMAT.md,
"Scenarios". Screens made only for the check, for what no template shows, are in `screens/` here (e.g. `unicode`:
scripts, fallback fonts, editing by code point; `scripts`: shaping, with Arabic, Devanagari, Chinese, Japanese,
Korean, colour emoji, ligatures and a caret inside a code ligature); their scenarios are in a file whose `screens`
is `"../screens"`. The fonts only these screens use are small subsets of open fonts, in `fonts/` (its README
says where each comes from).

**Where a new scenario goes:** in the file of the area it shows (a template's file for a template, `shapes.json` for
shapes), or in a new `<area>.json` for a new area. Open the pictures its first run records and check that they show
what you expect. A `wait` moves the clock, so what it animates is at its end when the picture is drawn.
