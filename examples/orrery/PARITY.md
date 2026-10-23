# Orrery: parity

Orrery's 3D view is modelled on Celestia, and its sky chart on Stellarium. This file lists what those kinds of app
do, and how far Orrery has come, one round at a time. Everything is in `orrery.screen.json`: no code of its own.

Status: **done**, **part** (there, but less than the model) or **missing**. The data is invented: a toy solar
system of six bodies on circular orbits in one plane.

## The 3D view (modelled on Celestia)

| # | Feature | Status | How |
|---|---|---|---|
| C1 | A 3D view of the bodies and their orbits, drawn in perspective | done | a `scene` node bound to computed `sceneView`: a sphere per body, orbits as rings of thin boxes |
| C2 | A star field behind, turning with the camera | done | 400 small boxes on a far sphere in the scene (computed `starNodes`), seen through the same camera |
| C3 | Turn the camera by dragging, zoom with the wheel | done | `onDrag`, `onScroll` on the view |
| C4 | Select a body by clicking it, the selection ringed | done | the scene's `onSelect` picks a body; the selection outlined |
| C5 | A glow round the Sun | done | an emissive Sun with a point light, and a translucent halo sphere round it |
| C6 | Labels by the bodies | done | a repeated label |
| C7 | Selected body's info, top left: name, distance, radius, apparent size, phase | done | computed `info`: distance, apparent size from the body's radius in the scene and its distance from the camera, phase from the angle Sun–body–camera |
| C8 | Time, top right: date, rate ("1000x faster"), reversed, paused | done | `formatDate` from an invented epoch, computed `rate`; time stops at the window's ends |
| C9 | Time rate up and down (L, K), reverse (J), pause (Space), start (!) | done | screen `keys`; `!` is `shift+1` and resets the rate |
| C10 | Go to the selected body (G), centre it (C), follow it (F) | done | titled actions animating `fly` and the distance; computed `target`; follow moves to a new selection; the wheel or a place stops a flight |
| C11 | Orbits, labels and markers toggled from the keyboard and a menu | part | O and P and the View menu (menu bar and toolbar), which also toggles stars, lit side and glow; no markers |
| C12 | Solar system browser: the bodies as a tree | done | a `tree` from computed `bodyTree` (B, View menu); open a body to go to it |
| C13 | Eclipse finder | missing | |
| C14 | Bookmarks (saved camera places) | done | the browser's Places tab, a `list` of saved places (camera, time, field of view); mod+D or the View menu adds one, the bin deletes one |
| C15 | Field of view and limiting magnitude shown, and changed | part | the field of view, bottom right, narrowed and widened with period and comma; no limiting magnitude |
| C16 | Phase and lit side of each body | done | the scene's point light at the Sun shades every sphere; the lit fraction in the info |
| C17 | Render options dialog | done | the built-in `dialog` with a toggle per layer (orbits, labels, stars, lit side, Sun glow); mod+comma or the View menu |

## The sky chart (modelled on Stellarium)

| # | Feature | Status | How |
|---|---|---|---|
| S1 | A sky chart with stars, orbits and bodies | part | a `canvas` seen from above, not from the ground |
| S2 | Horizon, ground and cardinal points | missing | |
| S3 | Constellation lines and names | missing | |
| S4 | Toggle bar at the bottom (lines, ground, atmosphere, grid) | missing | |
| S5 | Object info on selection, top left | missing | |
| S6 | Time controls: faster, slower, now, real time | part | play, next or previous event (by direction), day 0, the time keys; no real time |
| S7 | Annotations on the chart (notes, marks) | done | the canvas's items |

## Orrery's own

| # | Feature | Status | How |
|---|---|---|---|
| A1 | Orbit graph: what orbits what, relinked by dragging | done | `graph` |
| A2 | Events timeline: eclipses, conjunctions, oppositions | done | `timeline` |
| A3 | Bodies strip (Sinopia's slide sorter, installed from that app) | done | feature, adapter |
| A4 | Thumbnails drawn by the screen | done | `snapshot` |
| A5 | Themes, locales (right to left), accessibility | part | dark theme only so far |

## Rounds

| Round | What | Parity (done / part / missing) | Tokens (screen file, o200k) |
|---|---|---|---|
| 0 | Before: the 3D view and the thumbnails came from a C++ host (`main.cpp`), counted as part | 4 / 10 / 15 | 15,178 + 2,654 for `main.cpp` |
| 1 | JSON only: the 3D view and the thumbnails are in the screen, the host is the standard one | 11 / 3 / 15 | 16,006 |
| 2 | Celestia's overlays: selected body's info and phase, the date and rate, the time keys | 14 / 4 / 11 | 17,519 |
| 3 | Go to, centre, follow; stars turn with the camera; culling behind it; clamped time; orbit and label toggles, View menu | 15 / 5 / 9 | 20,262 |
| 4 | Solar system browser (a tree of the bodies), bookmarks, field of view | 17 / 6 / 6 | 22,685 |
| 5 | Fixes from the round 4 critique: flights land once, moons placed round their planet, reparenting refused into a loop or past three levels, places keep the time and field of view and can be deleted, the view fits a small window | 17 / 6 / 6 | 24,401 |
| 6 | The lit side of every body, the render options dialog (stars, lit side and glow can be hidden) | 19 / 5 / 5 | 25,816 |
| 7 | The 3D view on the scene node (spheres, a point light, picking); fixes from the round 6 critique: the wheel and places stop a flight, follow moves to a new selection, apparent size from the scene | 19 / 5 / 5 | 25,999 |
