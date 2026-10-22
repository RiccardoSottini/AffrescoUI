# Examples

One folder per application: its screens, scenarios, natives, assets and a short README. What more than one of them
uses is in `shared/`. None of this is part of the library; each uses only its public API.

```
examples/
├── templates.json      the shipped templates: each one's name and folder (read by the tests, the tools, the web
│                       build and the release archive)
├── shared/             Assets.hpp, Templates.hpp, CodeEditor (the "code" type), SocketDir.hpp, components/, fonts/
│
├── host/               affrescoui_host, the stock host that runs any screen
├── studio/             affrescoui_studio, the person's client, and its template "studio"
├── flow/               template "kanban": a Kanban board (what the host opens by default)
├── haven/              template "smart-home": a smart-home dashboard, images/
├── tempo/              template "music-player": a music player, images/
├── streak/             template "habit-tracker": a habit tracker
├── sign-in/            template "sign-in": sign in, create an account, signed in
├── code-editor/        template "code-editor": Quaderno, a code editor
│
├── arcadia/            a game launcher built only from the public docs and tools, covers/, its end-to-end test
├── forge/              affrescoui_forge, a demo IDE: features/, catalog/, its scenarios
├── server/             affrescoui_server, the document server, and deploy/ (Caddy, nginx)
├── embed/              affrescoui_embed, a screen inside a host's own OpenGL scene (C API)
└── surfaces/           affrescoui_surfaces, native OpenGL drawing in "surface" nodes
```

A template is a screen document with no C++ of its own: any of them runs in the host.

```sh
./build/affrescoui_host examples/haven/smart-home.screen.json
./build/affrescoui test examples/haven/smart-home.scenarios.json
```

A screen's relative paths (its `include`s, fonts, images) are read from its own folder, so the templates name their
fonts `../shared/fonts/...`. A new template gets a folder and a line in `templates.json`, which the scenarios test,
the tools, the web build and the release archive read (CI's scenarios step names the folders too).

The programs find this folder through `Assets.hpp`: in a build, `examples/` in the source tree; in a release
(`tools/release`), `assets/` next to the program, which has the template folders and `shared/` laid out the same way,
without the C++ sources.
