# sGLGUI

A native **rendering engine for AI-driven interfaces**, written from scratch in **C++23** and **OpenGL 4.1 core**.

An application's whole UI, its layout, state and behaviour, is a JSON document that the engine renders natively and
keeps **live**. While the app runs, an AI agent (or any program) can change that document through a stream of
**revisions**. Every change is governed: permissions decide who may change what, the user reviews proposals before
they apply, and the full history can be restored, exported and shared.

Created by Riccardo Sottini. Started in 2017 as a way to learn OpenGL and C++, revived in 2026.

## Architecture
The engine is five independent modules. Each one is its own library target and talks to the others **only through
interfaces** in `core`: no module includes or links against another module's implementation. `sgl::Screen` is the
composition root that plugs them together, and any of them can be swapped for another implementation of the same
interface (another transport, another version-control backend, another document format).

```mermaid
flowchart TB
    subgraph core["core: the interfaces (header-only)"]
        direction LR
        IExec{{"Executor<br/>post work to the UI thread"}}
        IChan{{"Channel<br/>send · onMessage · onClosed"}}
        IVcs{{"VersionControl<br/>document · state · submit · review · history"}}
        IObs{{"VersionObserver<br/>documentChanged · proposalsChanged"}}
        IRules{{"DocumentRules<br/>validate · findNode · policy · locks"}}
    end

    render["<b>render</b><br/>rendering engine<br/>Application · Window · Panel · text · OpenGL"]
    format["<b>format</b><br/>JSON screen format<br/>Rules · expressions · Runtime (build, reconcile, actions)"]
    vcs["<b>vcs</b><br/>version control<br/>Repository: history · permissions · review · change sets"]
    channel["<b>channel</b><br/>communication<br/>ProcessChannel · StdioChannel · Session (protocol)"]
    screen["<b>sgl::Screen</b><br/>composition root"]

    render -.->|implements| IExec
    channel -.->|implements| IChan
    vcs -.->|implements| IVcs
    format -.->|implements| IRules
    format -.->|implements| IObs

    channel -->|uses| IExec
    channel -->|Session uses| IVcs
    vcs -->|uses| IRules
    vcs -->|notifies| IObs
    format -->|uses| IVcs
    format ==>|public API: builds panels| render

    screen --> render & format & vcs & channel
```

| Module | Namespace | Responsibility | Depends on |
|---|---|---|---|
| `core` | `sgl` | The contracts: `Executor`, `Channel`, `VersionControl`, `VersionObserver`, `DocumentRules`; `Signal`, JSON types | nlohmann/json (only for the document interfaces) |
| `render` | `sgl` | Rendering engine: windows, panel tree, layout, input, text, batched OpenGL renderer. No JSON. | `core` |
| `format` | `sgl::format` | The JSON screen format: validation and permission rules (`Rules`), `{{expressions}}`, and `Runtime`, which builds a document into panels, reconciles on change and runs actions | `core`, `render` |
| `vcs` | `sgl::vcs` | Version control for any JSON document: revisions (JSON Patch, `#id` paths), per-author policies, locks, proposals, preview, history, restore, change sets | `core` |
| `channel` | `sgl::channel` | Communication: child-process and stdio transports, and `Session`, the live JSON-lines protocol | `core` |
| `sglgui` | `sgl` | `sgl::Screen`: wires `Rules` → `Repository` → `Runtime` + `Session` | all of the above |

### How a change flows

```mermaid
sequenceDiagram
    participant Agent as AI agent / brain
    participant Chan as channel<br/>Channel + Session
    participant VCS as vcs<br/>Repository
    participant Rules as format<br/>Rules
    participant RT as format<br/>Runtime
    participant R as render<br/>Window · Panels
    actor User

    Agent->>Chan: {"type": "revision", "patch": [...]}
    Chan->>VCS: submit(revision)  [VersionControl]
    VCS->>Rules: policyFor · checkChange · lockedPaths · validate  [DocumentRules]
    alt policy "review"
        VCS-->>Chan: Proposed
        VCS->>RT: proposalsChanged()  [VersionObserver]
        RT->>R: review bar shows the proposal
        User->>R: clicks Accept
        R->>RT: onClick → accept step
        RT->>VCS: accept(id)  [VersionControl]
        VCS-->>Chan: onDecision → {"type": "applied"}
    end
    VCS->>RT: documentChanged()  [VersionObserver]
    RT->>R: reconcile: update panels in place
    Note over VCS,RT: if the runtime can't build it, the revision is rolled back
    User->>R: types, clicks
    R->>RT: Panel signals
    RT->>VCS: state changes · emit
    RT-->>Chan: {"type": "event"} back to the agent
```

1. **The screen file** describes the UI: layout, styles, state, bindings and actions ([SCREEN_FORMAT.md](SCREEN_FORMAT.md)).
2. **format** builds it into a panel tree that **render** draws with one batched draw call, and runs the actions
   when the user interacts. When state changes, the runtime updates only what changed and keeps focus, typing and drags.
3. **channel:** an outside program sends **revisions** (JSON Patch) and receives what the user does. It can be the
   **Python MCP server**, so any MCP agent such as Claude can drive the app, or any "brain" process.
4. **vcs** checks every revision against the document's rules: who sent it, what it touches, and whether the user
   must approve it first. See [GOVERNANCE.md](GOVERNANCE.md).

## The vision
The engine is the foundation for a larger idea, at the **application layer**: a user starts from a rich template
application, then **customises it by talking to it**. "Add a filter to this list", "make the sidebar collapsible",
"show the total in red when it's negative". Each request becomes a reviewed revision of the screen. The result is
stored as a **change set**, a small, portable file that works like a **plugin**: it can be shared, reviewed and
installed by others on the same template.

