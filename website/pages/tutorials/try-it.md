# Try it in a minute

Build the library, its tools and the examples, then open the Launcher:

```sh
git clone https://github.com/RiccardoSottini/AffrescoUI.git
cd AffrescoUI
cmake -S . -B build && cmake --build build
./build/affrescoui_launcher
```

You need CMake 3.25 or later and a C++23 compiler. CMake downloads the pinned dependencies (GLFW, nlohmann/json and
the others).

The Launcher lists every example app; a click opens one. Its **Settings** choose the AI provider and the model the
apps' assistants use; keys stay in the system's keychain. Open the *Flow* board, click the round assistant button in
its bottom right corner and ask for a change in plain words: the change comes back as a proposal you preview, then
accept or reject, and the board changes without restarting. To connect an agent through MCP instead, see
[an agent changes a running app](agent-review.md).

The host listens for peers on local sockets (in `/tmp/affrescoui-<uid>` on macOS and Linux unless you pass
`--socket DIR`).

## Other apps

An app is a folder under `examples/` with an `index.screen.json`, and any of them opens the same way, with no C++:

```sh
./build/affrescoui_launcher examples/flow/index.screen.json
./build/affrescoui_launcher examples/haven/index.screen.json
./build/affrescoui run website/recipes/todo-list.screen.json
```

The examples include `flow` (a Kanban board), `haven` (a smart-home dashboard), `tempo` (a music player), `streak` (a
habit tracker), `sign-in`, `code-editor`, and larger tools: an IDE (`forge`, with `--folder DIR` to open a folder), an
image editor, a presentation editor and a 3D modeller. None has a program of its own: every one runs on
`affrescoui_launcher`.

## Without a window

The command-line tool checks and draws screens headless, the way CI does:

```sh
./build/affrescoui check website/recipes/todo-list.screen.json
./build/affrescoui render website/recipes/todo-list.screen.json -o todo.png
```

`render` needs OpenGL: a display, or Xvfb on a server. Next: [a first screen](first-screen.md).
