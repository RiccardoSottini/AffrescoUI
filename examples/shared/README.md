# Shared

What more than one example uses.

| File | What it is |
|---|---|
| `Assets.hpp` | Where the programs find the templates and fonts: this source tree's `examples/`, or `assets/` next to the program in a release |
| `Templates.hpp` | The templates' names and folders, read from [`../templates.json`](../templates.json) |
| `CodeEditor.cpp`, `CodeEditor.hpp` | The `code` type of the code-editor template, registered by the host |
| `SocketDir.hpp` | The checks on the socket folder, shared by the host, the Studio, Forge and the CLI |
| `addons/` | Add-ons to `include`: `assistant/` (a chat with the connected agents, [README](addons/assistant/README.md)) |
| `components/` | Components to `include`: `charts.json` (line, bar and area charts) |
| `fonts/` | The fonts every template uses, and their licences ([`fonts/README.md`](fonts/README.md)) |
