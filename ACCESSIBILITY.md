# Accessibility

Every AffrescoUI window hands its screen to the platform's accessibility API, so VoiceOver (macOS), Narrator
(Windows) and Orca (Linux) read and drive it like a native application. This page says how, what the automated tests
check through each platform's API, and what the maintainer checks by ear before a release.

## What a screen exposes

A node with a `role` (given, or one from its type: see `SCREEN_FORMAT.md`, "Every type: focus and accessibility")
becomes an element of the window's tree:

| What | From |
|---|---|
| Role | `role`, or the type's: `button`, `checkbox`, `slider`, `textbox`, `text`, `img`, `tab`, `tree`, `dialog`, ... |
| Name and description | `label` (else the node's text, the labels inside it, its `tooltip`) and `description` |
| Value | a slider's number with its minimum, maximum and step; a toggle's checked state; a field's text |
| States | `isOpen` (expanded), `selected`, `disabled`, and whether it has the focus |
| Live region | `live: "polite"` (toasts: the built-in `toast` is one) or `"assertive"`: new text is read without moving the focus |
| Text field | its text, the caret and the selection |
| Actions | press (its `onClick`), increment and decrement (a slider), expand and collapse (an `isOpen` node), focus |
| Bounds | in screen points: the platform scales them on HiDPI displays |

Updates are incremental: after a frame that changed something, the window sends only the elements whose role, name,
value, state or bounds changed (`modules/render/src/platform/AccessKitTree.cpp`). They are bounded (at most 20,000
elements and 16 KiB of text per element) and never wait for the assistive technology: AccessKit answers it from the
last tree it was given. Actions it asks for are queued and run on the UI thread in the next frame, as a click would.

## The backend (decision D8)

| OS | API | How | Why |
|---|---|---|---|
| macOS | NSAccessibility | AccessKit's subclassing adapter on the GLFW content view | One tree-to-API mapping kept by the AccessKit project (used by egui, Slint, Bevy); writing an `NSAccessibilityElement` per node by hand would duplicate it |
| Windows | UI Automation | AccessKit's adapter; the window's procedure is subclassed (`SetWindowSubclass`) to answer `WM_GETOBJECT`, so GLFW is untouched | A native UIA provider is several COM interfaces per pattern; AccessKit implements them |
| Linux, BSDs | AT-SPI over D-Bus | AccessKit's unix adapter, which runs on its own thread: it reads the last tree handed over and queues actions | AT-SPI over D-Bus by hand means a D-Bus binding and the AT-SPI protocol; AccessKit brings both |

AccessKit's C bindings ship prebuilt static libraries for each platform and architecture, so no Rust toolchain is
needed: CMake downloads the release pinned by its SHA-256 (`cmake/AffrescoUIAccessibility.cmake`).
`-DAFFRESCOUI_ACCESSIBILITY=OFF` builds without it, and windows then expose nothing.

## Checked through the API (automated)

| Test | What it reads | Where it runs |
|---|---|---|
| `tests/unit/accessibility_os.cpp` | the tree the platform gets: ids that last, values and ranges, a field's selection, bounds, `isOpen`, `selected`, `disabled`, `live`, the actions allowed, and running them | every platform, headless |
| `tests/unit/window/accessibility_mac.cpp` | NSAccessibility, as VoiceOver reads it: roles (`AXStaticText`, `AXButton`, `AXCheckBox`, `AXSlider`, `AXTextField`), names, help, values, the caret, frames in screen points, press and increment, updates after a change, a disabled element, a hidden panel leaving the tree | macOS: `ctest -R window` locally; CI builds it |
| `tests/unit/window/accessibility_windows.cpp` | UI Automation, as Narrator reads it: control types, names, the RangeValue pattern, bounds in physical pixels, Invoke and Toggle | Windows: compiled in CI; it runs on a desktop with OpenGL 4.1 (`ctest -R window`), which CI's runners lack |
| `tests/unit/window/accessibility_linux.cpp` | AT-SPI, as Orca reads it: roles, names, the Value interface, a press through the Action interface | Linux CI under Xvfb with at-spi2-core (`AFFRESCOUI_ATSPI=1`); skipped where no accessibility bus is up |

The focus moving between elements isn't asserted through the platform's API: a hidden test window is never the key
window, and the platforms report focus only for that one. The tree's focus is checked in `accessibility_os.cpp`.

## Checked by ear (the maintainer, before a release)

With VoiceOver (Cmd+F5), Narrator (Ctrl+Win+Enter) and Orca (Super+Alt+S), on the Kanban template and on Forge:

1. Tab through the window: each control is announced once, with its role and name, in tree order.
2. The activity bar's views are read as tabs, the selected one as selected; the editor tabs likewise.
3. In the explorer tree, an item reads expanded or collapsed, and the screen reader's expand command opens it.
4. A slider reads its value and changes by one step with the screen reader's increment.
5. In a text field, moving the caret and selecting text are read.
6. A toast (Forge: run "Reindex Folder") is read without the focus moving.
7. A dialog (Forge: delete a file) is announced with its title, and its buttons are reachable.
8. On a HiDPI display, the screen reader's highlight lines up with each element.
9. Disabled elements are read as dimmed or unavailable.