```mermaid
flowchart TB
    subgraph vision["Application layer: the vision (built on top of the engine)"]
        direction LR
        template["Template app<br/>e.g. CRM, editor, dashboard"]
        ask["User asks in the chat<br/>'add a filter to this list'"]
        propose["AI agent proposes<br/>a revision"]
        approve["User previews,<br/>accepts or rejects"]
        plugin[("Change set<br/>= plugin")]
        catalogue[("Shared plugins<br/>others can install")]
        template --> ask --> propose --> approve --> plugin --> catalogue
        catalogue -->|install| template
    end

    subgraph engine["sGLGUI engine: this repository"]
        direction LR
        format["format<br/>layout · state · actions"]
        renderer["render<br/>native OpenGL, live updates"]
        stream["channel + MCP server<br/>revisions in, events out"]
        governance["vcs: governance<br/>permissions · review"]
        history["vcs: history<br/>restore · export · import"]
    end

    template -.->|is a| format
    template -.->|drawn by| renderer
    propose -.->|sent through| stream
    approve -.->|enforced by| governance
    plugin -.->|exported from| history
    catalogue -.->|imported into| history
```

This repository is the **engine** that makes that possible: rendering, the live document, governance, history,
change sets and the agent interfaces. The application layer (template catalogues, a plugin marketplace) builds on top.

## Try it

```sh
cmake -S . -B build && cmake --build build

./build/chatbot_demo                       # a chat app whose UI and behaviour are a screen file
./build/chatbot_demo --brain "python3 examples/brains/rules_brain.py"
# type "dark mode", "add a counter", "bigger text": each becomes a proposal you preview, accept or reject

./build/screen_viewer examples/assets/screens/sign-in.screen.json   # any screen file, no C++
./build/chatbot_demo --plugin my-changes.json                       # start with a change set applied
```

### With Claude (MCP)
The Python MCP server starts the app and feeds the agent's changes into its stream:

```sh
claude mcp add sglgui -- python3 /path/to/sGLGUI/mcp/sglgui_mcp.py -- /path/to/sGLGUI/build/chatbot_demo
```

Then ask Claude to change the running app. Its changes arrive as proposals for you to review in the app.
See [mcp/README.md](mcp/README.md).

## Native performance
Measured on an Apple M4 Pro (release build), chat with thousands of messages, each a wrapped text bubble:

| Messages on screen | Full frame (draw + GPU) | Keystroke → UI updated | Revision applied |
|---|---|---|---|
| 200 | 0.47 ms | 1.1 ms | 1.4 ms |
| 1,000 | 0.45 ms | 4.3 ms | 4.5 ms |
| 3,000 | 0.47 ms | 12.3 ms | 12.4 ms |

- **One draw call** per frame per font: rectangles and text share a shader and a texture.
- **No work when idle:** frames are drawn only when something changed; 0% CPU while waiting for input.
- **16-byte vertices**, a grow-only index buffer, buffer orphaning, glyph atlases uploaded once.
- **Small:** the example app is a ~1.1 MB binary. No browser engine, no web view.

## Features
- **Screen format:** layout, styles, named colors, fonts, templates, state, `{{bindings}}`, `$` operators,
  `repeat` / `when`, two-way `bind`, actions with conditions and loops, custom native types
- **Live documents:** in-place reconciliation, revisions (JSON Patch, atomic, `#id` paths), a JSON-lines channel
- **Governance:** per-author policies (allow / review / deny), locked parts, protected controls, proposals with
  live preview, accept/reject, an audit history, restore, change sets (plugins)
- **Agents:** a Python MCP server, a brain-process channel; authors are assigned by the channel, never self-declared
- **UI engine:** nested panels in pixels or percentages, anchors, clipping, stacks, typed input events, mouse
  capture, hover, cursors, TrueType text with wrap and fit-to-text, a text input with caret, HiDPI

## Using the library

```cpp
#include <sgl/sgl.hpp>

int main() {
	sgl::Application app;
	sgl::Screen screen = sgl::Screen::fromFile("app.screen.json");
	sgl::Window& window = screen.createWindow(app);

	screen.onAction("save", [&](sgl::Panel&) { /* native code, if needed */ });
	return app.run();
}
```

```cmake
add_subdirectory(sGLGUI)   # or FetchContent
target_link_libraries(my_app PRIVATE sglgui::sglgui)
```

Requires CMake ≥ 3.25 and a C++23 compiler. GLFW and nlohmann/json are downloaded automatically.

## Project layout
```
modules/core/      interfaces between modules (include/sgl/core): Executor, Channel, VersionControl, DocumentRules
modules/render/    rendering engine (include/sgl/render; src/gl: all OpenGL code, src/detail: UTF-8, PPM)
modules/format/    JSON screen format: Rules, expressions, Runtime (include/sgl/format)
modules/vcs/       version control: Repository (include/sgl/vcs)
modules/channel/   transports and the live protocol Session (include/sgl/channel)
modules/screen/    sgl::Screen, the composition root, and the umbrella header sgl/sgl.hpp
mcp/               the Python MCP server
schema/            JSON Schema of the screen format
examples/          example apps, screens, fonts and a sample brain
external/          vendored glad (OpenGL loader) and stb_truetype
```
No public header exposes OpenGL or GLFW. Each module can be linked on its own (`sglgui::render`, `sglgui::vcs`, ...)
or all together through `sglgui::sglgui`.

## License
**Apache License 2.0**: free to use, modify and distribute, commercially or not. Anyone who distributes it must pass
on the [NOTICE](NOTICE) file, which credits the author. Copyright (c) 2017-2026 Riccardo Sottini. See
[LICENSE](LICENSE) and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
