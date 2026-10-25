# Fuzzing

[libFuzzer](https://llvm.org/docs/LibFuzzer.html) targets for everything a peer can send. Each one feeds generated
inputs to one entry point, built with AddressSanitizer and UndefinedBehaviorSanitizer, and stops at the first input
that crashes, leaks memory, does something undefined, hangs or breaks one of the rules below. The unit tests check
the cases someone thought of; these keep looking for the ones nobody did.

| Target | Input | Checks, besides not crashing |
|---|---|---|
| `json` | Any text, to `parseUntrusted` (`core/Json.hpp`) | Throws only `json::parse_error` or `DocumentError`; what parses nests at most `maxNesting` levels |
| `expression` | A JSON value (templates, `$` operators) or, if it isn't JSON, the inside of a `{{ }}` | Evaluating gives a value or a `DocumentError`, the only error the runtime catches |
| `revision` | A revision's JSON, submitted to a `vcs::Repository` holding the Kanban template | A revision that isn't applied changes nothing; reverting one that is (directly or after review) restores the document and the state |
| `policy` | A JSON object: the Kanban template's `permissions` with budgets and data rules (VERSION_CONTROL.md, "Budgets" and "Data rules"), and a `revision` judged as an agent's | Judging changes nothing and gives the same decision twice; what an agent reads never holds a path a data rule says it may never touch |
| `delta` | A JSON object `{"from", "to", "delta"}`, to the history's deltas (`vcs/src/Deltas.hpp`), which a history on disk keeps for each commit and its id doesn't cover | Applying a delta gives a document or throws `json::exception` or `DocumentError`; the delta from `from` to `to` rebuilds `to` exactly |
| `session` | Lines from two peers, through in-memory channels to `channel::Session`: the agent, or the reviewer (`studio`) when a line starts with `>`. The agent's limits are low (4 KiB a message, 24 messages, 6 changes, none refilled), so inputs reach them | Every answer is one JSON object with a `"type"` |
| `scenarios` | A scenarios file (`modules/automation`), read and played headless on the Kanban template, pictures off | Reading gives scenarios or a `DocumentError`; every scenario is reported, every failure says what went wrong, and the report can be written |
| `png` | A file's bytes, to the PNG reader golden images go through (`automation/src/PngReader.hpp`) | Refused with a reason, or a picture of width × height × 3 bytes within the pixel limit |
| `text` | UTF-8 text, shaped, measured and wrapped by a `Font` with fallbacks for Arabic, Devanagari, Chinese and colour emoji (HarfBuzz and FreeType); the first byte picks the width | One advance per code point and one caret more, all finite; the advances sum to the width `measure` gives; wrapped lines are the text in order, cut only at a space or a line break |
| `layout` | A JSON node, put in a screen as the root's only child, built into a headless window, laid out at two sizes, and its geometry recorded (`GeometryCache`, no OpenGL): flex and grid keys, tracks, cells, aspect ratios, gradients | Errors are `DocumentError`s; a shown panel's bounds aren't inverted and lie within its parent's |
| `richtext` | Runs of UTF-8 text cut at U+001F, each led by a style byte (bold, larger, italic, a link), laid out by `TextLayout` across fonts; the first byte picks the width and the alignment | Lines cover the text in order and stack up, every piece is on its line and placed, positions and hits stay in the text, carets are finite |
| `path` | SVG path data after a byte that picks the drawing (the fill rule, the join, the cap, a dash, the scale), parsed by `VectorPath`, flattened and meshed as a fill and a stroke | Refused data throws `std::invalid_argument` only; accepted data flattens within the point limit and meshes within the quad limit, in whole quads, with finite vertices and coverage in [0, 1] |
| `text_model` | Edit commands (inserts of any bytes, erases, undo, redo, edits outside an undo step) run on `TextModel`, the piece tree under the text editor | After every command the text is valid UTF-8, its size and lines match the line index, and a position survives its offset; undoing every step and redoing them all gives the same text |
| `find_pattern` | A pattern (plain, whole word or a regular expression), a replacement and a subject of any bytes, as the editor's find and the search service get them from peers | A pattern compiles or gives an error, never both; patterns over the length limit are refused; every match lies inside the subject at or after where the search started, and a plain pattern's replacement always expands. The match limits end catastrophic backtracking |
| `search_glob` | A glob list, a `.gitignore` text and paths, as the search service gets "include", "exclude" and the ignore files of a searched folder (`services/src/search/SearchGlob.hpp`) | A list compiles or gives an error; the rules kept are capped; a path matches the same way twice, and whatever a matching folder holds matches too |
| `path_tree` | Paths, one per line, as a file explorer gives `pathTree` the names a folder holds (`format/src/functions/ExpressionFunctionsPaths.cpp`) | Each path has one node, whose id is its parent's and its label; every path given has its node; folders come first |
| `git_policy` | git arguments, as a screen asks the git grant to run them (`services/src/grants/GitPolicy.cpp`) | What is accepted names only the options the policy lists and paths inside the folder; the shaper runs it on the root's own repository |
| `cli` | A screen file, as the `affrescoui` tool reads it (`tools/affrescoui`): the text reader and printer behind `fmt` and `ids`, the places `ids` names, and the schema check behind `check` | `fmt` writes the same JSON value, and formatting its output again changes nothing; the schema check reports problems, never throws |
| `actions` | A JSON value, run as the steps of an action of the Kanban template (its state and actions, no window) | Every event stops within its work budget (the run's `-timeout` catches one that doesn't), and writes no more into the state than the budget lets it |
| `ask_reply` | A JSON value: a reply, or a list of replies (at most 300), to the request a screen's `ask` step just sent (`Runtime::receiveReply`, `format/src/assistant/Asks.hpp`); one without `request` answers it | The request stays listed in `requests`; a reply's text and replier are cut to their size at a character boundary, so the state still writes as UTF-8 |
| `audio` | A sound's bytes after a mode byte: played from memory, or pushed in chunks into an encoded stream decoded on its own thread, with an encoding hinted (miniaudio's WAV, FLAC and MP3 decoders), on an engine the target pulls within small limits (512 KiB, two seconds) | Refused with a reason, or every sample finite, every band of its spectrum from 0 to 1 and nothing played longer than the limit; a stream within its capacity takes every chunk |
| `image` | A file's bytes, decoded by `Bitmap::decode` (stb_image for PNG and JPEG, the SVG subset rasterised with vector paths) within small limits: a revision can name any file in an allowed folder | What decodes is within the limits, four bytes a pixel, premultiplied (no channel above its alpha); anything else is a `std::runtime_error` |
| `lookups` | A font's GSUB or GPOS table (the first byte: odd for GPOS), read by `font/LookupSpans` for the fast path of plain text: a revision can name any font file | Every span is at most `unbounded`, and the same when read again |
| `http_response` | A response head, to the network module's parsers (`net/src/http/Http.hpp`): status lines, header lines and blocks (the WebSocket handshake), Retry-After | A status is 100..999, header names are lower-case tokens, a block holds at most 256 headers and no value spans lines, a retry delay is within an hour |
| `sse` | A streamed body cut into chunks (the first byte picks their size), to the server-sent events and line stream parsers (`net/src/http/Streams.hpp`), with a 64-byte limit | Every event and line is within the limit, a line is one line and not blank, and a parser over its limit stops |
| `peer_handshake` | A remote peer's opening on the server side (`net/src/peers/PeerHandshake.hpp`): an upgrade request, or a first message; the first byte picks which | A refusal has a known status and a reason without the token, an accepted request has a key, and a token is never empty or over 8 KiB |
| `websocket` | WebSocket frames, cut into pieces, to the frame reader (`net/src/websocket/WebSocketFrames.hpp`) as a client (unmasked) or a server (masked) reads them; the first byte picks the side | A message is UTF-8 within the 256-byte limit, control payloads are at most 125 bytes, an error carries a close code and nothing is read after it |
| `frames` | What a service process writes (`services/src/rpc/Framing.hpp`), cut into pieces: Content-Length frames or lines; the first byte picks the framing and the piece size | Every body is within the 64-byte limit, a line holds no line feed, and a refused stream stays refused with no body after |
| `dap` | A message body a debug adapter writes, to the `dap` service's reader (`services/src/dap/DapProtocol.hpp`), and the same bytes as a request's arguments | A message is read or refused with a reason; what a screen gets is UTF-8, its lists within their limits, and every source it names says whether it is inside the folder |
| `syntax` | Text in a language the first byte picks, then edits of five bytes each, to the syntax engine (`services/Syntax.hpp`, tree-sitter and its grammars) | It parses within its limits; runs stay on their lines, folds go downwards, bracket pairs read back as pairs; an edited document highlights as a fresh parse of the same text does |
| `data_models` | `{"models", "next", "query"}` (models with migrations and seeds, the next version of them, a query) or, if it isn't JSON, a CSV text, to the data module's store (`modules/data`) | Refused with `DocumentError` only; a plan changes nothing and agrees with what follow then does; a refused change leaves every record as it was; CSV rows stay within their limits |
| `migrate` | A version 1 document or patch, to `migrate` and `migratePatch` (`format/Migrate.hpp`), each twice | Refused with `json::parse_error`; otherwise migrated, a changed document declares version 2, and the second migration changes nothing |
| `feature_manifest` | A feature's manifest (RFC 0004), to `checkManifest` (`format/src/features/FeatureManifest.hpp`), and if it passes, its record composed into the Kanban template | Refused with `json::parse_error` or `DocumentError`; a manifest that passes composes into a screen or problems |
| `catalog_index` | A catalogue's index, signed at start-up by a trusted key made then, to `readCatalog` (`format/src/features/FeatureCatalog.hpp`) | An index or a `DocumentError`; releases newest first and once each, keys well formed, URLs inside the folder |
| `signature` | Any text, to the signature parsers (`format/src/features/FeatureSignatures.hpp`): base64url, public keys, digests, key files, canonical JSON, verifying | Fixed-length parsing gives that length or nothing and writes back as the same text; canonical JSON is its own canonical form; verifying gives the named key or a `DocumentError` |
| `composition` | `{ manifest, adapter, points, exports }`: a feature from another app proven on the Kanban template with those contribution points (`format/src/composition/CompositionProof.hpp`), and the template's contributions listed | Every proof checks as a proof record within its size, gives its verdict, and a passed one comes with the composed document; listing refuses with `DocumentError` or prints |
| `url` | An allowlist entry, a line feed, then a URL, to `parseUrl` and the origin allowlist (`net/Url.hpp`): every URL a connection, a sign-in provider or a remote peer names | A URL that parses has a known scheme, a lower-case host, a port and a target starting with `/`, and its origin parses again to the same; encoded segments and query parts hold unreserved characters only |
| `oauth` | A sign-in provider's answer (`net/src/oauth/OAuth.hpp`): a token, a device authorization or an error answer, a redirect's query, base64url; the first byte picks the reader and whether an id token is checked | Tokens read have an access token and expire after now; an error never quotes the access token; device codes poll at a sane interval; base64url read back writes the same bytes |
| `history` | A history on disk (`Repository::openHistory`, `VERSION_CONTROL.md` "History on disk") with `refs.json` or an object file replaced by the input | It opens, or throws `DocumentError` naming the file |
| `token_file` | The document server's token file (`apps/server/TokenFile.hpp`), `<sha256 hex> <name>` lines, and the tokens peers present | What reads lists valid names only, at most `maxEntries`; `verify()` gives a listed name or nothing |
| `git_output` | What git prints for Forge (`examples/forge/features/scm`): `status --porcelain=v2 -z`, `for-each-ref`, and the line diff of two texts; the first byte picks which | Within the byte, entry and path limits; every file has a known kind, at most one branch is current, a diff's rows account for every line |
| `color_theme` | A colour theme, imported by Forge (`examples/forge/features/themes/ThemeImport.hpp`) | An id of lower-case letters, digits and dashes (at most 40), and a patch adding only `#rrggbb` or `#rrggbbaa` colours under `/tokens/`, or a `std::runtime_error` |
| `adapters` | Any text, to `convertForeign` (`format/include/affresco/format/Adapters.hpp`): an A2UI message stream or an Adaptive Card, as the screen loader converts it | Refused with `json::parse_error` or `DocumentError`, or not another format's; a conversion is a screen the format accepts, within the limits |
| `gltf` | A file's bytes, `.gltf` or `.glb`, to the glTF loader (`modules/scene3d/src/gltf/`), without a folder, so only embedded and `data:` buffers are read | Refused with an error, or a model within the limits it was given |
| `scene` | `{ gltf, scene, animations, time }`: a scene node's scene and clips (`format/src/creative/SceneType.cpp`), the glTF placed as an asset, read at the time | Within its node and material limits; every placed node is in the scene and selects its owner; read back from its JSON it is the same scene |
| `images` | `{ images, steps }`: the images a screen edits (`format/src/imaging`), each entry within small limits, and up to 48 `image` steps with any `with` | An entry is refused with a reason or makes an image; a step that can't be done leaves its reason, as the step does; every snapshot the screen's state receives can be written out, and the image stays within its side limit |

