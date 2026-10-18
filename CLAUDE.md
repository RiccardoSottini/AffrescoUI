# AffrescoUI

C++23 / OpenGL 4.1 GUI library whose UIs are JSON screen documents that people and AI agents change live, under
version control (started 2017 as sGLGUI). Overview and architecture: `README.md`. Format: `SCREEN_FORMAT.md`.
Version control and permissions: `VERSION_CONTROL.md`.

Build & check: `cmake -S . -B build -G Ninja && cmake --build build`, then
`AFFRESCOUI_SCREENSHOT=out.ppm ./build/affrescoui_host` renders the first frame of the Kanban template to `out.ppm`.
`AFFRESCOUI_SCREENSHOT_AFTER=<ms>` captures after running that long (e.g. while peers change it).
MCP server (two-way): `mcp/affrescoui_mcp.py`. Studio client: `./build/affrescoui_studio`.
After any change, `python3 tests/render/check.py` must pass: every template, scripted, pixel for pixel. Update the
golden images (`--update`) only for an intended visual change.

Rules: keep things simple; match the surrounding code (Doxygen on every function, `.clang-format`).
License: Apache-2.0 (the NOTICE file and its credit lines must stay).
