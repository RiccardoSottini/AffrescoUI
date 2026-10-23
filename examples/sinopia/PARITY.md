# Sinopia parity matrix

Sinopia is a presentation editor. This matrix lists what a daily user of such an editor
expects, and how far Sinopia goes: **done** (works as expected), **partial** (there, with less) or **missing**.
*Start* is Sinopia before wave 14 (a C++ app); *Now* is this branch, where Sinopia is JSON only and the stock host
runs it (the slide show, presenter view and export of the C++ app come back in later rounds, on library
capabilities). Evidence names the frame of `tests/render/scenarios/sinopia.json` that shows it.

| Area | Feature | Start | Now | Evidence |
|---|---|---|---|---|
| Shell | Ribbon with tabs (Home, Insert, Ink, Design, Transitions, Motion, Present, Check, View) | missing | partial | first-frame, insert-tab, design-tab |
| Shell | Ribbon groups with titles, large and small buttons | missing | done | first-frame |
| Shell | Ribbon galleries (shapes, styles, themes, transitions, animations) | missing | partial | first-frame (shapes), design-tab (themes) |
| Shell | Collapse and pin the ribbon | missing | missing | |
| Shell | Contextual tab (Shape Format) when a shape is selected | missing | missing | |
| Shell | Quick access toolbar (save, undo, redo, start from beginning) | missing | partial | first-frame |
| Shell | Title bar with the file name | partial | done | first-frame |
| Shell | Search ("tell me what you want to do") that runs commands | missing | missing | |
| Shell | File view (new, open, save, export) | missing | missing | |
| Shell | KeyTips (Alt shows a key on every command) | missing | missing | |
| Slides | Thumbnails pane with slide numbers | partial | done | first-frame, slide-4 |
| Slides | New slide with a layout, duplicate, delete | done | partial | |
| Slides | Drag to reorder | partial | partial | |
| Slides | Sections (add, rename, collapse) | partial | partial | first-frame |
| Slides | Hide slide (dimmed, crossed number, skipped in the show) | partial | partial | |
| Slides | Slide context menu | missing | missing | |
| Canvas | Slide centred on grey | partial | done | first-frame, resized |
| Canvas | Placeholders ("Click to add title", "Click to add text") | missing | partial | |
| Canvas | Zoom and fit to window | partial | done | first-frame, resized |
| Canvas | Notes pane under the slide | partial | done | first-frame, notes-hidden |
| Canvas | Rulers, gridlines and guides (View) | partial | partial | |
| Status bar | Slide x of y | missing | done | slide-4 |
| Status bar | Language | missing | done | first-frame |
| Status bar | Notes and comments toggles | missing | partial | notes-hidden |
| Status bar | Views (normal, sorter, reading, slide show) | partial | partial | first-frame |
| Status bar | Zoom slider, percentage and fit | missing | done | first-frame |
| Text | Font, size, grow and shrink | missing | partial | |
| Text | Bold, italic, underline, strikethrough, shadow | missing | partial | |
| Text | Font colour and highlight | missing | missing | |
| Text | Change case and clear formatting | missing | partial | |
| Text | Paragraph alignment (left, centre, right, justify) | missing | partial | |
| Text | Line spacing and columns | missing | missing | |
| Text | Bullets and numbering | missing | missing | |
| Text | List levels with Tab and Shift+Tab | missing | missing | |
| Text | Edit text in place (double-click, caret, selection) | missing | missing | |
| Text | Vertical anchor and autofit | missing | missing | |
| Shapes | Shapes gallery (lines, rectangles, basic shapes, arrows, stars, callouts) | partial | partial | first-frame |
| Shapes | Shape fill (solid, gradient, none) | partial | missing | |
| Shapes | Shape outline (colour, weight, dashes) | partial | missing | |
| Shapes | Shape effects (shadow, glow, soft edges) | partial | missing | |
| Shapes | Quick styles | missing | missing | |
| Shapes | Text inside shapes | missing | missing | |
| Format pane | Fill and line | partial | missing | |
| Format pane | Effects | partial | missing | |
| Format pane | Size and position (with lock aspect ratio) | partial | missing | |
| Format pane | Text box options (anchor, margins, wrap) | missing | missing | |
| Arrange | Move and resize with handles | done | partial | selected |
| Arrange | Smart guides (centres and edges) | missing | partial | |
| Arrange | Distance indicators (equal gaps) | missing | missing | |
| Arrange | Align (left, centre, right, top, middle, bottom) | done | missing | |
| Arrange | Distribute horizontally and vertically | partial | missing | |
| Arrange | Group and ungroup | missing | missing | |
| Arrange | Order (front, back, forward, backward) | done | partial | |
| Arrange | Rotate (handle, 90 degrees) and flip | partial | missing | |
| Arrange | Selection pane (names, show and hide) | missing | missing | |
| Arrange | Nudge with arrow keys | partial | missing | |
| Content | Insert picture | partial | missing | |
| Content | Crop a picture | missing | missing | |
| Content | Tables | missing | missing | |
| Content | Table styles | missing | missing | |
| Content | Charts (column, bar, line, pie) | missing | missing | |
| Content | Icons | missing | missing | |
| Content | Text box and WordArt-like styles | partial | partial | |
| Design | Layouts (title, title and content, section header, two content, comparison, title only, blank) | missing | partial | |
| Design | Slide master view | missing | missing | |
| Design | Themes | missing | partial | design-tab |
| Design | Variants | missing | partial | design-tab |
| Design | Theme colours | missing | missing | |
| Design | Theme fonts | missing | missing | |
| Design | Slide size (16:9, 4:3) | partial | missing | |
| Design | Format background | missing | missing | |
| Tools | Format painter | missing | missing | |
| Tools | Find | missing | missing | |
| Tools | Replace | missing | missing | |
| Tools | Undo and redo | done | done | |
| Tools | Cut, copy and paste | partial | partial | |
| Transitions | Fade | partial | missing | |
| Transitions | Push | partial | missing | |
| Transitions | Wipe | partial | missing | |
| Transitions | Morph-like (objects glide between slides) | missing | missing | |
| Transitions | Effect options, duration, apply to all | partial | missing | |
| Transitions | Preview | missing | missing | |
| Animations | Entrance effects | partial | missing | |
| Animations | Emphasis effects | missing | missing | |
| Animations | Exit effects | missing | missing | |
| Animations | Motion paths | missing | missing | |
| Animations | Animation pane | missing | missing | |
| Animations | Timing (on click, with, after; duration, delay) | missing | missing | |
| Animations | Preview | missing | missing | |
| Slide show | Full screen | missing | missing | |
| Slide show | Advance by click and keys; back; go to slide by number | done | missing | |
| Slide show | From the beginning and from the current slide | partial | missing | |
| Slide show | Black and white screen (B, W) | done | missing | |
| Slide show | End show (Esc) | done | missing | |
| Presenter view | Current slide | missing | missing | |
| Presenter view | Next slide | done | missing | |
| Presenter view | Notes | partial | missing | |
| Presenter view | Timer (pause, reset) and clock | partial | missing | |
| Presenter view | Slide navigator | missing | missing | |
| Presenter view | Pen | missing | missing | |
| Presenter view | Laser pointer | missing | missing | |
| Presenter view | Audience window | partial | missing | |
| Files | Export to PDF | done | missing | |
| Files | Export slides as images | partial | missing | |
| Files | Save in its own format | done | partial | |
| Files | Open its own format | partial | missing | |
| Always | Light and dark | done | missing | |
| Always | Locales (English, Spanish, French) | missing | missing | |
| Always | Keyboard access to every command | partial | partial | |
| Always | Accessibility (names, roles, focus order) | partial | partial | |
| Always | Assistant chat add-on | missing | missing | |

## Counts

| | Done | Partial | Missing | Total |
|---|---:|---:|---:|---:|
| Start | 12 | 37 | 62 | 111 |
| Now | 10 | 27 | 74 | 111 |
