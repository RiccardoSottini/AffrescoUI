# Sinopia parity matrix

Sinopia is a presentation editor. This matrix lists what a daily user of such an editor
expects, and how far Sinopia goes: **done** (works as expected), **partial** (there, with less) or **missing**.
*Start* is Sinopia as a C++ app; *Now* is Sinopia as a folder of JSON that the stock host runs,
with its slide show, presenter view, audience window and files on library capabilities (the `windows` section,
`saveFile`, `openFile`, `export`, snapshots). Evidence names a scenario of [`index.scenarios.json`](lisp/index.scenarios.json) or
[`design.scenarios.json`](lisp/design.scenarios.json) (without its `sinopia-` prefix), or a frame of `tests/render/scenarios/sinopia.json`, that shows it.

| Area | Feature | Start | Now | Evidence |
|---|---|---|---|---|
| Shell | Ribbon with tabs (Home, Insert, Ink, Design, Transitions, Motion, Present, Record, Check, View, Help) | missing | done | first-frame, insert-tab, design-tab, ink, rehearse-timings, check-and-comments |
| Shell | Ribbon groups with titles, large and small buttons | missing | done | first-frame |
| Shell | Ribbon galleries (shapes, styles, themes, transitions, animations) | missing | partial | first-frame (shapes), design-tab (themes), launchers-and-gallery (the whole list), shape-format-tab (quick styles) |
| Shell | Collapse and pin the ribbon | missing | partial | sections-and-ribbon (Ctrl+F1 and the bar's button; a tab brings it back pinned, not over the slide) |
| Shell | A contextual tab for the selected shape | missing | done | shape-format-tab (Shape Format: insert shapes, quick styles, arrange, size) |
| Shell | Quick access toolbar (save, undo, redo, start from beginning) | missing | done | save-and-open, present-from-current, first-frame |
| Shell | Title bar with the file name | partial | done | first-frame |
| Shell | Search ("tell me what you want to do") that runs commands | missing | done | search-commands (the named commands, and the text in the slides), search-every-command (the ribbon menus' items too) |
| Shell | File view (new, open, save, export) | missing | done | new-and-recent-decks (a new deck from a theme, recent decks), save-and-open, export-pdf |
| Shell | Key hints (Alt shows a key on every command) | missing | missing | |
| Slides | Thumbnails pane with slide numbers | partial | done | first-frame, slide-4 |
| Slides | New slide with a layout, duplicate, delete | done | done | new-duplicate-delete-slide |
| Slides | Drag to reorder | partial | done | sorter-drag-to-reorder |
| Slides | Sections (add, rename, collapse) | partial | done | first-frame, section-menu, sections-and-ribbon |
| Slides | Hide slide (dimmed, crossed number, skipped in the show) | partial | done | hide-a-slide, present-skips-and-blacks |
| Slides | Slide context menu | missing | done | context-menus (on the rail, a shape and the canvas; Shift+F10) |
| Canvas | Slide centred on grey | partial | done | first-frame, resized |
| Canvas | Placeholders ("Click to add title", "Click to add text") | missing | done | edit-text-in-place |
| Canvas | Zoom and fit to window | partial | done | first-frame, resized |
| Canvas | Notes pane under the slide | partial | done | first-frame, notes-hidden, speaker-notes |
| Canvas | Rulers, gridlines and guides (View) | partial | done | rulers-grid-and-guides (gridlines over the slide, centre guides, guides dragged out of the rulers; objects snap to them) |
| Status bar | Slide x of y | missing | done | slide-4 |
| Status bar | Language | missing | done | first-frame |
| Status bar | Notes and comments toggles | missing | done | speaker-notes, notes-hidden, check-and-comments |
| Status bar | Views (normal, sorter, reading, slide show) | partial | done | sorter-view, outline-view, present-from-current, background-and-reading-view |
| View | Notes page (the slide over its notes, edited in place; Export Notes Pages as a PDF) | missing | done | notes-page |
| Status bar | Zoom slider, percentage and fit | missing | done | first-frame |
| Text | Font, size, grow and shrink | missing | done | font-size-and-case (the two fonts the app ships) |
| Text | Bold, italic, underline, strikethrough, shadow | missing | done | copy-style |
| Text | Font colour and highlight | missing | done | colour-grids |
| Text | Change case and clear formatting | missing | done | font-size-and-case |
| Text | Paragraph alignment (left, centre, right, justify) | missing | done | align-text, align-a-paragraph (per paragraph while the text is edited; justified lines fill the box's width, the last one flush left) |
| Text | Line spacing and columns | missing | done | paragraph-menus |
| Text | Bullets and numbering | missing | done | bullets-numbering-and-levels |
| Text | List levels with Tab and Shift+Tab | missing | done | bullets-numbering-and-levels, edit-text-in-place |
| Text | Edit text in place (double-click, caret, selection) | missing | done | edit-text-in-place (double click or F2; the text is edited plain and its formatting shows again when the edit ends) |
| Text | Vertical anchor and autofit | missing | done | paragraph-menus (anchor and text direction), format-gradient-dash-and-text-box (shrink text on overflow, resize shape to fit) |
| Shapes | Shapes gallery (lines, rectangles, basic shapes, arrows, stars, callouts) | partial | done | add-a-shape, first-frame, more-shape-presets |
| Shapes | Shape fill (solid, gradient, none) | partial | done | format-fill-and-line, colour-grids, format-gradient-dash-and-text-box |
| Shapes | Shape outline (colour, weight, dashes) | partial | done | format-fill-and-line, colour-grids, format-gradient-dash-and-text-box |
| Shapes | Shape effects (shadow, glow, soft edges) | partial | partial | shape-effects (a shadow and a glow, unblurred; no soft edges) |
| Shapes | Quick styles | missing | done | colour-grids |
| Shapes | Text inside shapes | missing | done | edit-text-in-place |
| Format pane | Fill and line | partial | done | format-fill-and-line, format-gradient-dash-and-text-box (gradients, dashes) |
| Format pane | Effects | partial | partial | shape-effects (none, shadow, glow) |
| Format pane | Size and position (with lock aspect ratio) | partial | done | format-size-and-rotation, format-gradient-dash-and-text-box, shape-format-tab |
| Format pane | Text box options (anchor, margins, wrap) | missing | done | format-gradient-dash-and-text-box (margins, wrap, autofit) |
| Arrange | Move and resize with handles | done | partial | selected (Shift held as the drag starts keeps a corner resize's proportions and a move on one axis; Ctrl to copy isn't there) |
| Arrange | Smart guides (centres and edges) | missing | done | equal-gaps (the canvas snaps to other objects' and the slide's edges and centres, and to guides, drawing the lines while dragging) |
| Arrange | Distance indicators (equal gaps) | missing | done | equal-gaps (a move snaps to a gap the row already has, or to the middle between two objects, and marks the equal gaps) |
| Arrange | Align (left, centre, right, top, middle, bottom) | done | done | align-and-distribute |
| Arrange | Distribute horizontally and vertically | partial | done | align-and-distribute |
| Arrange | Group and ungroup | missing | done | group-and-ungroup (Arrange, the context menu, Ctrl+G and Ctrl+Shift+G; a click on one member selects the group) |
| Arrange | Order (front, back, forward, backward) | done | done | arrange-order |
| Arrange | Rotate (handle, 90 degrees) and flip | partial | done | format-size-and-rotation, group-and-ungroup (Arrange: rotate 90° either way, flip either way; the canvas's handle turns one object) |
| Arrange | Selection pane (names, show and hide) | missing | done | selection-pane (Home > Select > Selection Pane), rename-in-selection-pane (a double click renames) |
| Arrange | Nudge with arrow keys | partial | done | align-and-distribute |
| Content | Insert picture | partial | done | insert-a-picture (a PNG or JPEG, at most 640 by 400 in its proportions, kept in the deck so the saved file carries it) |
| Content | Crop a picture | missing | partial | crop-a-picture (the picture's menu crops to a square, 4:3 or 16:9 around its middle, and Whole Picture undoes it; no crop handles to drag) |
| Content | Tables | missing | done | insert-a-table (Insert > Table's size grid; a double click edits a cell, Tab and Shift+Tab move, Tab in the last cell adds a row) |
| Content | Table styles | missing | done | insert-a-table (an accent of the theme for the selected table, under Insert > Table) |
| Content | Charts (column, bar, line, pie) | missing | done | insert-a-chart (Insert > Chart: sample data drawn as columns, bars, a line or a pie with a legend and a value axis; a double click edits the data as a table) |
| Content | Icons | missing | done | insert-tab |
| Content | Links (to a slide of the deck, to a web address) | missing | partial | link-to-a-slide (to a slide, followed by a click in the show; no web addresses) |
| Content | Text box and styled text effects | partial | done | add-a-text-box, insert-tab |
| Design | Layouts (title, title and content, section header, two content, comparison, title only, blank) | missing | done | slide-menus |
| Design | Slide master view | missing | missing | |
| Design | Themes | missing | partial | design-tab |
| Design | Variants | missing | partial | design-tab |
| Design | Theme colours | missing | done | theme-colors-and-fonts (another theme's colours), own-theme-colours (hex codes of the deck's own) |
| Design | Theme fonts | missing | partial | theme-colors-and-fonts (the titles' and the text's, from the two fonts the app ships) |
| Design | Slide size (16:9, 4:3) | partial | missing | |
| Design | Format background | missing | done | background-and-reading-view |
| Tools | Format painter | missing | done | copy-style |
| Tools | Find | missing | done | find-and-replace |
| Tools | Replace | missing | done | find-and-replace |
| Tools | Undo and redo | done | done | delete-and-undo |
| Tools | Cut, copy and paste | partial | done | duplicate-an-object, paste-menu, clipboard-with-other-apps (the objects' text goes to the system clipboard; another app's text pastes as a text box; pictures from other apps don't yet) |
| Ink | Draw with a pen and a highlighter (colours, widths) | missing | done | ink |
| Ink | Eraser and Clear Ink | missing | done | ink |
| Ink | Ink to shape and ink replay | missing | done | ink-to-shape-and-replay (closed strokes to rectangles and ellipses) |
| Review | Comments (add, write, resolve, delete, next and previous) | missing | done | check-and-comments |
| Review | Accessibility check (titles, empty placeholders, alternative text) | missing | done | check-and-comments |
| Review | Spelling | missing | missing | |
| Help | Keyboard shortcuts | missing | done | check-and-comments |
| Transitions | Fade | partial | done | slide-transition, glide-and-smooth-fade (through black or smoothly) |
| Transitions | Push | partial | done | slide-transition |
| Transitions | Wipe | partial | done | slide-transition |
| Transitions | Glide (objects move to their places on the next slide) | missing | done | glide-and-smooth-fade (matched by name, else the same object moved or resized) |
| Transitions | Effect options, duration, apply to all | partial | done | slide-transition |
| Transitions | Preview | missing | done | preview (in the window, from the slide before) |
| Animations | Entrance effects | partial | done | add-an-animation |
| Animations | Emphasis effects | missing | done | emphasis-and-timing (pulse, spin, teeter) |
| Animations | Exit effects | missing | done | add-an-animation, emphasis-and-timing (fly to any side; wipe from the left only) |
| Animations | Motion paths | missing | partial | emphasis-and-timing (four preset paths: right, down, arc, circle; no drawn paths) |
| Animations | Animation pane | missing | done | animation-pane (the order, reorder, remove, select) |
| Animations | Timing (on click, with, after; duration, delay) | missing | done | add-an-animation, emphasis-and-timing (delay and repeat on the Motion tab and in the Animation Pane) |
| Animations | Preview | missing | done | preview (each build once the one before has played) |
| Slide show | Full screen | missing | done | present-and-step |
| Slide show | Advance by click and keys; back; go to slide by number | done | done | present-and-step, present-go-to-number |
| Slide show | From the beginning and from the current slide | partial | done | present-from-current |
| Slide show | Black and white screen (B, W) | done | done | present-skips-and-blacks |
| Slide show | End show (Esc) | done | done | present-and-step, present-ends-at-the-end |
| Slide show | Rehearse timings, and a show that uses them | missing | done | rehearse-timings |
| Slide show | Record audio and video, export to video | missing | missing | |
| Presenter view | Current slide | missing | done | presenter-view |
| Presenter view | Next slide | done | done | presenter-view |
| Presenter view | Notes | partial | done | presenter-view |
| Presenter view | Timer (pause, reset) and clock | partial | done | presenter-view |
| Presenter view | Slide navigator | missing | done | presenter-view (See All Slides) |
| Presenter view | Pen | missing | done | presenter-pen-and-laser (strokes kept per slide while the show runs) |
| Presenter view | Laser pointer | missing | done | presenter-pen-and-laser (while pressed and dragged) |
| Presenter view | Audience window | partial | done | presenter-view |
| Files | Export to PDF | done | done | export-pdf |
| Files | Export slides as images | partial | partial |  |
| Files | Save in its own format | done | done | save-and-open |
| Files | Open its own format | partial | done | save-and-open |
| Always | Light and dark | done | done | dark-mode |
| Always | Locales (English, Spanish, French) | missing | missing | |
| Always | Keyboard access to every command | partial | partial | |
| Always | Accessibility (names, roles, focus order) | partial | partial | |
| Always | Assistant chat add-on | missing | missing | |

## Counts

| | Done | Partial | Missing | Total |
|---|---:|---:|---:|---:|
| Start | 12 | 37 | 62 | 111 |
| Before the controls audit | 34 | 37 | 40 | 111 |
| Now | 101 | 14 | 7 | 122 |

The Ink, Record, Check and Help tabs added nine rows that the first list didn't have, Insert > Link one and View > Notes Page one (all missing at the start), so *Now* counts 122 rows.

## Left for later

Every row above that is still partial or missing, and every control still disabled (its tooltip says "… isn't
available yet"), with what it needs: **JSON** (Sinopia's own documents) or a **library** feature first. Priority: **P1**
a daily user meets it, **P2** occasionally, **P3** rarely.

| Priority | Row or control | What it needs |
|---|---|---|
| P2 | Tools: paste a picture from another app | Library: a step that reads the system clipboard's image |
| P2 | Arrange: Ctrl and a drag copies | Library: the canvas's `onTransform` saying the drag was a copy |
| P3 | Content: crop a picture by dragging | JSON: a crop mode whose handles move the crop's sides instead of the box |
| P2 | Content: links to web addresses | Library: a step that opens a web address after asking the person (no document-run code) |
| P2 | Design: slide size (Design > Slide Size) | JSON: the 960 by 540 written in the slide, the show and the export read the deck's `size` instead |
| P2 | Review: spelling (Check > Spelling) | Library: a spelling service and squiggles under unknown words |
| P2 | Always: locales | Library: string tables per locale in expressions; then JSON for every label |
| P2 | Always: keyboard access, and names, roles and focus order | JSON: names on the remaining nodes and an audit of the focus order; key hints below |
| P3 | Shell: key hints (Alt shows a key on every command) | Library: an event for Alt pressed and released alone; then JSON badges on the tabs and buttons |
| P3 | Animations: wipe directions | JSON: the wipe band from the right, the top and the bottom |
| P3 | Shell: ribbon galleries (transitions and builds as galleries) | JSON: the transitions and builds as galleries, like shapes and themes |
| P3 | Shell: collapse the ribbon (a tab shows it over the slide) | JSON: the ribbon as an overlay while collapsed |
| P3 | Shapes and format pane: effects (soft edges, blurred shadows) | Library: blur on path nodes' shadows and edges |
| P3 | Design: slide master view | JSON: a view that edits the layouts and their placeholders |
| P3 | Design: themes and variants of one's own | JSON: save the deck's look as a theme |
| P3 | Design: more theme fonts | Library: more vendored fonts (the app ships two) |
| P3 | Animations: motion paths drawn by hand (custom paths), and their line on the canvas | JSON: a path drawn on the slide as points, followed like an ink stroke |
| P3 | Slide show: record and export to video (Record > Record, Record > Export to Video) | Library: audio and video capture and encoding |
| P3 | Files: export every slide as an image at once | Library: an export of several PNG pages in one step |
| P3 | Always: the assistant chat add-on | The kit add-on's placement (right-to-left, docking, the chat button's overlap); then JSON |
| P3 | Text: character spacing (Home > Character Spacing) | Library: letter spacing on runs |
| P3 | Text: convert to a diagram (Home > Convert to a Diagram) | JSON: diagram layouts made from a list's items |
| P3 | Text: speak to type (Home > Dictate) | Library: a speech service (the microphone isn't reachable from a screen) |
| P3 | Insert: equation (Insert > Equation) | Library: math layout |
| P3 | Insert: video and audio (Insert > Video, Insert > Audio) | Library: media playback nodes |
| P3 | Insert: photo album (Insert > Photo Album) | After pictures; JSON: one slide per picture |
| P3 | Help: feedback (Help > Feedback) | A place to send it; until then the button stays disabled |