## Inventory of untrusted inputs

Every place where bytes the app didn't write itself come in, and the target that covers it, or why none does. A new
reader of outside bytes adds a row here and, unless a reason below fits, a target. `SECURITY.md` says who controls each
of these and what limits them.

| Where bytes come in | Who controls them | Target, or why none |
|---|---|---|
| A peer's lines on a channel (revisions, requests, reviews, queries) | Any peer: an agent, a person's client, a remote peer | `session`, `json`, `revision`, `expression`, `actions`, `delta`, `layout`, `policy` |
| A peer's replies to the screen's own requests (`ask`) | Any peer | `ask_reply` |
| A remote peer's opening (upgrade request, first message) | Anyone who reaches the document server | `peer_handshake`, `http_response` |
| WebSocket frames, server-sent events, HTTP response heads | The server a connection opens, or a peer | `websocket`, `sse`, `http_response` |
| URLs a connection, a sign-in provider or a peer names | Screens, peers, providers | `url` |
| Sign-in answers (token, device code, error, redirect, id token) | The provider, or whoever answers in its place | `oauth` |
| Image files a revision names (PNG, JPEG, SVG) | Peers, within the allowed folders | `image`, `path` |
| A screen's models, migrations, seeds and queries; CSV files an importCsv step reads | Peers (revisions), the person (a file) | `data_models` |
| glTF models a scene node names | Peers, within the allowed folders | `gltf`, `scene` |
| A scene node's scene and animations (state) | Agents and peers, by revisions | `scene` |
| The images a screen edits (its `images` section) and the `image` steps that change them | Agents and peers, by revisions | `images` |
| Font files a revision names | Peers, within the allowed folders | `lookups` for the tables the app reads itself; the rest is FreeType and HarfBuzz, fuzzed upstream by OSS-Fuzz, and `text` and `richtext` shape through them |
| Text typed, pasted or sent to an editor | Users and peers | `text_model`, `text`, `richtext`, `syntax` (the grammars that colour it) |
| Find and search patterns, replacements, globs, `.gitignore` files | Peers, and whoever made the searched folder | `find_pattern`, `search_glob`, `path_tree` |
| What a service program writes (process, `jsonrpc` for language and debug servers) | The program the screen runs | `frames` for the framing, `json` for the messages, `dap` for what a debug adapter sends |
| What a terminal program writes (the VT stream) | The program running in the terminal | None yet: a target is pending (`SECURITY.md`, gap G-24). The parser is libvterm; the stream reaches the screen only as cells within the terminal's size |
| What git prints for Forge | Whoever made the repository (file names, branches, contents) | `git_output` |
| The git commands a screen asks the git grant to run | The screen, a revision of it | `git_policy` |
| A history folder on disk | Anyone who can write to it, a crash, a full disk | `history`, `delta` |
| Feature manifests, catalogue indexes, signatures and key files | A catalogue's publisher, or whoever serves it | `feature_manifest`, `catalog_index`, `signature` |
| Features from another app, their adapters and proofs | Another app's author, an agent or a peer | `composition` |
| The document server's token file | Whoever can write its folder | `token_file` |
| A colour theme imported into Forge | Whoever published the theme | `color_theme` |
| A document in another format (an A2UI message stream, an Adaptive Card) | Whoever wrote it: an agent, a service, a file | `adapters` |
| Scenario files and golden images | The project's contributors | `scenarios`, `png` |
| Screen files read by the `affrescoui` tool and the MCP server | Whoever wrote the file | `cli`, `json` |
| Screen files and their includes loaded by the host | Trusted (the app ships them), but read with the same `parseUntrusted` | `json`, `layout` |
| The state file a host keeps | Trusted: the host wrote it; read with `parseUntrusted` and its nesting limit | `json` |
| Paths from file dialogs, drops, the fs and watch services | The operating system gives them; no parser reads them, and the services check each against the allowed folders | None needed: no parser; the folder check is unit-tested |
| Command-line arguments and environment variables | The user who starts the program | None needed: trusted, and read by plain comparisons |

