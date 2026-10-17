<div align="center">

# sGLGUI

### Native apps that AI agents can rewrite while they run.<br/>Never without your permission.

[![C++23](https://img.shields.io/badge/C%2B%2B-23-00599C?logo=cplusplus&logoColor=white)](#use-it-in-your-app)
[![OpenGL 4.1 core](https://img.shields.io/badge/OpenGL-4.1%20core-5586A4?logo=opengl&logoColor=white)](#under-the-hood)
[![MCP](https://img.shields.io/badge/MCP-two--way-8B5CF6)](mcp/README.md)
[![Frame time](https://img.shields.io/badge/full%20frame-0.47%20ms-10B981)](#native-performance)
[![Platform](https://img.shields.io/badge/platform-macOS-lightgrey?logo=apple)](#quick-start)
[![License](https://img.shields.io/badge/license-Apache%202.0-blue)](LICENSE)

**[Quick start](#quick-start)** · **[Templates](#templates)** · **[How it works](#how-it-works)** ·
**[Under the hood](#under-the-hood)** ·
**[Screen format](SCREEN_FORMAT.md)** · **[Version control](VERSION_CONTROL.md)** · **[MCP server](mcp/README.md)**

<br/>

<img src=".github/media/kanban-after.png" width="860" alt="The Flow Kanban board template, rendered natively by sGLGUI after an AI agent added a Blocked column while it ran">

<sub>A native C++ / OpenGL Kanban board. Claude Code proposed a new <b>Blocked</b> column over the agent socket: new
data and a five-column layout in one revision. It was reviewed and accepted while the app kept running, and it can
be undone.</sub>

</div>

---

Today, when an AI changes an app, it edits source code, then someone rebuilds and redeploys it. **sGLGUI works
differently.** The whole UI (layout, state and behaviour) is a **live JSON document** that a native engine renders and
keeps running. People, scripts and AI agents send it **revisions**. The engine checks each one against the screen's
permissions, can show you a **live preview**, and applies it **in place in milliseconds**, while the app keeps
running. Your typing, focus and drags survive.

<table>
<tr>
<td width="50%" align="center"><img src=".github/media/kanban.png" alt="Kanban board at revision 0"><br/><sub><b>Revision 0</b>: the template as shipped</sub></td>
<td width="50%" align="center"><img src=".github/media/kanban-after.png" alt="Kanban board at revision 1"><br/><sub><b>Revision 1</b>: <code>agent/claude-code</code> added a Blocked column</sub></td>
</tr>
</table>

## Why it's different

<table>
<tr>
<td width="33%" valign="top">

### 🎨 Truly native
A from-scratch **C++23 / OpenGL** engine. No browser, no web view, a **~1 MB** app binary. A whole frame is
**one draw call** per font, and an idle app uses **0% CPU**.

</td>
<td width="33%" valign="top">

### 📄 Live documents
Layout, styles, state, `{{bindings}}`, loops and actions, all in JSON. Revisions are standard **JSON Patch**,
atomic, and reconciled **in place**.

</td>
<td width="33%" valign="top">

### 🛡️ Version control built in
**Git's model with Claude Code's approvals:** content-addressed commits, proposals as branches with **live
preview**, merge, revert and diff; `allow` / `ask` / `deny` rules, modes and *"don't ask again"*.
An AI can change your app but **can never approve its own changes**.

</td>
</tr>
<tr>
<td valign="top">

### 🤖 Agent-ready
A **two-way MCP server**: requests from your client reach the agent, and the agent's revisions and replies come back.
Works with Claude Code out of the box.

</td>
<td valign="top">

### 🔌 Any number of peers
Clients, agents and scripts connect over local sockets. The engine doesn't care what they are, only **who they
are**, and a peer's identity is set by the socket it connects to.

</td>
<td valign="top">

### 🧩 Changes become plugins
Export any run of revisions as a **change set**: a small, portable file that anyone with the same template
can review and install.

</td>
</tr>
</table>

## How it works

```mermaid
%%{init: {"theme": "base", "flowchart": {"curve": "basis", "nodeSpacing": 28, "rankSpacing": 46, "padding": 14}, "themeVariables": {"fontFamily": "Helvetica, Arial, sans-serif", "fontSize": "14px", "lineColor": "#94A3B8", "primaryTextColor": "#0F172A", "edgeLabelBackground": "#FFFFFF"}}}%%
flowchart LR
    subgraph P["Who changes it"]
        direction TB
        you["<b>You</b><br/>sGLGUI Studio"]
        ai["<b>AI agent</b><br/>Claude Code · MCP"]
        any["<b>Scripts & apps</b><br/>any socket client"]
    end

    subgraph V["Version control"]
        direction TB
        rules{"<b>Permission rules</b><br/>deny · ask · allow"}
        pr["<b>Proposal</b><br/>branch · live preview"]
        no["<b>Rejected</b><br/>nothing changes"]
    end

    subgraph E["Engine"]
        direction TB
        main[("<b>main</b><br/>SHA-256 commits")]
        rt["<b>Runtime</b><br/>reconcile in place"]
        gl["<b>Renderer</b><br/>C++23 · OpenGL"]
    end

    app(["<b>Your running app</b><br/>never restarts"])

    you & ai & any -- "revision" --> rules
    rules -- "allow" --> main
    rules -- "ask" --> pr
    rules -- "deny" --> no
    pr -- "you accept" --> main
    main --> rt --> gl --> app

    classDef peer fill:#FFFFFF,stroke:#CBD5E1,stroke-width:1px,color:#0F172A
    classDef gate fill:#FFFBEB,stroke:#D97706,stroke-width:1.5px,color:#78350F
    classDef wait fill:#FFFFFF,stroke:#D97706,stroke-width:1px,stroke-dasharray:4 3,color:#78350F
    classDef stop fill:#FFFFFF,stroke:#E11D48,stroke-width:1px,color:#9F1239
    classDef core fill:#EEF2FF,stroke:#6366F1,stroke-width:1.5px,color:#1E1B4B
    classDef out fill:#0F172A,stroke:#0F172A,color:#FFFFFF
    class you,ai,any peer
    class rules gate
    class pr wait
    class no stop
    class main,rt,gl core
    class app out
    style P fill:#F8FAFC,stroke:#E2E8F0,color:#64748B
    style V fill:#F8FAFC,stroke:#E2E8F0,color:#64748B
    style E fill:#F8FAFC,stroke:#E2E8F0,color:#64748B
    linkStyle 0,1,2 stroke:#94A3B8
    linkStyle 3 stroke:#16A34A,stroke-width:2px
    linkStyle 4 stroke:#D97706,stroke-width:1.5px
    linkStyle 5 stroke:#E11D48,stroke-width:1.5px
    linkStyle 6 stroke:#16A34A,stroke-width:2px
    linkStyle 7,8,9 stroke:#6366F1,stroke-width:2px
```

1. **Anyone can propose a change.** The app (`sglgui_host`) only renders a screen, here the *Flow* Kanban board, and
   listens for peers on local sockets. You use the **sGLGUI Studio**. An AI agent connects through the MCP server,
   which works both ways: your requests are pushed into its session, and its replies come back to the Studio. Any
   script can connect too. Every change is a **revision**: a JSON Patch.
2. **The permission rules decide**, written like Claude Code's: `deny` refuses, `ask` sends the change to you, `allow`
   applies it. Rules judge what a change *does*, not how its patch is written. The templates let the Studio change
   anything, always ask before the agent changes behaviour, and protect the status bar.
3. **You review proposals.** A proposal is a branch, like a pull request: the agent can keep refining it while you
   preview it live in the app. Then **Accept**, **Always allow** (don't ask again for that part of the screen) or
   **Reject** with a reason. Switch the mode to *accept edits* or *plan* at any time.
4. **Accepted changes become commits on `main`**, with SHA-256 ids and both the author and the approver recorded. The
   runtime updates the running app in place: no restart, and focus and typing are kept. Any commit can be reverted
   or restored. See [VERSION_CONTROL.md](VERSION_CONTROL.md).

<p align="center">
<img src=".github/media/studio.png" width="640" alt="The sGLGUI Studio: ask the AI, direct JSON Patch, review with modes and grants, activity and history"><br/>
<sub>The <b>sGLGUI Studio</b>, a client that is itself an sGLGUI screen: ask the AI, patch directly, review, always allow, revert, restore.</sub>
</p>

## Quick start

```sh
cmake -S . -B build && cmake --build build

./build/sglgui_host                  # the Kanban board template, listening in /tmp/sglgui-<uid>
./build/sglgui_studio                # the client: apply patches, review, ask the AI
```

Then connect an AI agent through MCP ([mcp/README.md](mcp/README.md)):

```sh
claude mcp add sglgui -- python3 /path/to/sGLGUI/mcp/sglgui_mcp.py
claude --dangerously-load-development-channels server:sglgui   # requests are pushed into the session
```

Type *"add a Blocked column between In progress and Review"* in the Studio. The request reaches Claude, Claude
changes the running board, and the change shows up in the Studio for review. Click **Accept** and the app updates
in place, or **Always allow** to stop being asked about that part of the screen. Switch to **Accept edits** to let
changes apply as they arrive.

```sh
./build/sglgui_host --mode acceptEdits                              # agent changes apply unless a rule asks
./build/sglgui_host examples/assets/screens/smart-home.screen.json   # any template or screen file, no C++
./build/sglgui_host --plugin my-changes.json                         # start with a change set applied
```

Requires CMake ≥ 3.25 and a C++23 compiler. GLFW and nlohmann/json are downloaded automatically. Tested on macOS; the
socket and process channels are POSIX.

## Templates

Five starting points, each a single `.screen.json` file with its layout, state and behaviour, and each ready to be
reshaped live by an agent. Open any of them with `./build/sglgui_host examples/assets/screens/<name>.screen.json`.

<table>
<tr>
<td width="50%" valign="top"><img src=".github/media/kanban.png" alt="Flow, a Kanban board"><br/><b>Flow</b> · <code>kanban</code><br/><sub>Columns, cards, label filters, moving cards, quick add.</sub></td>
<td width="50%" valign="top"><img src=".github/media/smart-home.png" alt="Haven, a smart-home panel"><br/><b>Haven</b> · <code>smart-home</code><br/><sub>Rooms, light switches and dimming, thermostat, blinds, scenes.</sub></td>
</tr>
<tr>
<td valign="top"><img src=".github/media/music-player.png" alt="Tempo, a music player"><br/><b>Tempo</b> · <code>music-player</code><br/><sub>Playlists, track list, now playing, play/pause, next, volume.</sub></td>
<td valign="top"><img src=".github/media/code-editor.png" alt="Forge, a code editor shell"><br/><b>Forge</b> · <code>code-editor</code><br/><sub>Explorer, tabs, highlighted code, a terminal that answers.</sub></td>
</tr>
<tr>
<td valign="top"><img src=".github/media/habit-tracker.png" alt="Streak, a habit tracker"><br/><b>Streak</b> · <code>habit-tracker</code><br/><sub>Daily habits, streaks, a clickable 20-week grid, weekly progress.</sub></td>
<td valign="top"><br/><sub>Ideas for an agent to try: <i>"add a Blocked column"</i>, <i>"a Movie night scene that dims every light"</i>,
<i>"show the album name under the track title"</i>, <i>"a dark theme for the habit tracker"</i>.</sub></td>
</tr>
</table>

## Under the hood

The engine is **five independent modules**. Each one is its own library target and talks to the others **only through
interfaces** in `core`: no module includes or links another module's implementation. `sgl::Screen` plugs them
together, and any of them can be swapped for another implementation of the same interface (another transport,
another version-control backend, another document format).

```mermaid
flowchart TB
    subgraph CH["🔌 channel · sgl::channel: peers in"]
        direction LR
        SS["<b>SocketServer</b><br/>studio.sock · agent.sock"] --> SC["<b>SocketChannel</b><br/>JSON lines · reader thread<br/>Executor::post to the UI thread"] --> SE["<b>Session</b> per peer<br/>author fixed by the socket"]
    end

    subgraph VC["🛡️ vcs · sgl::vcs: version control"]
        direction LR
        SUB["<b>submit</b><br/>JSON Patch · #id paths"] --> POL{"rules<br/>deny · ask · allow<br/>+ mode, grants"}
        POL -- "ask" --> PRO["<b>Proposal</b><br/>a branch · live preview"] -- "merge" --> COM
        POL -- "allow" --> COM["<b>Commit on main</b><br/>SHA-256 · log · revert<br/>change sets"]
        POL -- "deny" --> REJ["<b>Rejected</b><br/>nothing changes"]
    end

    subgraph FM["📄 format · sgl::format: the live document"]
        direction LR
        RU["<b>Rules</b><br/>validate · permission<br/>rules · locks"] ~~~ RT["<b>Runtime</b><br/>build · reconcile<br/>in place"] --- EXP["<b>Expressions</b><br/>{{bindings}}<br/>$operators"] --- ACT["<b>Actions</b><br/>set · push · if<br/>forEach · emit"]
    end

    subgraph RD["🎨 render · sgl: pixels out"]
        direction LR
        TREE["<b>Panel tree</b><br/>anchors · px / %"] --> LAY["<b>Layout pass</b><br/>only when dirty"] --> QR["<b>QuadRenderer</b><br/>rects + glyphs<br/>one batch"] --> GL[["glDrawElements × 1"]]
        IN["<b>Input</b><br/>hit-test · focus<br/>capture · hover"]
    end

    SE == "VersionControl" ==> SUB
    SUB -. "DocumentRules" .-> RU
    COM == "VersionObserver" ==> RT
    RT == "creates and updates panels" ==> TREE
    IN -. "onClick · typing" .-> ACT
    COM -. "onCommit · onReviewChanged: to every peer" .-> SE

    style CH fill:#E0F2FE,stroke:#0284C7,stroke-width:2px,color:#0C4A6E
    style VC fill:#FEF3C7,stroke:#D97706,stroke-width:2px,color:#78350F
    style FM fill:#D1FAE5,stroke:#059669,stroke-width:2px,color:#064E3B
    style RD fill:#EDE9FE,stroke:#7C3AED,stroke-width:2px,color:#4C1D95
    classDef node fill:#FFFFFF,stroke:#64748B,color:#0F172A
    class SS,SC,SE,SUB,PRO,COM,RU,RT,EXP,ACT,TREE,LAY,QR,IN node
    classDef decision fill:#F59E0B,stroke:#B45309,color:#111
    class POL decision
    classDef hot fill:#111827,stroke:#7C3AED,stroke-width:2px,color:#fff
    class GL hot
    classDef no fill:#FFE4E6,stroke:#E11D48,color:#881337
    class REJ no
```

| Module | Namespace | Responsibility | Depends on |
|---|---|---|---|
| `core` | `sgl` | The contracts: `Executor`, `Channel`, `VersionControl`, `VersionObserver`, `DocumentRules`; `Signal`, JSON types | nlohmann/json (only for the document interfaces) |
| `render` | `sgl` | Rendering engine: windows, panel tree, layout, input, text, clipboard, batched OpenGL renderer. No JSON. | `core` |
| `format` | `sgl::format` | The JSON screen format: validation and permission rules (`Rules`), `{{expressions}}`, and `Runtime`, which builds a document into panels, reconciles on change and runs actions | `core`, `render` |
| `vcs` | `sgl::vcs` | Git-like version control for any JSON document: content-addressed commits, `main`, proposals as branches, merge, preview, log, diff, revert, restore, change sets; allow / ask / deny rules judged by effect, modes, grants | `core` |
| `channel` | `sgl::channel` | Communication: local-socket server and client, child-process and stdio transports, and `Session`, the live JSON-lines protocol | `core` |
| `sglgui` | `sgl` | `sgl::Screen`: wires `Rules` → `Repository` → `Runtime`, plus one `Session` per peer, relaying requests between peers | all of the above |

### The life of a revision

```mermaid
sequenceDiagram
    autonumber
    actor Person
    participant Studio as 🧑 sglgui_studio
    participant Peers as 🔌 sgl::Screen<br/>Sessions
    participant VCS as 🛡️ vcs<br/>Repository
    participant RT as 🎨 format Runtime<br/>+ render
    participant MCP as MCP server
    participant Agent as 🤖 AI agent

    Person->>Studio: "add a dark theme"
    Studio->>Peers: {"type": "request"}  (studio.sock)
    Peers->>MCP: relayed, "from": "studio"  (agent.sock)
    MCP->>Agent: channel event (or wait_for_request)
    Agent->>MCP: get_screen, submit_revision
    MCP->>Peers: {"type": "revision", "patch": [...]}
    Peers->>VCS: submit(revision)  [VersionControl]
    Note over VCS: deny · ask · allow rules, mode and grants, validity  [DocumentRules]
    rect rgba(245, 158, 11, 0.15)
    alt no rule allows it (mode "default")
        VCS-->>Peers: a proposal branch → {"type": "review"} to every peer
        Person->>Studio: Preview, then Accept (or Always allow)
        Studio->>Peers: {"type": "preview"}, {"type": "accept", "head"}  (reviewers only)
        Peers->>VCS: preview(id), accept(id, approval) → merge commit
    end
    end
    rect rgba(139, 92, 246, 0.15)
    VCS->>RT: documentChanged()  [VersionObserver]
    RT->>Person: panels updated in place
    Note over VCS,RT: if the runtime can't build it, the revision is rolled back
    end
    VCS-->>Peers: onCommit → {"type": "committed"} to every peer
    Agent->>MCP: reply("dark theme is on")
    MCP->>Peers: {"type": "reply"}
    Peers->>Studio: relayed
```

The app's own interactions (clicks, typing, actions in the screen file) change its state locally. `emit` steps send
events to every peer.

## Native performance

Measured on an Apple M4 Pro (release build) with an earlier chat template: a screen with thousands of wrapped text
items.

| Items on screen | Full frame (draw + GPU) | Keystroke → UI updated | Revision applied |
|---|---|---|---|
| 200 | **0.47 ms** | 1.1 ms | 1.4 ms |
| 1,000 | **0.45 ms** | 4.3 ms | 4.5 ms |
| 3,000 | **0.47 ms** | 12.3 ms | 12.4 ms |

- **One draw call** per frame per font: rectangles and text share a shader and a texture.
- **No work when idle:** frames are drawn only when something changed, and the app uses 0% CPU while it waits for input.
- **16-byte vertices**, a grow-only index buffer, buffer orphaning, and glyph atlases uploaded once.
- **Small:** an example app is a ~1 MB binary. No browser engine, no web view.

## Use it in your app

```cpp
#include <sgl/sgl.hpp>

int main() {
	sgl::Application app;
	sgl::Screen screen = sgl::Screen::fromFile("app.screen.json");
	screen.createWindow(app);

	screen.onAction("save", [&](sgl::Panel&) { /* native code, if needed */ });
	screen.listen(app, "/tmp/my-app/agent.sock", "agent"); // optional: let other programs change it live
	return app.run();
}
```

```cmake
add_subdirectory(sGLGUI)   # or FetchContent
target_link_libraries(my_app PRIVATE sglgui::sglgui)
```

<details>
<summary><b>Everything the engine does</b></summary>

- **Screen format:** layout, styles, named colors, fonts, templates, state, `{{bindings}}`, `$` operators,
  `repeat` / `when`, two-way `bind`, actions with conditions and loops, custom native types.
  Spec: [SCREEN_FORMAT.md](SCREEN_FORMAT.md), with a [JSON Schema](schema/sglgui-screen.schema.json) for editor
  autocompletion.
- **Live documents:** in-place reconciliation, revisions (JSON Patch, atomic, `#id` paths), any number of peers
  over local sockets (or any channel), broadcasts of commits and proposals, request/reply relaying between peers.
- **Version control:** Git's model (SHA-256 commits, an append-only `main`, proposals as branches merged with a merge
  commit, log, diff, revert, restore, `HEAD~1` names) with Claude Code's approvals (`allow` / `ask` / `deny` rules
  judged by effect, modes `default` / `acceptEdits` / `plan`, "don't ask again" grants), reviewers, protected
  controls, approvals pinned to the version reviewed, change sets (plugins).
- **Agents:** a two-way Python MCP server (requests pushed into the agent as channel events, or pulled; changes and
  replies sent back). Authors are assigned by the socket a peer uses, never self-declared.
- **UI engine:** nested panels in pixels or percentages, anchors, clipping, stacks, typed input events, mouse
  capture, hover, cursors, TrueType text with wrap and fit-to-text, a text input with caret and paste, HiDPI.
- **Examples:** `sglgui_host` (any screen, the Kanban template by default), five templates, `sglgui_studio` (a client that is
  itself an sGLGUI screen), and the MCP server.

</details>

## The vision

The engine is the foundation for a bigger idea at the **application layer**: start from a rich template app, then
**customise it by talking to it**. *"Add a filter to this list"*, *"make the sidebar collapsible"*, *"show the total
in red when it's negative"*. Each request becomes a reviewed revision of the screen. The result is stored as a
**change set**, a small, portable file that works like a **plugin**: others with the same template can share,
review and install it.

```mermaid
flowchart LR
    template["📦 Template app<br/>board · player · editor"]
    ask["💬 You ask<br/>'add a filter to this list'"]
    propose["🤖 An agent proposes<br/>a revision"]
    approve["✅ You preview,<br/>accept or reject"]
    plugin[("🧩 Change set<br/>= plugin")]
    catalogue[("🌍 Shared plugins<br/>others can install")]
    template --> ask --> propose --> approve --> plugin --> catalogue
    catalogue -. "install" .-> template

    classDef step fill:#EEF2FF,stroke:#6366F1,color:#312E81
    classDef win fill:#10B981,stroke:#047857,color:#fff
    class template,ask,propose,approve step
    class plugin,catalogue win
```

This repository is the **engine** that makes that possible: rendering, the live document, version control (permissions, review, history),
change sets and the agent interfaces, plus a template and a client to show them off. The application layer
(template catalogues, a plugin marketplace) builds on top.

## The story

<img align="right" src=".github/media/2020-layout-demo.png" width="200" alt="The original layout demo, running again in 2026">

sGLGUI started in **December 2017**, when I was **16**, as a way to teach myself OpenGL and C++: nested panels,
pixel/percent sizing, anchors and a Qt-style signal/slot system, all from scratch. In 2018 I added a text area. It
collected keystrokes but **could never draw them**, and the project stalled. A restart in 2020 rewrote the layout
engine (the image on the right is that demo), then stalled again.

In **2026** I revived it with **Claude Code**. We ported it to OpenGL 4.1 core, rewrote it in C++23, finally rendered
text, and then took it further than the original plan: UIs described in JSON, live revisions, version control and a
two-way MCP bridge for AI agents. The layout ideas from 2017 still drive every frame.

```mermaid
timeline
    2017 : Started at 16 : nested panels, anchors, signal → slot events
    2018 : A text area that can't draw text : the project stalls
    2020 : Restarted : new layout engine, stalled again
    2026 : Revived with Claude Code : C++23 · OpenGL 4.1 core · real text : live JSON screens · version control · MCP
```

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
examples/          sglgui_host, sglgui_studio, templates (kanban, smart-home, music-player, code-editor,
                   habit-tracker), studio and sign-in screens, fonts
external/          vendored glad (OpenGL loader) and stb_truetype
```

No public header exposes OpenGL or GLFW. Each module can be linked on its own (`sglgui::render`, `sglgui::vcs`, ...)
or all together through `sglgui::sglgui`.

## License

**Apache License 2.0**: free to use, modify and distribute, commercially or not. Anyone who distributes it must pass
on the [NOTICE](NOTICE) file, which credits the author. See [LICENSE](LICENSE) and
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

<div align="center">
<br/>

**Created by [Riccardo Sottini](https://github.com/RiccardoSottini)** · 2017–2026

<sub>If sGLGUI made you think "wait, an AI can change a native app *while it runs*?", leave a ⭐.</sub>

</div>
