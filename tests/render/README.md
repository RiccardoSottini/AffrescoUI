# Rendering check

Plays scripted scenarios on the example screens and compares every captured frame, pixel for pixel, with the golden
images in `golden/`. If a change to the library alters what any screen draws, this catches it.

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
changed since the last capture. The runner also draws each one from scratch, and fails the scenario on any pixel that
differs ("stale pixels", with the frame drawn from scratch saved as `<frame>.scratch.ppm`), whatever the golden image
says. The `damage` scenario (`scenarios/damage.json`) moves, overlaps, hides, pushes and scrolls panels through such
partial frames.

Frames don't depend on the machine or on what ran before. The window is hidden and only gets the events the steps
send. Captures are drawn offscreen at the scenario's `scale` (1 by default; `kanban-retina` uses 2, the Retina path).
Screens load without a state file, and channels find no host.

## Scenarios

The scenarios are in `scenarios/`, one file per area: `kanban.json`, `code-editor.json`, `studio.json`, and so on.
`check.py` reads every `.json` there in the order of the file names, so a new file needs no other edit, and parallel
changes that add scenarios to different areas don't touch the same file. A scenario's name must be unique across the
files, because it is the prefix of its frames' names.

Each file names a `screens` folder, relative to the file, and lists its scenarios: a name, a `screen` in that folder,
an optional `scale` and `note`, and the steps:

```json
{
	"screens": "../../../examples/assets/screens",
	"scenarios": [
		{ "name": "studio", "screen": "studio.screen.json", "steps": [{ "capture": "first-frame" }] }
	]
}
```

Targets are `[x, y]` in window units, or `"#id"`, the centre of the first shown panel with that id (`"nth": 2` picks
the third). Screens made only for the check, for what no template shows, are in `screens/` here (e.g. `unicode`:
scripts, fallback fonts, editing by code point; `scripts`: shaping, with Arabic, Devanagari, Chinese, Japanese,
Korean, colour emoji, ligatures and a caret inside a code ligature); their scenarios are in a file whose `screens`
is `"../screens"`. The fonts only these screens use are small subsets of open fonts, in `fonts/` (its README
says where each comes from).

**Where a new scenario goes:** in the file of the area it shows (a template's file for a template, `shapes.json` for
shapes), or in a new `<area>.json` for a new area. Open the frames its first run records and check that they show
what you expect.

| Step | Does |
|---|---|
| `{ "capture": "name" }` | Saves the frame as `<scenario>-<name>` |
| `{ "move": target }` | Moves the pointer (hover) |
| `{ "click": target }` | Left press and release |
| `{ "press": target }`, `{ "release": target }` | One half of a click, for drags (with `move` between) |
| `{ "scroll": target, "dy": -3 }` | Scrolls there (`dx` too) |
| `{ "type": "text" }` | Types ASCII characters into the focused panel |
| `{ "key": "enter" }` | Presses a key: `enter escape tab backspace delete space left right up down home end slash a`–`z`, with `shift+ ctrl+ alt+ cmd+` |
| `{ "revision": { ... } }` | Applies a revision, as the app would |
| `{ "action": "name", "event": { ... } }` | Runs one of the screen's actions |
| `{ "resize": [w, h] }` | Resizes the window |
| `{ "wait": ms }` | Runs timers for real (tooltips, `every`, animations). Capture only once what they animate has finished |
