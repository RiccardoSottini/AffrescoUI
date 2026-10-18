# Changelog

All notable changes are listed here. AffrescoUI follows [Semantic Versioning](https://semver.org). Until 1.0.0, a
minor version may change the C++ API and the screen format.

## 0.1.0 (2026-10-18)

The first public release of AffrescoUI, which started in 2017 as sGLGUI.

- **Renderer** (`affrescoui::render`): C++23 and OpenGL 4.1 core.
  - Layout: nested panels in pixels or percentages, anchors, rows, columns, grids, scrolling and overlays.
  - Text: TrueType text with wrapping, a text input and a multi-line text editor.
  - Controls: toggles, checkboxes and sliders.
  - HiDPI support, one draw call per font per frame, and no work when idle.
- **Screen format** (`affrescoui::format`, format version 1), with a JSON Schema in `schema/`:
  - layout, state and JS-style `{{expressions}}`;
  - components, styles and themes;
  - events with bubbling, shortcuts, drag and drop, timers and animations;
  - screens, channels, `persist` and `include`;
  - a catalog for agents.
- **Version control** (`affrescoui::vcs`):
  - SHA-256 commits and an append-only `main`;
  - proposals as branches with a live preview;
  - revert, restore and diff;
  - `allow` / `ask` / `deny` rules, modes and grants, with approvals pinned to the version reviewed;
  - change sets.
- **Peers** (`affrescoui::channel`): any number of them over local sockets, each one's author set by its socket, and
  request/reply relaying between them.
- **Agents:** a two-way Python MCP server (`mcp/`).
- **Examples:**
  - `affrescoui_host`, with five templates and a sign-in screen;
  - `affrescoui_studio`, a client that is itself a screen.
- **Checks:** `tests/render`, a pixel-exact rendering check of every template, also run by `ctest`.

Known limits:
- Tested on macOS only. Linux is untested and Windows is not supported yet.
- The history is kept in memory, so it doesn't survive a restart.
- Text covers Latin-1 only.
