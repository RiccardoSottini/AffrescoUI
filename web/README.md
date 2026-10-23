# AffrescoUI in a browser tab

A static site that runs AffrescoUI compiled to WebAssembly with Emscripten, drawing with WebGL2 (the library's
OpenGL ES 3.0 backend). It has three kinds of page:

- `index.html`: the template picker, a link per template;
- `<template>.html`: one template in the host, the same program as `affrescoui_host` on a desktop;
- `studio.html`: the Studio, with a form that connects it to a running host or document server.

The files here are the site's sources; `tools/web/build.sh` builds the programs and writes the site.

## Building

You need emsdk **6.0.10** (`AFFRESCOUI_EMSDK_VERSION` in `cmake/AffrescoUIWeb.cmake`; another release builds, and the
script warns), Ninja, CMake and Python 3.

```sh
git clone https://github.com/emscripten-core/emsdk.git
emsdk/emsdk install 6.0.10 && emsdk/emsdk activate 6.0.10 && source emsdk/emsdk_env.sh
tools/web/build.sh          # the build in build-web/, the site in build-web/site/
```

The build is Release. It makes `affrescoui_host` and `affrescoui_studio` for the browser, and the command-line tool
`affrescoui` for Node, which plays the templates' scenarios on the same WebAssembly:

```sh
node build-web/affrescoui.js test examples/flow/index.scenarios.json
```

The templates' folders (`examples/templates.json`) and `examples/shared` are preloaded into each program under `/assets`
(`/assets/flow`, `/assets/shared`, ..., without their C++ sources), in the `.data` file next to its `.wasm`.

Measured with emsdk 6.0.10 (Release, `gzip -9`):

| File | Size | Gzipped |
|---|---|---|
| `affrescoui_host.wasm` | 6.5 MB | 2.18 MB |
| `affrescoui_studio.wasm` | 6.3 MB | 2.12 MB |
| `affrescoui_host.data`, `affrescoui_studio.data` (the assets, each) | 2.06 MB | 0.88 MB |

## Getting the build

Nothing here deploys. The site is plain files: the maintainer's GitHub Release carries them as
`affrescoui-web-<version>.zip` (checksummed and attested). Unzip it and serve the folder with a static server you run
yourself that serves `.wasm` as `application/wasm`. The read-only workflow `.github/workflows/web.yml` builds the same
folder on pull requests and keeps it as the run's `web-site` artifact; nothing uploads it anywhere.

Each page carries its Content Security Policy in a `<meta>` tag: scripts from the site only (and
`'wasm-unsafe-eval'`, which WebAssembly needs), styles from the site, no forms posted anywhere, no `<base>`, no
plug-ins, and no referrer. A host that adds headers can repeat them; nothing else is needed.

## What the site trusts

Nothing from the page's address. Which program a page starts and which template it shows come from its own `<body>`
(`data-program`, `data-screen`), written by the build, and `site.js` checks both against fixed lists before loading
anything. A query string or fragment is never read.

## The Studio

A browser tab can't open the local sockets the desktop Studio uses, so the Studio in a tab connects to a remote-peer
server (`affresco::net::PeerServer`, `modules/net/README.md`), such as the document server:

```sh
affrescoui_server --folder screens --listen 127.0.0.1:7420 --origin https://your-site.example
```

- Off the loopback, the server needs `--allow-remote` and TLS in front of it (`apps/server/deploy/`, Caddy or nginx),
  so the page connects with `wss://`. `ws://` is accepted on the loopback only, by the page's policy and by the
  library.
- `--origin` names the site's origin: the server refuses an upgrade from any other page.
- The token goes in the form's password field. It stays in the Studio's memory and leaves it only as the connection's
  first message (a browser's WebSocket can't send an `Authorization` header). It is never put in a URL, the page's
  storage or a log, and the form clears the field after connecting.
- Every message is bounded by the client's limits, as on a desktop.

A screen whose `"channels"` entry has a `ws://` or `wss://` address (SCREEN_FORMAT.md, "Channels and persistence")
connects the same way, natively through the net module's client and in a tab through the browser's WebSocket.

## What a browser can't do

The services registry reports these missing, and a screen that declares them gets a refusal it can show, never a
crash:

| Missing | Why |
|---|---|
| Processes and JSON-RPC programs | A page runs no programs |
| Pseudo terminals | The same |
| Watching folders | Files live in the page's memory (MEMFS) and nothing else changes them |
| Local sockets (the host's `studio.sock`, `agent.sock`) | A page can't listen; use a remote-peer server |
| A peer server, a sign-in's loopback redirect | The same: a sign-in uses a window of the page instead |

`fs` works, on the page's own folder in memory: `/assets` and what the screen writes, gone when the tab closes.
The network uses the browser's `fetch` and `WebSocket` (with the library's allowlist and limits, credentials never
sent and redirects refused), not libcurl.

## One thread, and what it costs

The build has no threads: they need `SharedArrayBuffer`, which needs a cross-origin-isolated page (headers a plain
static host may not send). So:

- The main loop runs on `emscripten_set_main_loop`, a frame per browser animation frame, instead of a loop that waits
  for events.
- Background tasks run inline on the main thread: a long one (a big search, a large file) holds up the frame it runs
  in.
- A request goes through `fetch` and its answer comes back on the main thread between frames; there is no worker to
  read a large body.
- Nothing is watched and no process runs (above).
