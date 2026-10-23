# Orrery: a small planetarium

Orrery shows a few planets going round a sun, modelled on Celestia (the 3D view) and Stellarium (the sky chart). It
is one screen document, `orrery.screen.json` (with the sky from the ground in `sky.json`, which it includes), with no
code of its own: the standard host runs it.

It was built by an outsider to the library, as a test of the public documentation. They had only `README.md`,
`SCREEN_FORMAT.md`, `VERSION_CONTROL.md`, the public headers (`modules/*/include/`), the other examples, the
`affrescoui` tool and `mcp/`. It is an example, not part of the library.

```
./build/affrescoui_host examples/orrery/orrery.screen.json [--socket DIR] [--data DIR]
```

The host listens for the Studio and an agent in `DIR` (`studio.sock`, `agent.sock`). Its permissions
apply the Studio's changes and send an agent's to review, which the Studio or the person at Orrery's window can give.

## What it has

| Part | What it does |
|---|---|
| 3D view | The Sun, Mercury, Venus, the Earth and its Moon, Mars and their orbits, in a `scene` node lit from the Sun, over a star field down to the limiting magnitude. Drag to turn the camera and scroll to zoom. A click selects a body. The selected body's name, distance, radius, apparent diameter and phase are top left, the date and the time rate top right. G goes to the selected body (and follows it), C centres it and F follows it; L, K, J, Space and ! speed time up, slow it down, reverse it, pause it and start it again, and Shift+K runs it at the real rate (the Time menu has them all). The View menu switches to a light theme. |
| Browser | B opens the bodies as a tree, the saved places (mod+D bookmarks the view) and the Eclipse finder: the eclipses of the next 420 days. Choosing one stops time, follows the planet and moves to that day. |
| Sky chart | The sky from the ground, from the Earth at 45°N: the stars, the planets, the Sun and fourteen invented constellations over a low landscape and the cardinal points. Drag to look round and scroll to zoom; by day the sky brightens and the stars fade. Going to an eclipse turns the view to the Sun or the Moon, and a solar eclipse darkens the sky. The bar at the bottom centres the selected body, and shows or hides the constellation lines and names, the grid, the ground, the cardinal points, the atmosphere and the labels. A click selects a planet, and its azimuth, altitude, right ascension and declination are top left. From above, it is a `canvas` with stars, orbits and bodies, and you draw annotations on it: the note button adds a note with the text in the box above in the middle of the view, and the crosshair button circles the selected body and labels it with the day. The selected body's name and orbit are top left. Notes can be moved and resized. Delete removes the selected notes, and the bin button removes them all. |
| Orbit graph | A `graph` of what orbits what. Each body's `orbits` input is linked to its parent's `satellites` output. Drag a link to another body to move a body there, or cut a link and the body orbits the Sun. A click selects the body. |
| Events timeline | A `timeline` of the next 420 days: eclipses, inferior conjunctions and an opposition, one second per day. Scrubbing moves the playhead, and the planets with it. Click an event to jump to it. Play (`mod+p`), next event (`mod+n`) and back to day 0 (`mod+0`) are in the toolbar. |
| Bodies strip | Sinopia's slide sorter (`examples/sinopia/features/slide-sorter`), installed from that app (see below). Click a body to select it everywhere. Drag to reorder the bodies, and duplicate adds a twin on the same orbit, half a turn away. |

The 3D view, the chart's items, the graph's nodes and links and the timeline's tracks are all computed values,
worked out from the same `state.bodies` and `state.time` (in days). The orbits are circles in one plane, with an
invented line of nodes for the Moon that gives two eclipse seasons a year, so the eclipses and conjunctions are those
of this toy model, not of the sky.

## The part from another app

Orrery's file declares what it offers to parts written for other apps:

- **Exports:** `sky.bodies`, a list of bodies, each with a `thumbnail` image. `sky.selected`, the selected body.
  And the actions `bodies.reorder`, `bodies.select` and `bodies.clone`.
- **Point:** `bodies.dock`, a `dock` slot, which is the strip under the timeline.

`affrescoui contributions examples/orrery/orrery.screen.json` lists them.

`orrery-sorter.adapter.json` binds the slide sorter's needs to those exports:

| The sorter's need | Orrery's export |
|---|---|
| `pages` | Every body except the Sun |
| `current` | The selected body |
| `reorder` | `bodies.reorder` |
| `select` | `bodies.select` |
| `duplicate` | `bodies.clone` |

The proof:

```
./build/affrescoui check --compose examples/orrery/orrery.screen.json examples/sinopia/features/slide-sorter \
    --adapter examples/orrery/orrery-sorter.adapter.json --revision install.json
```

The proof plays the feature's scenarios and Orrery's own. Its revision is the install record that the shipped
screen's `features` section holds. An agent does the same through the MCP server with `check_composition` and then
`install_feature`, and `e2e_test.py` checks that.

## The thumbnails

Each body's thumbnail is a node of the screen, a shaded disc in the body's colour, with `"snapshot": "body:<id>"`: it
is drawn into that image instead of on the screen, and drawn again when the body changes. The slide sorter's `image`
need gets those names.

## Checks

- `orrery.scenarios.json`: the day counter, the next event, play, annotations, reparenting and the camera.
- `bodies-strip.scenarios.json`: the installed strip. It is in a file of its own because the proof plays
  `orrery.scenarios.json` before the strip is installed.

```
./build/affrescoui test examples/orrery/orrery.scenarios.json examples/orrery/bodies-strip.scenarios.json
python3 examples/orrery/e2e_test.py build    # or: ctest --test-dir build -R mcp_orrery
```

`e2e_test.py` tests the whole agent loop:

1. It starts `affrescoui_host` on a copy of the screen without the strip, in a private socket folder.
2. The agent reads the screen and its contributions, proves the slide sorter fits through the adapter, and installs
   it. In this screen an agent's revision is only proposed.
3. The agent gets a picture of the proposal and plays the strip's scenarios on it.
4. The Studio approves the proposal, and the strip is on main.
5. Finally, the test plays Orrery's scenarios on the screen as shipped.

## Assets

There are no binary assets of Orrery's own:

- The fonts are the examples' shared ones (`../shared/fonts`):
  - Roboto, Apache License 2.0;
  - Lucide icons, ISC License, with icons derived from Feather under the MIT License.

  Their sources and licences are in `examples/shared/fonts/README.md`.
- The stars, the orbits and the events are numbers in the screen file.
- The bodies' thumbnails are nodes of the screen, drawn into images.
