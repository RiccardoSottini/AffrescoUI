# Orrery: a small planetarium

Orrery shows a few planets going round a sun. It is one screen document, `orrery.screen.json`, with a small host,
`main.cpp`, that adds only the two things a file can't: a 3D view drawn by the `scene3d` module, and a picture of
each body.

It was built by an outsider to the library, as a test of the public documentation. They had only `README.md`,
`SCREEN_FORMAT.md`, `VERSION_CONTROL.md`, the public headers (`modules/*/include/`), the other examples, the
`affrescoui` tool and `mcp/`. It is an example, not part of the library.

```
./build/affrescoui_orrery [screen.json] [--socket DIR] [--data DIR]
```

Like `affrescoui_host`, it listens for the Studio and an agent in `DIR` (`studio.sock`, `agent.sock`).

## What it has

| Part | What it does |
|---|---|
| 3D view | The host's `scene` surface (`makeSceneRenderer`). It draws the Sun, Mercury, Venus, the Earth and its Moon, Mars and their orbits, lit by a point light in the Sun. Drag to turn the camera and scroll to zoom. The selected body is outlined. |
| Sky chart | A `canvas` seen from above, with stars, orbits and bodies. You draw annotations on it: the note button adds a note with the text in the box above in the middle of the view, and the crosshair button circles the selected body and labels it with the day. Notes can be moved and resized. Delete removes the selected notes, and the bin button removes them all. |
| Orbit graph | A `graph` of what orbits what. Each body's `orbits` input is linked to its parent's `satellites` output. Drag a link to another body to move a body there, or cut a link and the body orbits the Sun. A click selects the body. |
| Events timeline | A `timeline` of the next 420 days: eclipses, inferior conjunctions and an opposition, one second per day. Scrubbing moves the playhead, and the planets with it. Click an event to jump to it. Play (`mod+p`), next event (`mod+n`) and back to day 0 (`mod+0`) are in the toolbar. |
| Bodies strip | Sinopia's slide sorter (`examples/sinopia/features/slide-sorter`), installed from that app (see below). Click a body to select it everywhere. Drag to reorder the bodies, and duplicate adds a twin on the same orbit, half a turn away. |

The 3D scene, the chart's items, the graph's nodes and links and the timeline's tracks are all computed values,
worked out from the same `state.bodies` and `state.time` (in days). Expressions have no `sin` or `cos`, so the
positions come from a cosine table read with `curve()` (`state.cos`, twelve steps, interpolated). The orbits are
circles in one plane, so the eclipses and conjunctions are those of this toy model, not of the sky.

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

## The host

`main.cpp` does three things:

1. It registers the `scene` surface, with a sphere mesh and a thin torus for the orbits.
2. It registers a native call, `orrery.scene`, which the screen's `onLoad` makes. The 3D view is shown only when the
   call answers `true`. The `affrescoui` tool has no `scene` surface, and the call fails there, so the screen shows a
   note instead and its scenarios, proofs and pictures still work without the host.
3. It draws a thumbnail for each body in the file's `state.bodies`, a shaded disc in the body's colour, and gives it
   to the screen as the image `body:<id>`. The slide sorter's `image` need wants an image the host names.

## Checks

- `orrery.scenarios.json`: the day counter, the next event, play, annotations, reparenting and the camera.
- `bodies-strip.scenarios.json`: the installed strip. It is in a file of its own because the proof plays
  `orrery.scenarios.json` before the strip is installed.

```
./build/affrescoui test examples/orrery/orrery.scenarios.json examples/orrery/bodies-strip.scenarios.json
python3 examples/orrery/e2e_test.py build    # or: ctest --test-dir build -R mcp_orrery
```

`e2e_test.py` tests the whole agent loop:

1. It starts `affrescoui_orrery` on a copy of the screen without the strip, in a private socket folder.
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
- The bodies' thumbnails are drawn by `main.cpp` when it starts.
