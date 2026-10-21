# screen: the composition root

`screen` puts the modules together. `affresco::Screen` loads a screen file, gives it to `format::Rules` and
`vcs::Repository`, builds it with `format::Runtime`, keeps its history and its `persist` state on disk, connects the
`channels` it declares, and gives each peer a `channel::Session`. The modules only talk through the interfaces in
`core`; this is the one place that knows them all. An application that wants them put together differently uses the
modules directly. `affresco/affresco.hpp` is the umbrella header. Target: `affrescoui::affrescoui`, the whole library
(uses every module).

## Public headers (`include/affresco/`)

| Header | Offers |
|---|---|
| `Screen.hpp` | `Screen`: load (`fromFile`, `fromString`), build (`createWindow`, `build`, `instantiate`), extend (`registerType`, `onAction`, `onCall`, `setModel`), change (`submit`, `commit`, proposals, history, change sets), peers (`connect`, `listen`, `startChannels`), lookup (`find`, `get<T>`, colours, fonts); images (`setImage`, `allowImageDirectory`, `waitForImages`); `defaultDataDirectory`, `defaultSocketDirectory` |
| `affresco.hpp` | Every public header of every module |

`Screen`'s callbacks take JSON as text (`Call`, `ApplyProperties`, `onEvent`), so an application can use the library
without including nlohmann/json; `versions()` gives the `VersionControl` to code that needs more.

## Invariants

- **Construction order is dependency order:** the rules, then the repository that uses them, then the runtime, the
  pictures and the peers that use the repository. They are destroyed the other way round, and socket servers stop
  before their peers go.
- **A peer is authored by the name it was connected with,** whatever name it gives itself. Every peer hears about
  commits, proposals and `emit` events; `request` and `reply` go to the other peers.
- **Pictures show every image:** `render` waits for the image files still being decoded before it draws.
- **Image files stay in their folders:** a screen's `image` nodes read files only inside the screen file's folder
  and the folders the host allows with `allowImageDirectory`.
- **Pictures never show, and change nothing:** to draw a proposal for a peer, the screen first runs the tasks main
  had deferred, then previews the proposal with the runtime's handlers suspended, draws offscreen and puts the old
  preview back, within one call and without presenting a frame. Events the runtime emits meanwhile aren't sent, and
  no onShow, onHide or watch step runs.
- **Data stays with the user:** the history and `persist` state live under `defaultDataDirectory()`
  (`$AFFRESCOUI_DATA_DIR`, else `~/.affrescoui`), and sockets under `defaultSocketDirectory()`.
- **The window must outlive the `Screen`.** A screen destroyed first takes back its timers, deferred tasks and
  signal connections, so the window can go on receiving input (its panels stay, inert).

## Threads

Main thread only. Channels deliver messages through the `Executor` given to `listen` and `startChannels` (the
`Application`), and the screen cancels its reconnect timers when it is destroyed.

## Errors

- A malformed screen file throws `ScreenError` (the same type as `DocumentError`), naming the place.
- `submit` and the review calls return results or reasons, as `VersionControl` does.
- Problems while the screen runs don't stop it. A `persist` file that can't be written is logged; a declared channel
  that can't connect runs the screen's `<name>:closed` action and tries again if the file asks; errors in handlers
  go to the peers as `error` messages and to the log.
- The screen file's findings (what it refers to but doesn't define) are logged as warnings when it is first built.

## Extension points

Everything `format::Runtime` offers, as text-based callbacks: `registerType`, `onAction`, `onCall`,
`onUnhandledAction`, `onEvent`; `setModel` for a `DataModel` the screen shows with `"in": "models.<name>"`; and
`onAsyncCall` for a slow function run on a worker thread (with `setExecutor` for a window the Screen didn't create;
destroying the Screen waits for the functions running). Plus the peers: `connect(channel, author)` for a channel you made, and
`listen(executor, path, author)` for a socket that accepts any number of them. `registerType` goes through the
runtime's type registry (`modules/format/README.md`).

## Files

```
include/affresco/
  Screen.hpp             the Screen class
  affresco.hpp           the umbrella header
src/
  Screen.cpp             Screen and Screen::Impl: loading (with "include": resolveIncludes, mergeInclude), the data
                         and socket folders, persist, channels, peers
  OffscreenImages.hpp / .cpp   pictures of the screen or of a proposal, for "render"
  Png.hpp / .cpp         a small PNG encoder and a PPM reader
CMakeLists.txt           the affrescoui target and its sources
```

## Recipes

**Run a screen with a native function.**

```cpp
affresco::Application app;
affresco::Screen screen = affresco::Screen::fromFile("board.screen.json");
screen.onCall("now", [](const std::string&) { return std::string("\"12:00\""); }); // returns JSON text
screen.createWindow(app);
screen.listen(app, affresco::Screen::defaultSocketDirectory() / "agent.sock", "agent"); // peers may join
return app.run();
```

**Add an application type.** Derive a panel (see the `render` README), then
`screen.registerType("chart", [] { return std::make_unique<Chart>(); }, {"series"}, applySeries)` before
`createWindow`. The document can then use `{"type": "chart", "series": [...]}`, with every property of the panel's
built-in type too.

**Wire a module in differently.** Copy what `Screen::Impl` does with the parts you need: for example, a
`vcs::Repository` and a `channel::Session` without a window give a headless document server.
