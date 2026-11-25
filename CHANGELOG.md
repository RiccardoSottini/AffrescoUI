# Changelog

All notable changes are listed here. AffrescoUI follows [Semantic Versioning](https://semver.org): from 1.0.0 on,
only a major version may change the C++ API or the screen format in a way that breaks what works.

## 2.0.0 (2026-11-25)

Apps are now written in **AffrescoUI Lisp**, a sandboxed Clojure dialect. A program builds its document and runs its
functions while the app is open, and keeps everything a document has: history, diffs, permissions and review. JSON
still works everywhere. The major version marks this new way of writing apps. The C++ API and screen format version
2 stay compatible: what worked on 1.0.0 still works. The one change for C++ users is `find_package(AffrescoUI 2.0)`,
since the package accepts its own major version only.

- **The Lisp language** (`modules/lisp`, RFCs 0005 and 0006): a notation for screen documents, then programs that run.
  `defscreen`, `defview`, `defstate` and `defcomputed` replace the JSON tree, with docstrings and locales. It is a
  documented, sandboxed subset of Clojure: everyday forms, strings, sets, reader forms and `ns`. Its score against a
  cross-dialect Clojure test suite is measured, and a test keeps it from going down. Every intended difference from
  Clojure is written down with its reason. Authority is the least among the authors whose code ran.
- **Every example in both notations:** each app has a `json/` and a `lisp/` folder, and the Launcher's Settings
  choose which opens (Lisp by default). Every Lisp version is an idiomatic program, held to the dialect's style by a
  check.
- **Connect an editor to a running app:** `--nrepl` starts an nREPL server. The person pairs the editor once, and
  then it evaluates forms as the person would.
- **Point and ask:** in every app with the assistant, the **Point** button (Ctrl+Alt+A, Cmd+Alt+A on macOS) picks an
  element on screen, and the request names it for the assistant or an MCP agent.
- **The assistant changes programs:** the built-in assistant edits a program's definitions. It says "Done." only when
  something changed.
- **Choose the assistant's AI:** the Launcher's Settings offer Claude Code, GitHub Copilot (the person's own `copilot`
  command line and sign-in) or a local model (Ollama, LM Studio or llama.cpp's `llama-server`). A local model that
  can't call tools still works: it answers in JSON instead.
- **Version control:**
  - Undo an earlier change and keep what was built on it. Where a later change depends on it, the person chooses.
  - A public three-way merge (`vcs/Merge.hpp`) keeps key order and lists each conflict.
  - The changes window shows every change file by file, as a git-style diff (Ctrl+Shift+D, Cmd+Shift+D on macOS).
- **Commands as tools:** agents list an app's commands with typed arguments, run them under their own authority and
  the app's run rules, and undo their runs.
- **Share and install:** a person's version of an app is one file. A feature can hold program code, which runs with
  its author's authority. Installing is a proposal the person reviews, from a file or from the signed `catalogue/`
  in the Launcher.
- **Text and translations:** `t()` covers command, menu and setting titles and `window.title`. `affrescoui strings`
  finds, extracts, requires and splits an app's text into one file per language.
- **Faster updates:** dependency tracking for state, lists, revisions and programs, so an update costs what it
  touches. An idle app runs no update.
- **Sharper text:** text is hinted below a display scale of 2, and glyphs sit on whole pixels at 2x and above. On
  Windows and Linux on X11, windows follow the display's scale (125% to 400%). The rendering check covers 1.25x,
  1.5x and 1.75x, with a measure of text sharpness.
- **Every example app revamped:** missing features added, glitches fixed and a better look, in both notations. The
  capabilities the apps needed went into the library as general features.
- **Fixes:**
  - A closed window's timers no longer keep the event loop spinning.
  - Code that an app's own file ships is no longer confined as if a feature had installed it.
  - Bugs found by the sanitizers and the fuzzers are fixed.

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
