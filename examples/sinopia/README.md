# Sinopia, slides and vector documents

Sinopia makes slide decks, presents them and exports them. It is a folder of JSON that the stock
`affrescoui_launcher` runs, with nothing compiled for it: [`index.screen.json`](index.screen.json) and its parts in
[`parts/`](parts). People and agents change it live like any other screen. Its permissions apply a `studio` peer's
changes (the person's client) and send an agent's to review, which only a `studio` peer can give.

```sh
./build/affrescoui_launcher examples/sinopia/index.screen.json --socket out/s   # the workspace, with the sample deck
./build/affrescoui test examples/sinopia/index.scenarios.json                   # the 41 scenarios (ctest: scenarios)
```

## The workspace

| Part | What it does |
|---|---|
| Ribbon | File (open, save, export PDF and PNG), Home (paste and copy menus, Copy Style, slides, layouts and sections; font, size, case, colour and highlight grids; bullets, numbering, list levels, line spacing, columns, text direction and anchor; shapes, quick styles, fill and outline grids, effects; Arrange: order, align, distribute; Find, Replace, Select), Insert, Design (themes and variants), Transitions, Motion (builds), Present, View (views, panes, dark mode) |
| Slide list | Each slide's picture (a snapshot of its page), its number and its section, a star when it has a transition or builds, dimmed when hidden; the arrows move through it |
| Canvas | The slide on a `canvas`: select, move, resize, with the zoom and fit of the status bar |
| Format pane | With objects selected: fill and line from the theme's slots, line width, size, place, rotation and lock. With none: the slide's background |
| Notes | The slide's speaker notes, under the slide; the status bar hides and shows them |
| Outline and sorter | The deck's text slide by slide, or every slide in a grid, dragged to reorder |
| Slide show | Full screen, or in a window of its own beside a presenter view: the slide, the next one, the notes (larger or smaller), a timer and the controls |

Keys while editing: `mod+f` find, `mod+h` replace, `mod+s` save, `mod+o` open, `mod+z` and `mod+y` undo and redo, `mod+m` new slide, `mod+d`
duplicate, `mod+c`, `mod+x` and `mod+v`, `mod+a` select all, `mod+b`, `mod+i` and `mod+u`, `mod+l`, `mod+e`, `mod+r`
and `mod+j` align the text, the arrow keys nudge (Shift: 10), Delete removes (a locked object stays), F5 presents from the start and Shift+F5 from this slide.
While presenting: Right, Down, Space, Enter and N go on, Left, Up and P go back, Home and End jump, B and W blank the
screen black or white, and Escape ends.

## The deck

The deck is the state `deck` (the sample is [`parts/sample-deck.json`](parts/sample-deck.json)): a title, a size,
a theme and variant, sections and slides. A slide has a layout, notes, a background, a transition (`fade`, `push`,
`wipe`, `cover`, `glide` and more, with a duration and a direction), builds (`appear`, `fade`, `wipe`, `fly` or `zoom`,
as an entrance or an exit, on a click, with or after the one before) and objects, back to front: text, shapes from
[`parts/geometry.json`](parts/geometry.json)'s presets, and lines. Saved, it is that JSON; opened, it is checked to
be a deck before it replaces the one on screen, as a change undo takes back.

## The parts

| Part | What |
|---|---|
| [`themes.json`](parts/themes.json) | The themes, their colour slots and variants, the layouts and their placeholders |
| [`geometry.json`](parts/geometry.json), [`slide.json`](parts/slide.json) | Shapes as paths, and a slide as nodes: its background and objects |
| [`edit.json`](parts/edit.json) | Every edit, each one undoable: slides, objects, text, arrange, themes, transitions and builds |
| [`ribbon.json`](parts/ribbon.json), [`tabs.json`](parts/tabs.json) | The ribbon's buttons, galleries and groups, and its tabs |
| [`show.json`](parts/show.json) | The slide show: the `audience` window, transitions and builds through `animate`, the presenter view |
| [`views.json`](parts/views.json) | The outline, the sorter, the format pane, and the deck as a file (`saveFile`, `openFile`, `export`) |
| [`menus.json`](parts/menus.json) | The ribbon's drop-down menus and colour grids, Copy Style, and the find box (find, replace, rename a section) |
| [`themes/sinopia.json`](themes/sinopia.json) | The app's own look: the window's colour tokens, light and dark |

The slide sorter as a part other apps install is [`features/slide-sorter`](features/slide-sorter) (Orrery's bodies
strip uses it).
