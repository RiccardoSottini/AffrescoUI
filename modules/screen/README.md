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
| `Screen.hpp` | `Screen`: load (`fromFile`, `fromString`), build (`createWindow`, also as a second window sharing another's context, `build`, `instantiate`), extend (`registerType`, `onAction`, `onCall`, `setModel`, `services`, `network`, `setSignInOptions`), change (`submit`, `commit`, proposals, history, change sets, `installChanges` and `repairs` for self-repairing ones), peers (`connect`, `listen`, `listenWebSocket`, `startChannels`), lookup (`find`, `get<T>`, colours, fonts); images (`setImage`, `allowImageDirectory`, `waitForImages`); audio (`audio`, `setAudio`, `setAudioSource`, `allowAudioDirectory`); `defaultDataDirectory`, `defaultSocketDirectory` |
| `affresco.hpp` | Every public header of every module |

**Assets** (VERSION_CONTROL.md, "Assets"): `assets()` (the version shown, `{name, type, size, sha256}`),
`assetBytes(name)` (nullptr if none), `attach(name, bytes)` (a file the host was given, as an `attach` step does:
type from the name, checked against the bytes, measured; returns `Attached{attachment, error}`), and `submitRevision`
takes `assets` and `run`. When a commit with `run` lands on main (a proposal merged), the screen runs its steps once,
after the update, as an action of the root panel. `asset:<name>` reads an asset wherever a screen reads a file: an
`image` node's `file`, a sound's `file`, an `image` step's `open` with `{"asset": name}`.

`Screen`'s callbacks take JSON as text (`Call`, `ApplyProperties`, `onEvent`), so an application can use the library
without including nlohmann/json; `versions()` gives the `VersionControl` to code that needs more.

## Invariants

- **Construction order is dependency order:** the rules, then the repository that uses them, then the runtime, the
  pictures and the peers that use the repository. They are destroyed the other way round, and socket servers stop
  before their peers go.
- **A peer is authored by the name it was connected with,** whatever name it gives itself. Every peer hears about
  commits, proposals and `emit` events; `request` and `reply` go to the other peers.
- **Evidence is the host's:** `getEvidence` draws main and the proposal, plays the scenarios the host gave
  `setScenarioCheck` on the proposal's document, and lists its changed texts; a revision can't carry any of it.
- **Pictures show every image:** `render` waits for the image files still being decoded before it draws.
- **Image files stay in their folders:** a screen's `image` nodes read files only inside the screen file's folder
  and the folders the host allows with `allowImageDirectory`.
- **Pictures never show, and change nothing:** to draw a proposal for a peer, the screen first runs the tasks main
  had deferred, then previews the proposal with the runtime's handlers suspended, draws offscreen and puts the old
  preview back, within one call and without presenting a frame. Events the runtime emits meanwhile aren't sent, and
  no onShow, onHide or watch step runs.
- **Services are the host's grants:** `services()` is the screen's `services::ServiceRegistry`. The host grants
  folders (`addRoot`) and programs (`allowProcess`) and may add providers before building; the file's `services`
  section can only name them, and its services start at their first use. They stop when the screen is destroyed.
- **Origins are the host's grants too:** `network()` is the screen's `net::Client`, made at the first call. The
  file's `connections` reach only the origins the host allows (`network().allow("https://api.example.com")`); its
  results arrive through the screen's executor.
- **Sound is the host's grant too:** `audio()` is the screen's `audio::Engine`, made at the first call (the system's
  output, or none with `$AFFRESCOUI_AUDIO_OUTPUT=null`); until then the file's `audio` steps say the host plays
  no audio. Its sounds' files come only from the screen's folder and those `allowAudioDirectory` allows, its URLs
  only through `network()`. Built without the audio module (`AFFRESCOUI_AUDIO=OFF`), `audio()` returns nullptr.
- **Data stays with the user:** the history and `persist` state live under `defaultDataDirectory()`
  (`$AFFRESCOUI_DATA_DIR`, else `~/.affrescoui`), and sockets under `defaultSocketDirectory()`.
- **The window must outlive the `Screen`.** A screen destroyed first takes back its timers, deferred tasks, native menu bar and
  signal connections, so the window can go on receiving input (its panels stay, inert).

## Threads and ownership

Main thread only. Channels deliver messages through the `Executor` given to `listen` and `startChannels` (the
`Application`), and the screen cancels its reconnect timers when it is destroyed.

`Screen` owns one `Screen::Impl` (`src/ScreenImpl.hpp`), which owns every part: the services, the rules, the
repository, the runtime, the pictures, the peers with their sessions and the socket servers. They are declared so
that each is destroyed before what it uses. Tasks posted for later and slots of the declared channels also hold
`Impl::alive`, a flag set to false when the screen goes, and do nothing after. The window and the `Application`
are the host's, and must outlive it.

## Errors

- A malformed screen file throws `ScreenError` (the same type as `DocumentError`), naming the place.
- `submit` and the review calls return results or reasons, as `VersionControl` does.
- Problems while the screen runs don't stop it. A `persist` file that can't be written is logged; a declared channel
  that can't connect runs the screen's `<name>:closed` action and tries again if the file asks; errors in handlers
  go to the peers as `error` messages and to the log.
- The screen file's findings (what it refers to but doesn't define) are logged as warnings when it is first built.

