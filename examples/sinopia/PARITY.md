# Sinopia parity matrix

Sinopia is a presentation editor. This matrix lists what a daily user of such an editor
expects, and how far Sinopia goes: **done** (works as expected), **partial** (there, with less) or **missing**.
*Start* is Sinopia before wave 14 (a C++ app); *Now* is Sinopia as a folder of JSON that the stock host runs,
with its slide show, presenter view, audience window and files on library capabilities (the `windows` section,
`saveFile`, `openFile`, `export`, snapshots). Evidence names a scenario of [`index.scenarios.json`](index.scenarios.json)
(without its `sinopia-` prefix), or a frame of `tests/render/scenarios/sinopia.json`, that shows it.

| Area | Feature | Start | Now | 62 | 27 | 31 | 120 |
|---|---|---|---|---|
| Shell | Ribbon with tabs (Home, Insert, Ink, Design, Transitions, Motion, Present, Record, Check, View, Help) | missing | done | first-frame, insert-tab, design-tab, ink, rehearse-timings, check-and-comments |
| Shell | Ribbon groups with titles, large and small buttons | missing | done | first-frame |
| Shell | Ribbon galleries (shapes, styles, themes, transitions, animations) | missing | partial | first-frame (shapes), design-tab (themes), launchers-and-gallery (the whole list) |
| Shell | Collapse and pin the ribbon | missing | missing | |
| Shell | A contextual tab for the selected shape | missing | missing | |
| Shell | Quick access toolbar (save, undo, redo, start from beginning) | missing | done | save-and-open, present-from-current, first-frame |
| Shell | Title bar with the file name | partial | done | first-frame |
| Shell | Search ("tell me what you want to do") that runs commands | missing | missing | |
| Shell | File view (new, open, save, export) | missing | partial | save-and-open, export-pdf |
| Shell | Key hints (Alt shows a key on every command) | missing | missing | |
| Slides | Thumbnails pane with slide numbers | partial | done | first-frame, slide-4 |
| Slides | New slide with a layout, duplicate, delete | done | done | new-duplicate-delete-slide |
| Slides | Drag to reorder | partial | done | sorter-drag-to-reorder |
| Slides | Sections (add, rename, collapse) | partial | partial | first-frame, section-menu (add, rename, remove; no collapse) |
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
| Text | Paragraph alignment (left, centre, right, justify) | missing | partial | |
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
| Arrange | Selection pane (names, show and hide) | missing | missing | |
| Arrange | Nudge with arrow keys | partial | done | align-and-distribute |
| Content | Insert picture | partial | missing | |
| Content | Crop a picture | missing | missing | |
| Content | Tables | missing | missing | |
| Content | Table styles | missing | missing | |
| Content | Charts (column, bar, line, pie) | missing | missing | |
| Content | Icons | missing | done | insert-tab |
| Content | Text box and styled text effects | partial | done | add-a-text-box, insert-tab |
| Design | Layouts (title, title and content, section header, two content, comparison, title only, blank) | missing | done | slide-menus |
| Design | Slide master view | missing | missing | |
| Design | Themes | missing | partial | design-tab |
| Design | Variants | missing | partial | design-tab |
| Design | Theme colours | missing | missing | |
| Design | Theme fonts | missing | missing | |
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
| Transitions | Preview | missing | missing | |
| Animations | Entrance effects | partial | done | add-an-animation |
| Animations | Emphasis effects | missing | missing | |
| Animations | Exit effects | missing | partial | add-an-animation |
| Animations | Motion paths | missing | missing | |
| Animations | Animation pane | missing | missing | |
| Animations | Timing (on click, with, after; duration, delay) | missing | partial | add-an-animation |
| Animations | Preview | missing | missing | |
| Slide show | Full screen | missing | done | present-and-step |
| Slide show | Advance by click and keys; back; go to slide by number | done | partial | present-and-step |
| Slide show | From the beginning and from the current slide | partial | done | present-from-current |
| Slide show | Black and white screen (B, W) | done | done | present-skips-and-blacks |
| Slide show | End show (Esc) | done | done | present-and-step, present-ends-at-the-end |
| Slide show | Rehearse timings, and a show that uses them | missing | done | rehearse-timings |
| Slide show | Record audio and video, export to video | missing | missing | |
| Presenter view | Current slide | missing | done | presenter-view |
| Presenter view | Next slide | done | done | presenter-view |
| Presenter view | Notes | partial | done | presenter-view |
| Presenter view | Timer (pause, reset) and clock | partial | done | presenter-view |
| Presenter view | Slide navigator | missing | missing | |
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
| Now | 54 | 29 | 28 | 111 |

The Ink, Record, Check and Help tabs added nine rows that the first list didn't have (all missing at the start), so *Now* counts 120 rows.
