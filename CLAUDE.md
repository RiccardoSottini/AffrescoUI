# sGLGUI

C++ / OpenGL GUI library (started 2017, revived 2026 with Claude Code).

Project intent, history, audit and roadmap. Read this first every session:
@docs/PROJECT-INTENT.md

At the end of every session:
1. Append a journal entry to `docs/LINKEDIN-POST.md` (what was done, Claude Code approaches used, lessons learned). Only real, verified outcomes.
2. Add `docs/steps/step-X.md` (next number) with a high-level description of what was done.
3. Update the **Status** section of `docs/PROJECT-INTENT.md` with the next step.

Build & check: `cmake -S . -B build -G Ninja && cmake --build build`, then
`SGLGUI_SCREENSHOT=out.ppm ./build/chatbot_demo` and compare against `docs/media/chatbot-demo.png` (the empty first frame).
Rules: never commit; keep things simple; no tests until asked.