## Running them

libFuzzer comes with clang (not GCC). The build option instruments the whole build, so use a build folder of its own.

**Linux** (clang 18, as CI uses; on Ubuntu, `clang-18` and `libclang-rt-18-dev`, plus the packages CI installs for GLFW):

```sh
CC=clang-18 CXX=clang++-18 cmake -S . -B build-fuzz -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DAFFRESCOUI_BUILD_FUZZERS=ON -DAFFRESCOUI_BUILD_EXAMPLES=OFF -DAFFRESCOUI_BUILD_TESTS=OFF
cmake --build build-fuzz
python3 tests/fuzz/corpus.py --out build-fuzz/fuzz-corpus     # the seeds and dictionaries
mkdir -p build-fuzz/corpus/session                            # where the corpus grows (the seeds stay as they are)
build-fuzz/tests/fuzz/affrescoui_fuzz_session -max_total_time=60 -dict=build-fuzz/fuzz-corpus/session.dict \
  build-fuzz/corpus/session build-fuzz/fuzz-corpus/session
```

Use `json`, `expression`, `revision`, `delta`, `session`, `text`, `richtext`, `cli`, `actions`, `layout`, `lookups`, `image`, `path`, `text_model`, `find_pattern`, `search_glob`, `scenarios`, `png`, `http_response`, `sse`, `websocket`, `peer_handshake`, `frames`, `feature_manifest`, `catalog_index`, `signature`, `composition`, `url`, `oauth`, `history`, `token_file`, `git_output`, `git_policy`, `path_tree`, `color_theme`, `gltf`, `scene` or `images` in place of
`session`. `-fork=4 -ignore_crashes=1`
keeps going after a crash, on 4 cores. A Debug build keeps nlohmann::json's assertions on; whatever the build type,
the code is compiled `-O1 -g`, as libFuzzer recommends.

