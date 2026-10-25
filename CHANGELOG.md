# Changelog

All notable changes are listed here. AffrescoUI follows [Semantic Versioning](https://semver.org): from 1.0.0 on,
only a major version may change the C++ API or the screen format in a way that breaks what works.

## 1.0.0 (2026-10-25)

The first release of AffrescoUI, which started in 2017 as sGLGUI. A UI is a folder of JSON documents that people and
AI agents change live, under version control, drawn by a C++23 engine on OpenGL 4.1 (or ES 3.0) on macOS, Linux,
Windows and the web.

- **Three layers.** Apps are folders of JSON, with nothing compiled for them. Kits and themes (toolbars, menus, docks,
  dialogs, design tokens) are JSON files in a module of their own, `modules/themes`, which the format finds through an
  interface in `core`; a document's own components still win. Every other module holds mechanisms only, and a CI
  check keeps app names and theme colours out of them.
- **Apps are folders.** An `app.json` gives the app's id, name, icon and entry screen, its themes, the capabilities it
  asks for (each with a reason), its data models with their migrations, and its seed data (JSON or CSV).
  `affrescoui check` checks it, and `Screen::fromApp` loads it. The person's data (history, persisted state, records)
  lives in their own data folder, per app.
- **The host is a browser.** `affrescoui_launcher` opens an app's folder, its `app.json` or a single screen, and with
  no file shows the Launcher. Nothing is allowed by default: the first time an app needs a capability (`network`,
  `folders`, `programs`, `audio`, `assistant`, `files`, `windows`, `channels`), the host asks, for that app only, and
  remembers the answer. `--permissions APP` forgets the answers and `--grant KEY` allows one capability for one run.
  A revision can never widen what an app may reach.
- **Version control** (`affrescoui::vcs`): SHA-256 commits and an append-only history; proposals as branches with a
  live preview; revert, restore and diff; `allow` / `ask` / `deny` rules, modes and grants, with approvals pinned to
  the version reviewed; change sets. Steps that write, run programs or reach the person's data are theirs to accept.
- **Peers and agents:** any number of peers over local sockets, each one's author set by its socket; a two-way MCP
  server (`mcp/`); an assistant chat in every app through `modules/intelligence`, with keys only in the system's
  secret store.
- **The screen format** (version 2, with a JSON Schema and `affrescoui migrate` from version 1): layout, state and
  expressions, components, styles and themes, events, shortcuts, drag and drop, animations, channels, windows and
  scenarios.
- **General services and nodes:** a `terminal` node on a `pty` service; an `lsp` language client; git status and
  branches, diffs, patches and edits; `parseJsonc`, `shellLine`, `qrCode`, `saveFile`, `openFile`; `export` to vector
  PDF, PNG and glTF; a screen's other windows; an audio spectrum; picture editing (`image` steps for sizes, transforms,
  layers, selections and a histogram); 3D scene x-ray and lighting; tab, editor and text run styles; the scenario
  steps `choose`, `propose` and `rightClick`. Every reader of untrusted input is fuzzed.
- **Hidden mode for tests:** `AFFRESCOUI_HIDDEN=1` runs any app with no window on screen; scenarios fail on a
  handler's error unless they expect it.
- **Twelve apps and two screens**, each with a look of its own and controls that all work or say why not: Arcadia
  (game library), Calamaio (text editor), Flow (issue tracker), Forge (IDE), Haven (smart home), Modello (3D scenes),
  Orrery (solar system), Quaderno (code editor), Sinopia (presentations), Streak (habits), Tempera (painting), Tempo
  (music), the Sign-in screen and the Launcher.
- **Security:** channels need their own capability and stay in the host's socket folder; an app's id is bound to its
  folder; git runs only on the folder's own repository, with no configured programs; expression nesting, decoded
  images, app includes, numbers read from text and open documents are bounded; network sockets are close-on-exec;
  origin wildcards need two labels; the MCP server never splits a command line and reads files only where it is
  allowed to. No tracked picture carries metadata.
- **Releases:** archives and an AppImage with every app folder, the fonts and the notices, checked on clean machines:
  every app is checked, and on macOS and Linux each one draws its first frame from the unpacked archive.

Removed before this release: the `code` node type (use `editor`) and the separate app programs (`affrescoui_forge`,
`affrescoui_tempera`, `affrescoui_sinopia`): every app runs on `affrescoui_launcher`.
