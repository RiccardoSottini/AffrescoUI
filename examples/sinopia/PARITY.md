# Sinopia parity matrix

Sinopia is a presentation editor. This matrix lists what a daily user of such an editor
expects, and how far Sinopia goes: **done** (works as expected), **partial** (there, with less) or **missing**.
*Start* is Sinopia as a C++ app; *Now* is Sinopia as a folder of JSON that the stock host runs,
with its slide show, presenter view, audience window and files on library capabilities (the `windows` section,
`saveFile`, `openFile`, `export`, snapshots). Evidence names a scenario of [`index.scenarios.json`](index.scenarios.json)
(without its `sinopia-` prefix), or a frame of `tests/render/scenarios/sinopia.json`, that shows it.

| Area | Feature | Start | Now | Evidence |
|---|---|---|---|---|
| Shell | Ribbon with tabs (Home, Insert, Ink, Design, Transitions, Motion, Present, Record, Check, View, Help) | missing | done | first-frame, insert-tab, design-tab, ink, rehearse-timings, check-and-comments |
| Shell | Ribbon groups with titles, large and small buttons | missing | done | first-frame |
| Shell | Ribbon galleries (shapes, styles, themes, transitions, animations) | missing | partial | first-frame (shapes), design-tab (themes), launchers-and-gallery (the whole list) |
| Shell | Collapse and pin the ribbon | missing | partial | sections-and-ribbon (Ctrl+F1 and the bar's button; a tab brings it back pinned, not over the slide) |
| Shell | A contextual tab for the selected shape | missing | missing | |
| Shell | Quick access toolbar (save, undo, redo, start from beginning) | missing | done | save-and-open, present-from-current, first-frame |
| Shell | Title bar with the file name | partial | done | first-frame |
| Shell | Search ("tell me what you want to do") that runs commands | missing | partial | search-commands (33 commands by name, and the text in the slides) |
| Shell | File view (new, open, save, export) | missing | partial | save-and-open, export-pdf |
| Shell | Key hints (Alt shows a key on every command) | missing | missing | |
| Slides | Thumbnails pane with slide numbers | partial | done | first-frame, slide-4 |
| Slides | New slide with a layout, duplicate, delete | done | done | new-duplicate-delete-slide |
| Slides | Drag to reorder | partial | done | sorter-drag-to-reorder |
| Slides | Sections (add, rename, collapse) | partial | done | first-frame, section-menu, sections-and-ribbon |
| Slides | Hide slide (dimmed, crossed number, skipped in the show) | partial | partial | hide-a-slide, present-skips-and-blacks |
| Slides | Slide context menu | missing | done | context-menus (on the rail, a shape and the canvas; Shift+F10) |
| Canvas | Slide centred on grey | partial | done | first-frame, resized |
| Canvas | Placeholders ("Click to add title", "Click to add text") | missing | done | edit-text-in-place |
| Canvas | Zoom and fit to window | partial | done | first-frame, resized |
| Canvas | Notes pane under the slide | partial | done | first-frame, notes-hidden, speaker-notes |
| Canvas | Rulers, gridlines and guides (View) | partial | partial | |
| Status bar | Slide x of y | missing | done | slide-4 |
| Status bar | Language | missing | done | first-frame |
| Status bar | Notes and comments toggles | missing | done | speaker-notes, notes-hidden, check-and-comments |
| Status bar | Views (normal, sorter, reading, slide show) | partial | done | sorter-view, outline-view, present-from-current, background-and-reading-view |
| Status bar | Zoom slider, percentage and fit | missing | done | first-frame |
| Text | Font, size, grow and shrink | missing | done | font-size-and-case (the two fonts the app ships) |
| Text | Bold, italic, underline, strikethrough, shadow | missing | done | copy-style |
| Text | Font colour and highlight | missing | done | colour-grids |
| Text | Change case and clear formatting | missing | done | font-size-and-case |
| Text | Paragraph alignment (left, centre, right, justify) | missing | partial | align-text (a text box's text as a whole; justified text draws flush left) |
| Text | Line spacing and columns | missing | done | paragraph-menus |
| Text | Bullets and numbering | missing | done | bullets-numbering-and-levels |
| Text | List levels with Tab and Shift+Tab | missing | done | bullets-numbering-and-levels, edit-text-in-place |
| Text | Edit text in place (double-click, caret, selection) | missing | done | edit-text-in-place (double click or F2; the text is edited plain and its formatting shows again when the edit ends) |
| Text | Vertical anchor and autofit | missing | partial | paragraph-menus (anchor and text direction; no autofit) |
| Shapes | Shapes gallery (lines, rectangles, basic shapes, arrows, stars, callouts) | partial | partial | add-a-shape, first-frame |
| Shapes | Shape fill (solid, gradient, none) | partial | partial | format-fill-and-line, colour-grids (no gradient) |
| Shapes | Shape outline (colour, weight, dashes) | partial | partial | format-fill-and-line, colour-grids |
| Shapes | Shape effects (shadow, glow, soft edges) | partial | partial | shape-effects (a shadow and a glow, unblurred; no soft edges) |
| Shapes | Quick styles | missing | done | colour-grids |
| Shapes | Text inside shapes | missing | done | edit-text-in-place |
| Format pane | Fill and line | partial | partial | format-fill-and-line |
| Format pane | Effects | partial | partial | shape-effects (none, shadow, glow) |
| Format pane | Size and position (with lock aspect ratio) | partial | partial | format-size-and-rotation |
| Format pane | Text box options (anchor, margins, wrap) | missing | missing | |
| Arrange | Move and resize with handles | done | partial | selected |
| Arrange | Smart guides (centres and edges) | missing | partial | |
| Arrange | Distance indicators (equal gaps) | missing | missing | |
| Arrange | Align (left, centre, right, top, middle, bottom) | done | done | align-and-distribute |
| Arrange | Distribute horizontally and vertically | partial | done | align-and-distribute |
| Arrange | Group and ungroup | missing | missing | |
| Arrange | Order (front, back, forward, backward) | done | done | arrange-order |
| Arrange | Rotate (handle, 90 degrees) and flip | partial | partial | format-size-and-rotation |
| Arrange | Selection pane (names, show and hide) | missing | partial | selection-pane (Home > Select > Selection Pane; the names are given, not renamed) |
| Arrange | Nudge with arrow keys | partial | done | align-and-distribute |
| Content | Insert picture | partial | missing | |
| Content | Crop a picture | missing | missing | |
| Content | Tables | missing | missing | |
| Content | Table styles | missing | missing | |
| Content | Charts (column, bar, line, pie) | missing | missing | |
| Content | Icons | missing | done | insert-tab |
| Content | Links (to a slide of the deck, to a web address) | missing | partial | link-to-a-slide (to a slide, followed by a click in the show; no web addresses) |
| Content | Text box and styled text effects | partial | done | add-a-text-box, insert-tab |
| Design | Layouts (title, title and content, section header, two content, comparison, title only, blank) | missing | done | slide-menus |
| Design | Slide master view | missing | missing | |
| Design | Themes | missing | partial | design-tab |
| Design | Variants | missing | partial | design-tab |
| Design | Theme colours | missing | partial | theme-colors-and-fonts (another theme's colours; no colours of the deck's own) |
| Design | Theme fonts | missing | partial | theme-colors-and-fonts (the titles' and the text's, from the two fonts the app ships) |
| Design | Slide size (16:9, 4:3) | partial | missing | |
| Design | Format background | missing | done | background-and-reading-view |
| Tools | Format painter | missing | done | copy-style |
| Tools | Find | missing | done | find-and-replace |
| Tools | Replace | missing | done | find-and-replace |
| Tools | Undo and redo | done | done | delete-and-undo |
| Tools | Cut, copy and paste | partial | partial | duplicate-an-object, paste-menu (no other apps' clipboard) |
| Ink | Draw with a pen and a highlighter (colours, widths) | missing | done | ink |
| Ink | Eraser and Clear Ink | missing | done | ink |
| Ink | Ink to shape and ink replay | missing | missing | |
| Review | Comments (add, write, resolve, delete, next and previous) | missing | done | check-and-comments |
| Review | Accessibility check (titles, empty placeholders, alternative text) | missing | done | check-and-comments |
| Review | Spelling | missing | missing | |
| Help | Keyboard shortcuts | missing | done | check-and-comments |
| Transitions | Fade | partial | partial | slide-transition |
| Transitions | Push | partial | done | slide-transition |
| Transitions | Wipe | partial | done | slide-transition |
| Transitions | Glide (objects move to their places on the next slide) | missing | missing |  |
| Transitions | Effect options, duration, apply to all | partial | done | slide-transition |
| Transitions | Preview | missing | done | preview (in the window, from the slide before) |
| Animations | Entrance effects | partial | done | add-an-animation |
| Animations | Emphasis effects | missing | missing | |
| Animations | Exit effects | missing | partial | add-an-animation |
| Animations | Motion paths | missing | missing | |
| Animations | Animation pane | missing | done | animation-pane (the order, reorder, remove, select) |
| Animations | Timing (on click, with, after; duration, delay) | missing | partial | add-an-animation |
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
| Presenter view | Pen | missing | missing | |
| Presenter view | Laser pointer | missing | missing | |
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
| Now | 68 | 31 | 22 | 121 |

The Ink, Record, Check and Help tabs added nine rows that the first list didn't have, and Insert > Link one (all missing at the start), so *Now* counts 121 rows.

## Left for later

Every row above that is still partial or missing, and every control still disabled (its tooltip says "… isn't
available yet"), with what it needs: **JSON** (Sinopia's own documents) or a **library** feature first. Priority: **P1**
a daily user meets it, **P2** occasionally, **P3** rarely.

| Priority | Row or control | What it needs |
|---|---|---|
| P1 | Arrange: group and ungroup | JSON: an object of kind `group` holding objects, drawn and moved as one, with Group and Ungroup in Arrange and the context menu |
| P1 | Content: insert picture (Insert > Pictures) | Library: `openFile` of an image into an asset the slide draws and the saved deck carries; then JSON for an object of kind `picture` |
| P1 | Content: tables (Insert > Table) | JSON: an object of kind `table` (rows of text cells) with a size grid to insert it and Tab between cells |
| P1 | Tools: cut, copy and paste with other apps | Library: steps that write and read the system clipboard's text and images |
| P1 | Arrange: move and resize with handles (Shift keeps the proportions, Ctrl copies) | Library: modifier keys on pointer events (`onClick` and drags carry no Shift); then JSON |
| P1 | Text: paragraph alignment per paragraph, and justified text | JSON: alignment per paragraph; library: justified wrapping in labels |
| P1 | Slides: hide slide (the crossed-out number) | JSON: a strike through the number in the slide list |
| P2 | Shell: a contextual shape tab | JSON: a tab shown while a shape is selected, from the Home tab's drawing and arrange groups |
| P2 | Shell: File view (new deck, recent decks) | JSON: New from the themes; library: a list of recently opened files |
| P2 | Canvas: rulers, gridlines and guides | JSON: rulers along the canvas, a drawn grid, guides dragged from the rulers that objects snap to |
| P2 | Text: autofit (shrink text on overflow) | Library: the measured height of a text in expressions; then JSON |
| P2 | Shapes and format pane: gradient fills, dashed outlines | Library: gradient fills and dashed strokes on path nodes; then the fill and outline grids |
| P2 | Format pane: size and position (scale in percent, lock the proportions) | JSON: scale fields and a lock that resizing honours |
| P2 | Format pane: text box options (margins, wrap) | JSON: margins and wrap in the pane, and the slide's text layout honouring them |
| P2 | Arrange: smart guides | JSON: snapping to other objects' edges and centres while dragging, with the lines drawn |
| P2 | Arrange: a rotate handle, and flip | JSON: the handle on the canvas; library: mirrored drawing of path nodes |
| P2 | Content: crop a picture | After pictures; library: a source rectangle on image nodes |
| P2 | Content: charts (Insert > Chart) | JSON: a chart object drawn as shapes from a small data table |
| P2 | Content: links to web addresses | Library: a step that opens a web address after asking the person (no document-run code) |
| P2 | Design: slide size (Design > Slide Size) | JSON: the 960 by 540 written in the slide, the show and the export read the deck's `size` instead |
| P2 | Transitions: glide (objects move to their places) | JSON: objects matched by name across two slides, their boxes animated |
| P2 | Animations: emphasis effects | JSON: pulse, spin and colour builds in the show's build template |
| P2 | Animations: timing (delay, repeat) | JSON: delay and repeat controls on the Motion tab and in the Animation Pane |
| P2 | Presenter view: pen | JSON: the ink layer over the show's slide, strokes kept per slide |
| P2 | Presenter view: laser pointer | JSON: a dot that follows the pointer over the show's slide; library: pointer moves on an image node, if not there |
| P2 | Review: spelling (Check > Spelling) | Library: a spelling service and squiggles under unknown words |
| P2 | Always: locales | Library: string tables per locale in expressions; then JSON for every label |
| P2 | Always: keyboard access, and names, roles and focus order | JSON: names on the remaining nodes and an audit of the focus order; key hints below |
| P3 | Shell: key hints (Alt shows a key on every command) | Library: an event for Alt pressed and released alone; then JSON badges on the tabs and buttons |
| P3 | Shell: search that runs commands (the rest of the commands) | JSON: the commands listed from the menus as well as the 33 named ones |
| P3 | Shell: ribbon galleries (transitions and builds as galleries) | JSON: the transitions and builds as galleries, like shapes and themes |
| P3 | Shell: collapse the ribbon (a tab shows it over the slide) | JSON: the ribbon as an overlay while collapsed |
| P3 | Shapes: the whole gallery (all stars, callouts and arrows) | JSON: more presets in `parts/geometry.json` |
| P3 | Shapes and format pane: effects (soft edges, blurred shadows) | Library: blur on path nodes' shadows and edges |
| P3 | Arrange: distance indicators | JSON: equal-gap hints while dragging |
| P3 | Arrange: rename in the selection pane | JSON: an editor in the row on a double click |
| P3 | Content: table styles | After tables; JSON |
| P3 | Design: slide master view | JSON: a view that edits the layouts and their placeholders |
| P3 | Design: themes and variants of one's own | JSON: save the deck's look as a theme |
| P3 | Design: theme colours of one's own | JSON: a hex field for each of the ten slots |
| P3 | Design: more theme fonts | Library: more vendored fonts (the app ships two) |
| P3 | Ink: ink to shape (Ink > Ink to Shape) and replay (Ink > Ink Replay) | JSON: replay by animating each stroke's points; ink to shape by fitting a stroke to a preset |
| P3 | Transitions: fade through black | JSON: an effect option of fade |
| P3 | Animations: exit effects (directions) | JSON: the fly and wipe directions for exits |
| P3 | Animations: motion paths | Library: a point along a path in expressions; then JSON |
| P3 | Slide show: record and export to video (Record > Record, Record > Export to Video) | Library: audio and video capture and encoding |
| P3 | Files: export every slide as an image at once | Library: an export of several PNG pages in one step |
| P3 | Always: the assistant chat add-on | The kit add-on's placement (right-to-left, docking, the chat button's overlap); then JSON |
| P3 | Text: character spacing (Home > Character Spacing) | Library: letter spacing on runs |
| P3 | Text: convert to a diagram (Home > Convert to a Diagram) | JSON: diagram layouts made from a list's items |
| P3 | Text: speak to type (Home > Dictate) | Library: a speech service (the microphone isn't reachable from a screen) |
| P3 | Insert: equation (Insert > Equation) | Library: math layout |
| P3 | Insert: video and audio (Insert > Video, Insert > Audio) | Library: media playback nodes |
| P3 | Insert: photo album (Insert > Photo Album) | After pictures; JSON: one slide per picture |
| P3 | View: notes page (View > Notes Page) | JSON: a page with the slide and its notes, which export prints |
| P3 | Help: feedback (Help > Feedback) | A place to send it; until then the button stays disabled |
