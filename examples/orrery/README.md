# Orrery: a small planetarium

Orrery shows a few planets going round a sun, in a 3D view and on a sky chart. It
is one screen document, `lisp/index.screen.lisp` (with the sky from the ground in `sky.lisp`, which it includes), and a
program, `lisp/orrery.lisp`, with no C++ of its own: the standard host runs it.

**Its `lisp/` is a real Lisp program** (RFC 0005; SCREEN_FORMAT.md, "The Lisp language"), because it has behaviour: the camera's
flights (Go to, Centre and Follow share one take-off and one landing), bodies moved in the graph, notes, bookmarks and the
eclipse finder. The one-step actions (the toggles, the time controls) stay data in the screen. `json/` beside it is the same app written in JSON, also by hand: the Launcher's Settings choose which one opens, the same scenarios check both, and `tools/notation/lisp_examples.py` leaves both alone.

It was built by an outsider to the library, as a test of the public documentation. They had only `README.md`,
`SCREEN_FORMAT.md`, `VERSION_CONTROL.md`, the public headers (`modules/*/include/`), the other examples, the
`affrescoui` tool and `mcp/`. It is an example, not part of the library.

```
./build/affrescoui_launcher examples/orrery/lisp/index.screen.lisp [--socket DIR] [--data DIR]
```

The host listens for the person's client and an agent in `DIR` (`studio.sock`, `agent.sock`). Its permissions
apply a `studio` peer's changes and send an agent's to review, which a `studio` peer or the person at Orrery's window
can give.

## What it has

| Part | What it does |
|---|---|
| 3D view | The Sun, Mercury, Venus, the Earth and its Moon, Mars and their orbits, in a `scene` node lit from the Sun, over a star field down to the limiting magnitude. Drag to turn the camera and scroll to zoom. A click selects a body. The selected body's name, distance, radius, apparent diameter and phase are on a card at the bottom left, the date and the time rate in a pill at the top, and the field of view and limiting magnitude bottom right. G goes to the selected body (and follows it), C centres it and F follows it; L, K, J, Space and ! speed time up, slow it down, reverse it, pause it and start it again, and Shift+K runs it at the real rate (the Time menu has them all). The View menu switches to a light theme, and N to the night mode. |
| Browser | B opens the bodies as a tree, the saved places (mod+D bookmarks the view) and the Eclipse finder: the eclipses of the next 420 days. Choosing one stops time, follows the planet and moves to that day. |
| Sky chart | The sky from the ground, from the Earth at 45°N (Shift+Up and Shift+Down move it): the stars, the planets, the Sun and fourteen invented constellations over a low landscape and the cardinal points. Drag to look round and scroll to zoom; by day the sky brightens and the stars fade. Going to an eclipse turns the view to the Sun or the Moon, and a solar eclipse darkens the sky. The bar at the bottom centres the selected body, and shows or hides the constellation lines and names, the grid, the ground, the cardinal points, the atmosphere and the labels. A click selects a planet, and its azimuth, altitude, right ascension and declination are top left. From above, it is a `canvas` with stars, orbits and bodies, and you draw annotations on it: the note button adds a note with the text in the box above in the middle of the view, and the crosshair button circles the selected body and labels it with the day. The selected body's name and orbit are top left. Notes can be moved and resized. Delete removes the selected notes, and the bin button removes them all. |
| Orbit graph | A `graph` of what orbits what. Each body's `orbits` input is linked to its parent's `satellites` output. Drag a link to another body to move a body there, or cut a link and the body orbits the Sun. A click selects the body. |
| Events timeline | A `timeline` of the next 420 days: eclipses, inferior conjunctions and an opposition, one second per day. Scrubbing moves the playhead, and the planets with it. Click an event to jump to it. M by a track mutes that kind of event, and L locks it, so its events no longer answer a click. Play (`mod+p`), next event (`mod+n`) and back to day 0 (`mod+0`) are in the toolbar. |
| Tour strip | Sinopia's slide sorter (`examples/sinopia/lisp/features/slide-sorter`), installed from that app (see below): the Lisp Orrery installs it written as a program (`sorter.lisp`), the JSON one its JSON manifest. Its pages are the saved views, titled "Tour of saved views": four come preset (the whole system, the inner planets, the Earth and Moon, Mars at opposition). Click one to go there, drag to change the tour's order, and the copy button copies the picked view. The play button beside the places (or View, Tour) shows each view for four seconds, one after another. |
| Search | Cmd+F (View, Find) finds a body, which is selected and flown to, or an event, whose day is shown. `@` and a date such as 2030-06-15, or a day number, goes to that day (Time, Go to a day). |
| Location | Shift+Up and Shift+Down (View, Go north and Go south) stand 5° further north or south, between 85°S and 85°N; the sky from the ground turns with it. |
| Night mode | N (View, Night mode) turns everything dim red, to keep the eyes used to the dark: the panels, the 3D scene and its light, the sky, the timeline's clips, the graph's wires and the strip's thumbnails. N again goes back to the theme before. |

