# Shared

What more than one example uses.

| File | What it is |
|---|---|
| `Assets.hpp` | Where the programs find the templates and fonts: this source tree's `examples/`, or `assets/` next to the program in a release |
| `Templates.hpp` | The templates' names and folders, read from [`../templates.json`](../templates.json) |
| `SocketDir.hpp` | The checks on the socket folder, shared by the host, Forge and the CLI |
| `addons/` | Add-ons to `include`: `assistant/` (a chat with the connected agents, [README](addons/assistant/README.md)) |
| `components/` | Components to `include`: `charts.json` (line, bar and area charts) |
| `languages/` | Declarative languages for the `editor` (C++, CMake, JavaScript, JSON, Markdown, Python, shell, TypeScript): `include` `languages.json` for all of them |
| `fonts/` | The fonts every template uses, and their licences ([`fonts/README.md`](fonts/README.md)) |