**macOS.** Apple's clang has no libFuzzer. Homebrew's LLVM has one (`brew install llvm`; checked with LLVM 23).
Build with it, linked against its own libc++, as `brew info llvm` says:

```sh
LLVM=$(brew --prefix llvm)
CC=$LLVM/bin/clang CXX=$LLVM/bin/clang++ \
LDFLAGS="-L$LLVM/lib/c++ -L$LLVM/lib/unwind -lunwind -Wl,-rpath,$LLVM/lib/c++" \
  cmake -S . -B build-fuzz -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DAFFRESCOUI_BUILD_FUZZERS=ON -DAFFRESCOUI_BUILD_EXAMPLES=OFF -DAFFRESCOUI_BUILD_TESTS=OFF
```

Then build and run them as on Linux. Or run the Linux commands in a container (Docker or Podman), which is what CI
does:

```sh
docker run --rm -it -v "$PWD":/src -w /src ubuntu:24.04 bash
apt-get update && apt-get install -y clang-18 libclang-rt-18-dev cmake ninja-build git python3 \
  libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libxext-dev libwayland-dev libxkbcommon-dev \
  wayland-protocols pkg-config
```

## When one fails

libFuzzer writes the input to `crash-<sha1>` (or `leak-`, `timeout-`, `oom-`) in the current folder, or in the
`-artifact_prefix` folder. CI uploads it as the `fuzz-<target>` artifact. To replay it, run the target on the file:

