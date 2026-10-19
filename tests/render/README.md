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

Frames don't depend on the machine or on what ran before. The window is hidden and only gets the events the steps
send. Captures are drawn offscreen at the scenario's `scale` (1 by default; `kanban-retina` uses 2, the Retina path).
Screens load without a state file, and channels find no host.

## Scenarios

`scenarios.json` lists screens (from `examples/assets/screens/`) and their steps. Targets are `[x, y]` in window
units, or `"#id"`, the centre of the first shown panel with that id (`"nth": 2` picks the third). Screens made only
for the check, for what no template shows, are in `screens/` here (e.g. `unicode`: scripts, fallback fonts, editing
by code point), named by a path relative to the templates' folder.

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