The 3D view, the chart's items, the graph's nodes and links and the timeline's tracks are all computed values,
worked out from the same `state.bodies` and `state.time` (in days). The orbits are circles in one plane, with an
invented line of nodes for the Moon that gives two eclipse seasons a year, so the eclipses and conjunctions are those
of this toy model, not of the sky.

## The part from another app

Orrery's file declares what it offers to parts written for other apps:

- **Exports:** `sky.bodies`, a list of bodies, each with a `thumbnail` image. `sky.selected`, the selected body.
  And the actions `bodies.reorder`, `bodies.select` and `bodies.clone`. For the tour: `tour.title`, `tour.views` (the
  saved views, each with the thumbnail of the body it looks at), `tour.current`, and the actions `tour.go`,
  `tour.reorder` and `tour.copy`.
- **Point:** `bodies.dock`, a `dock` slot, which is the strip under the timeline.

`affrescoui contributions examples/orrery/lisp/index.screen.lisp` lists them.

`orrery-sorter.adapter.lisp` binds the slide sorter's needs to those exports:

| The sorter's need | Orrery's export |
|---|---|
| `pages` | `tour.views`, the saved views |
| `current` | `tour.current`, the picked view |
| `title` | `tour.title` |
| `reorder` | `tour.reorder` |
| `select` | `tour.go` |
| `duplicate` | `tour.copy` |

The proof:

```
./build/affrescoui check --compose examples/orrery/lisp/index.screen.lisp examples/sinopia/lisp/features/slide-sorter \
    --adapter examples/orrery/lisp/orrery-sorter.adapter.lisp --revision install.json
```

The proof plays the feature's scenarios and Orrery's own. Its revision is the install record that the shipped
screen's `features` section holds. An agent does the same through the MCP server with `check_composition` and then
`install_feature`, and `e2e_test.py` checks that.

## The thumbnails

Each body's thumbnail is a node of the screen, a shaded disc in the body's colour, with `"snapshot": "body:<id>"`: it
is drawn into that image instead of on the screen, and drawn again when the body changes. The slide sorter's `image`
need gets those names.

## Checks

- `index.scenarios.json`: the day counter, the next event, play, annotations, reparenting, the camera, the night
  mode, the search and the location.
- `controls.scenarios.json`: every control clicked as a person would: the toolbar, the keys, the sky's bar, the
  render options, the browser's tabs, the notes and the timeline's M and L.
- `tour-strip.scenarios.json`: the installed strip. It is in a file of its own because the proof plays
  `index.scenarios.json` before the strip is installed.

```
./build/affrescoui test examples/orrery/lisp/index.scenarios.json examples/orrery/lisp/controls.scenarios.json examples/orrery/lisp/tour-strip.scenarios.json
python3 examples/orrery/lisp/e2e_test.py build    # or: ctest --test-dir build -R mcp_orrery
```

`e2e_test.py` tests the whole agent loop:

1. It starts `affrescoui_launcher` on a copy of the screen without the strip, in a private socket folder.
2. The agent reads the screen and its contributions, proves the slide sorter fits through the adapter, and installs
   it. In this screen an agent's revision is only proposed.
3. The agent gets a picture of the proposal and plays the strip's scenarios on it.
4. A `studio` peer approves the proposal, and the strip is on main.
5. Finally, the test plays Orrery's scenarios on the screen as shipped.

## Assets

There are no binary assets of Orrery's own:

- The fonts are the examples' shared ones (`../shared/fonts`):
  - Roboto, Apache License 2.0;
  - Lucide icons, ISC License, with icons derived from Feather under the MIT License.

  Their sources and licences are in `examples/shared/fonts/README.md`.
- The stars, the orbits and the events are numbers in the screen file.
- The bodies' thumbnails are nodes of the screen, drawn into images.
