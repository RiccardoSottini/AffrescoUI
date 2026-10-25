# Shared

What the programs in `apps/` and `examples/`, the command line and the tests share: plain helpers of a program, not
the library's.

| File | What it is |
|---|---|
| `Assets.hpp` | Where the programs find the apps and fonts: this source tree's `examples/`, or `assets/` next to the program in a release |
| `Templates.hpp` | The templates' names and folders, read from [`../../examples/templates.json`](../../examples/templates.json) |
| `SocketDir.hpp` | The checks on the socket folder, shared by the host, Forge and the CLI |