## Extension points

`setLocale(tag)` gives screens whose `strings` name no `locale` the computer's (`format::systemLocale()`); the host
calls it at start. Everything `format::Runtime` offers, as text-based callbacks: `registerType`, `onAction`, `onCall`,
`onUnhandledAction`, `onEvent`; `setModel` for a `DataModel` the screen shows with `"in": "models.<name>"`; and
`onAsyncCall` for a slow function run on a worker thread (with `setExecutor` for a window the Screen didn't create;
destroying the Screen waits for the functions running). Plus the peers: `connect(channel, author)` for a channel you made, and
`listen(executor, path, author)` for a socket that accepts any number of them, and
`listenWebSocket(executor, "127.0.0.1:7420", verifier)` for remote peers, each authored as its token's verifier says
(`modules/net/README.md`). `registerType` goes through the
runtime's type registry (`modules/format/README.md`).

## Files

```
include/affresco/
  Screen.hpp             the Screen class
  affresco.hpp           the umbrella header
src/
  Screen.cpp             loading (with "include": resolveIncludes, mergeInclude; menus merge menu by menu, so
                         feature files add to one menu), the data and socket folders, building, lookups
  ScreenImpl.hpp / .cpp  Screen::Impl: the parts put together, the "persist" state file, the peers
  ScreenChannels.cpp     the channels a file declares, and the peers the screen connects or listens for
  ScreenVersions.cpp     revisions, proposals, history, change sets and modes, forwarded to the repository
  ScratchFolder.hpp      a new folder only this user can open, removed with what is in it
  Png.hpp / .cpp         a small PNG encoder and a PPM reader
  pictures/              what peers see of the screen
    OffscreenImages.hpp / .cpp   pictures of the screen or of a proposal, for "render" and "getEvidence"
    ProposalEvidence.hpp / .cpp  a proposal's evidence: the changed region of two pictures, its changed texts
    Inspector.hpp / .cpp   the inspector: the overlay on the panel under the pointer, its statistics and event trace
  features/
    FeaturesService.hpp / .cpp   the `features` provider: the catalogues, installing a feature by revision
  windows/
    SecondWindow.cpp     a screen in a second window sharing another's OpenGL context
    EmbeddedScreen.cpp   a screen built into the host's OpenGL context
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

**Embed it in the host's OpenGL.** `screen.embed(embedding)` instead of `createWindow`: the screen is built in an
`affresco::Embedding` (see the `render` README), whose frames the host draws (`src/windows/EmbeddedScreen.cpp`).

**Wire a module in differently.** Copy what `Screen::Impl` does with the parts you need: for example, a
`vcs::Repository` and a `channel::Session` without a window give a headless document server.