```sh
build-fuzz/tests/fuzz/affrescoui_fuzz_session crash-1234abcd           # runs that input once and shows the error
build-fuzz/tests/fuzz/affrescoui_fuzz_session -minimize_crash=1 -runs=10000 crash-1234abcd   # a smaller one
```

A failing input is a bug in the library, not in the target. Fix it, and add the input (or a readable version of it)
as a regression test in `tests/unit/`, as the tests marked "Found by fuzzing" there do. Keep the input itself in
`regressions/<target>/`, under a name that says what it found: `corpus.py` adds those files to the target's seeds, so
every run tries them again. A broken rule prints
`affrescoui fuzz: broken rule: ...` before stopping.

## CI

The sanitizer job of `.github/workflows/ci.yml` builds every target with the tests, and runs each for 35 seconds on
every pull request (`.github/workflows/fuzz.sh`, which finds the targets in the build: a new one needs no edit
there). `.github/workflows/nightly.yml` runs each for 15 minutes every night and keeps the corpus it grew in the
Actions cache; every run starts from the latest one, so the search goes on where it stopped. It also runs on demand
(Actions, "Nightly", "Run workflow"), for as long as you ask. A failing input is uploaded as the `fuzz-found`
artifact, in a folder named after its target.

## Files

| File | What it is |
|---|---|
| `json.cpp`, `expression.cpp`, `revision.cpp`, `session.cpp`, `delta.cpp`, `text.cpp`, `richtext.cpp`, `actions.cpp`, `path.cpp`, `text_model.cpp`, `find_pattern.cpp`, `search_glob.cpp`, `path_tree.cpp`, `git_policy.cpp`, `scenarios.cpp`, `png.cpp`, `http_response.cpp`, `sse.cpp`, `websocket.cpp`, `peer_handshake.cpp`, `frames.cpp`, `url.cpp`, `oauth.cpp`, `history.cpp`, `token_file.cpp`, `git_output.cpp`, `color_theme.cpp`, `gltf.cpp`, `scene.cpp` and the others named above | The targets |
| `Fuzz.hpp` | What they share: the input as text, the Kanban template, `broken()` |
| `corpus.py` | Writes the seeds and dictionaries: from the templates (`examples/templates.json`), the examples and the protocol table in `SCREEN_FORMAT.md` and `VERSION_CONTROL.md`, and the operators in `Expression.cpp`. Generated rather than checked in, so they follow the docs |
| `CMakeLists.txt` | The targets, added when `AFFRESCOUI_BUILD_FUZZERS` is on |

To add a target, for a new kind of input: a `<name>.cpp` with `LLVMFuzzerTestOneInput`, one `affrescoui_fuzzer()`
line in `CMakeLists.txt`, and its seeds in `corpus.py`; CI finds it in the build.
