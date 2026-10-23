# Orrery: parity

Orrery has a 3D view of space and a sky chart from the ground. This file lists what those kinds of app
do, and how far Orrery has come, one round at a time. Everything is in `orrery.screen.json` and the sky from the ground in `sky.json`, which it includes: no code of its own.

Status: **done**, **part** (there, but less than the model) or **missing**. The data is invented: a toy solar
system of six bodies on circular orbits in one plane.

## The 3D view

| # | Feature | Status | How |
|---|---|---|---|
| C1 | A 3D view of the bodies and their orbits, drawn in perspective | done | a `scene` node bound to computed `sceneView`: a sphere per body, orbits as rings of thin boxes |
| C2 | A star field behind, turning with the camera | done | small boxes on a far sphere in the scene (computed `starNodes`, as many as the limiting magnitude lets through, out of 400), seen through the same camera |
| C3 | Turn the camera by dragging, zoom with the wheel | done | `onDrag`, `onScroll` on the view |
| C4 | Select a body by clicking it, the selection ringed | done | the scene's `onSelect` picks a body; the selection outlined |
| C5 | A glow round the Sun | done | an emissive Sun with a point light, and a translucent halo sphere round it |
| C6 | Labels by the bodies | done | a repeated label |
| C7 | Selected body's info, top left: name, distance, radius, apparent size, phase | done | computed `info`: distance, apparent size from the body's radius in the scene and its distance from the camera, phase from the angle Sun–body–camera |
| C8 | Time, top right: date, rate ("1000x faster"), reversed, paused | done | `formatDate` from an invented epoch, computed `rate`; time stops at the window's ends |
| C9 | Time rate up and down (L, K), reverse (J), pause (Space), start (!) | done | screen `keys`; `!` is `shift+1` and resets the rate |
| C10 | Go to the selected body (G), centre it (C), follow it (F) | done | titled actions animating `fly` and the distance; computed `target`; a flight ends following the body, and F follows (the toolbar toggle stops); follow moves to a new selection; the wheel or a place stops a flight |
| C11 | Orbits, labels and markers toggled from the keyboard and a menu | done | O and P, the View menu (its items checked when on) and the built-in `toolbar` (toggles shown pressed); the menu also toggles stars, lit side and glow; M marks or unmarks the selected body (four ticks round it) |
| C12 | Solar system browser: the bodies as a tree | done | a `tree` from computed `bodyTree` (B, View menu); open a body to go to it |
| C13 | Eclipse finder | done | the browser's Eclipses tab (View menu), a `list` from computed `eclipses`, each row with its start (UTC), length and kind: every new and full moon that falls in an eclipse season (the Sun near the moon's line of nodes, from the moon's invented `nodeDay`, `eclipseYear` and `eclipseSeason`); choosing one stops time, selects and follows the planet and moves to the day. The timeline's eclipses come from the same list |
| C14 | Bookmarks (saved camera places) | done | the browser's Places tab, a `list` of saved places (camera, time, field of view); mod+D or the View menu adds one, the bin deletes one |
| C15 | Field of view and limiting magnitude shown, and changed | done | bottom right: the field of view, narrowed and widened with period and comma; the limiting magnitude and the stars it shows, raised and lowered with equal and minus (the stars have invented magnitudes, brighter ones drawn larger) |
| C16 | Phase and lit side of each body | done | the scene's point light at the Sun shades every sphere; the lit fraction in the info |
| C17 | Render options dialog | done | the built-in `dialog` with a toggle per layer (orbits, labels, stars, lit side, Sun glow); mod+comma or the View menu |

## The sky chart

