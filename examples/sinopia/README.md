# Sinopia, slides and vector documents

Sinopia makes slide decks (16:9 and 4:3) and vector pages (A4, Letter, custom), presents them and exports them. Its
whole workspace is a screen document, [`sinopia.screen.json`](sinopia.screen.json), with its parts in
[`parts/`](parts): people and agents change it live like any other screen. Its permissions apply the Studio's changes
and send an agent's to review, which only the Studio can give. The C++ beside it is the deck model and the few natives
a screen can't do itself: reading and checking decks, drawing thumbnails, exporting, and the audience window.

```sh
cmake --build build --target affrescoui_sinopia
./build/affrescoui_sinopia --socket out/s --data out/data   # the workspace, with decks/sample.sinopia.json
./build/affrescoui_sinopia --check examples/sinopia/sinopia.scenarios.json   # the scenarios, headless (ctest: sinopia)
./build/affrescoui_sinopia --check examples/sinopia/sinopia.scenarios.json --out out/shots   # and their captures
```

## The workspace

| Part | What it does |
|---|---|
| Toolbar | Add a rectangle, ellipse, polygon, star, line, arrow, pen path, text frame or image; new, duplicate and delete slide; undo; save; sorter; theme; export PDF and PNG; Present |
| Slide list and outline | Thumbnails with their sections (the sorter's `slideList`), or the deck's text as an outline |
| Canvas | The slide on a `canvas`: select, move, resize and rotate, with smart guides, snapping to the grid and to other objects, and rulers |
| Inspector | With objects selected: fill (solid or gradient), stroke, corners, rotation, shadow, rich text, align and distribute, z-order, group, lock and the object's build. With none: the slide's transition, skip, section and builds, and the deck's size |
| Notes | The slide's speaker notes |
| Sorter | Every slide as a thumbnail in a grid: drag to reorder |
| Presenter | The audience's frame, the next slide, the notes, a timer and the controls; the audience window shows the same frame |

Keys while editing: `mod+s` save, `mod+z` undo, `mod+d` duplicate, Backspace and Delete delete, `mod+g` and
`mod+shift+g` group and ungroup, `mod+shift+f` and `mod+shift+b` bring to front and send to back, `mod+l` and
`mod+shift+l` lock and unlock, `mod+shift+n` new slide, `mod+alt+p` present, `mod+e` export PDF; the canvas's own
arrow keys nudge. While presenting: Right, Down and Space go on, Left and Up go back, B blacks the screen and Escape ends.

## Decks

A deck is a JSON file in [`decks/`](decks) (`*.sinopia.json`): a title, a size and page, theme tokens, masters and
layouts (a background and objects under the slides that use them, placeholders among them), sections and slides.
A slide has its notes, background, transition (`none`, `fade`, `push`, `wipe` or `zoom`, with a duration) and objects,
back to front. An object is a `rect`, `ellipse`, `polygon`, `star`, `line`, `arrow`, `path`, `text`, `image` or
`group`, with its frame, rotation, fill or gradient, stroke, corner radius, shadow, rich text (runs, with autofit or
overflow), lock and build (`appear`, `fade` or `fly`, in order). [`deck/Deck.hpp`](deck/Deck.hpp) has every key; `readDeck` checks every
value against `DeckLimits` (SECURITY.md, "Limits"), and every edit is read again, so a deck never gets past them.

## Natives

| Call | Does |
|---|---|
| `sinopia.load` (`file`) | Reads a deck from `decks/` |
| `sinopia.save` (`name`, `deck`) | Writes it to `decks/<name>.sinopia.json` |
| `sinopia.render` (`deck`) | The deck as the screen draws it: masters and layouts resolved, text fitted, pages sized |
| `sinopia.edit` (`deck`, `slide`, `op`, `ids`, ...) | One edit ([`deck/Edit.hpp`](deck/Edit.hpp) lists them): it gives back the deck, the slide and the selection |
| `sinopia.thumbnails` (`render`) | Draws each changed slide into its host image `thumb-<id>` |
| `sinopia.exportPdf`, `sinopia.exportPng` (`render`) | The shown slides as one vector PDF (`modules/export`) or a PNG each, 1920 pixels wide |
| `sinopia.openAudience`, `sinopia.closeAudience` | The audience window: a second window sharing the workspace's GL context |
| `sinopia.mirror` (`render`, `index`, `step`, `t`, `black`) | Draws the presented frame, with the transition at `t`, into the host image `present` |

The slides are drawn once more on a stage behind the workspace (`slidePage` in [`parts/objects.json`](parts/objects.json)),
at the deck's size: the thumbnails, the presented frames and the exports render those panels.

## The slide sorter, reusable

[`parts/sorter.json`](parts/sorter.json) is a part any app can include for a list or grid of pages. It knows nothing
about decks: it needs a list of `{ id, index, skip, section }`, two action names, and host images named `thumb-<id>`
that the app draws however it likes (Sinopia renders each slide's panel to an image and keeps it until the slide
changes).

```json
"include": ["parts/sorter.json"],
...
{ "use": "slideList", "with": { "slides": "{{computed.pages}}", "current": "{{state.page}}", "sections": [],
  "pick": "pickPage", "move": "movePage", "width": 160, "prefix": "pages" } }
```

| Component | Params |
|---|---|
| `slideList` | `slides`, `current`, `sections` (`{ id, title }`, for the headers), `pick`, `move`, `width`, `prefix`: a vertical list with section headers |
| `slideSorter` | `slides`, `current`, `pick`, `move`, `width`, `prefix`: a wrapping grid |
| `slideThumb` | `slide`, `selected`, `width`, `pick`, `move`, `slides`, `prefix`: one thumbnail, used by both |

`pick` runs with `{ index }` when a thumbnail is clicked, and `move` with `{ from, to }` when one is dropped on
another. A skipped page is dimmed and marked. Ids are `<prefix>-<id>`, so two sorters can share a screen.

## The code

| Where | What |
|---|---|
| [`main.cpp`](main.cpp) | The command line: the workspace, or `--check` to play the scenarios |
| [`Natives.cpp`](Natives.cpp), [`Present.cpp`](Present.cpp) | The natives: decks, thumbnails and exports; the audience window and its frames |
| [`deck/`](deck) | The deck model, no window needed: reading and writing a deck within its limits, the edits, and aligning, distributing and grouping |
| [`drawing/`](drawing) | Drawing: the `sinopiaShape` type and its outlines, a deck as panels, and the slide transitions |
