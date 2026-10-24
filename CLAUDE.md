# AffrescoUI

C++23 / OpenGL 4.1 GUI library whose UIs are JSON screen documents that people and AI agents change live, under
version control (started 2017 as sGLGUI). Overview and architecture: `README.md`. Format: `SCREEN_FORMAT.md`.
Version control and permissions: `VERSION_CONTROL.md`.

Build & check: `cmake -S . -B build -G Ninja && cmake --build build`, then
`AFFRESCOUI_SCREENSHOT=out.ppm ./build/affrescoui_host` renders the first frame of the Launcher to `out.ppm`
(the Kanban template: `AFFRESCOUI_SCREENSHOT=out.ppm ./build/affrescoui_host examples/flow/index.screen.json`).
`AFFRESCOUI_SCREENSHOT_AFTER=<ms>` captures after running that long (e.g. while peers change it).
MCP server (two-way): `mcp/affrescoui_mcp.py`. Studio client: `./build/affrescoui_studio`.
After any change, `python3 tests/render/check.py` must pass: every template, scripted, pixel for pixel. Update the
golden images (`--update`) only for an intended visual change.

Rules: keep things simple; match the surrounding code (Doxygen on every function, `.clang-format`).
License: Apache-2.0 (the NOTICE file and its credit lines must stay).

Parallel work (agents and contributors): one roadmap item per branch `roadmap/<id>`, one draft PR to `master`.
Touch only the files the item needs, and leave `README.md` and `CHANGELOG.md` to the maintainer: describe user-visible
changes in the PR. Before a PR is ready, rebase on `origin/master`, build with warnings as errors, and pass the
rendering check without `--update`. Never push to `master`, merge or tag. For live tests, pass the host, the Studio and the
MCP server the same short folder of your own (`--socket out/s`, and `AFFRESCOUI_SOCKET_DIR=out/s` for the channels
inside screens), so parallel sessions don't meet.