| # | Feature | Status | How |
|---|---|---|---|
| S1 | A sky chart with stars, orbits and bodies | done | from the ground (the default): 1,200 stars in three tints, the planets and the Sun seen from the Earth at 45°N, in a perspective view turned by dragging and zoomed with the wheel, every position a computed value from right ascension, declination and the sidereal time; from above: a `canvas` with the orbits |
| S2 | Horizon, ground and cardinal points | done | a landscape of low hills (a `path` from the horizon's profile), N, E, S and W on it, an azimuthal grid labelled in degrees; the sky brightens through a twilight and the stars fade while the Sun is up (time starts at 17,280x, a day in five seconds), and darkens again while the Moon covers it; the place, field of view and date bottom right |
| S3 | Constellation lines and names | done | fourteen invented constellations of the brighter stars, their lines in a `path`, their names at their centres |
| S4 | Toggle bar at the bottom (lines, ground, atmosphere, grid) | done | the built-in `toolbar`, bottom left: centre on the selected body (and keep it there), lines, names, grid, ground, cardinal points, atmosphere, planet labels; the place and field of view bottom right |
| S5 | Object info on selection, top left | done | the selected body ringed; its name, and its azimuth and altitude, right ascension and declination from the ground, or its distance from the Sun and orbit from above |
| S6 | Time controls: faster, slower, now, real time | done | play, next or previous event (by direction), day 0, the time keys, real time (Shift+K, the toolbar, the Time menu: 10x faster and slower, freeze, real time, reverse, back to day 0, eclipses); the invented calendar has no "now" of its own |
| S7 | Annotations on the chart (notes, marks) | done | the canvas's items |

## Orrery's own

| # | Feature | Status | How |
|---|---|---|---|
| A1 | Orbit graph: what orbits what, relinked by dragging | done | `graph` |
| A2 | Events timeline: eclipses, conjunctions, oppositions | done | `timeline`, a track per kind; muting a track skips that kind in next and previous event |
| A3 | Bodies strip (Sinopia's slide sorter, installed from that app) | done | feature, adapter |
| A4 | Thumbnails drawn by the screen | done | `snapshot` |
| A5 | Themes, locales (right to left), accessibility | part | dark and light themes (View menu), the views of space dark in both; one locale |

## Rounds

| Round | What | Parity (done / part / missing) | Tokens (screen file, o200k) |
|---|---|---|---|
| 0 | Before: the 3D view and the thumbnails came from a C++ host (`main.cpp`), counted as part | 4 / 10 / 15 | 15,178 + 2,654 for `main.cpp` |
| 1 | JSON only: the 3D view and the thumbnails are in the screen, the host is the standard one | 11 / 3 / 15 | 16,006 |
| 2 | The 3D view's overlays: selected body's info and phase, the date and rate, the time keys | 14 / 4 / 11 | 17,519 |
| 3 | Go to, centre, follow; stars turn with the camera; culling behind it; clamped time; orbit and label toggles, View menu | 15 / 5 / 9 | 20,262 |
| 4 | Solar system browser (a tree of the bodies), bookmarks, field of view | 17 / 6 / 6 | 22,685 |
| 5 | Fixes from the round 4 critique: flights land once, moons placed round their planet, reparenting refused into a loop or past three levels, places keep the time and field of view and can be deleted, the view fits a small window | 17 / 6 / 6 | 24,401 |
| 6 | The lit side of every body, the render options dialog (stars, lit side and glow can be hidden) | 19 / 5 / 5 | 25,816 |
| 7 | The 3D view on the scene node (spheres, a point light, picking); fixes from the round 6 critique: the wheel and places stop a flight, follow moves to a new selection, apparent size from the scene | 19 / 5 / 5 | 25,999 |
| 8 | Markers (M, View menu); the limiting magnitude, shown and changed with equal and minus, the stars sized by magnitude | 21 / 3 / 5 | 27,039 |
| 9 | The built-in menu bar, toolbar and status bar in place of hand-made ones; muting a kind of event; fixes from the round 8 critique: a body moved under a planet orbits close to it and goes back to its old orbit, spheres sized without the labels' clamp, nothing marked without a selection | 21 / 3 / 5 | 27,556 |
| 10 | Eclipse finder (eclipses computed from the orbits and eclipse seasons, the timeline's eclipses with them); the selected body's info on the sky chart | 23 / 3 / 3 | 28,846 |
| 11 | The sky from the ground: horizon, ground, cardinal points, grid, daylight, invented constellations, the toggle bar | 27 / 2 / 0 | 29,306 + 5,334 for `sky.json` |
| 12 | Fixes from the round 11 critique: an eclipse turns the sky to the Sun or the Moon and the sky darkens under a solar eclipse, a landscape instead of a flat ground, more stars in three tints, the date under the sky, centring on a body, days with their decimal | 27 / 2 / 0 | 29,368 + 6,800 for `sky.json` |
| 13 | Real time and a Time menu; a light theme | 28 / 1 / 0 | 30,114 + 6,809 for `sky.json` |
| 14 | Fixes from the round 11 critique still open: grid labels, the eclipse finder's start, length and kind per row, larger names from above, moon labels clear of their planet | 28 / 1 / 0 | 30,342 + 7,146 for `sky.json` |
| 15 | Fixes from the round 14 critique: F after G keeps following, the grid's altitude labels, the Time menu in the native menu bar, a default rate that shows day and night, seconds on the clock, a real-time icon, a twilight and a brighter ground by day, the selection ringed in the sky | 28 / 1 / 0 | 30,455 + 7,213 for `sky.json` |
| 16 | The assistant add-on (one include line, in Orrery's colours) | 28 / 1 / 0 | 30,588 + 7,213 for `sky.json` |
