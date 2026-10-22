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
| `delta` | A JSON object `{"from", "to", "delta"}`, to the history's deltas (`vcs/src/Deltas.hpp`), which a history on disk keeps for each commit and its id doesn't cover | Applying a delta gives a document or throws `json::exception` or `DocumentError`; the delta from `from` to `to` rebuilds `to` exactly |
| `session` | Lines from two peers, through in-memory channels to `channel::Session`: the agent, or the reviewer (`studio`) when a line starts with `>`. The agent's limits are low (4 KiB a message, 24 messages, 6 changes, none refilled), so inputs reach them | Every answer is one JSON object with a `"type"` |
| `scenarios` | A scenarios file (`modules/automation`), read and played headless on the Kanban template, pictures off | Reading gives scenarios or a `DocumentError`; every scenario is reported, every failure says what went wrong, and the report can be written |
| `png` | A file's bytes, to the PNG reader golden images go through (`automation/src/PngReader.hpp`) | Refused with a reason, or a picture of width × height × 3 bytes within the pixel limit |
| `text` | UTF-8 text, shaped, measured and wrapped by a `Font` with fallbacks for Arabic, Devanagari, Chinese and colour emoji (HarfBuzz and FreeType); the first byte picks the width | One advance per code point and one caret more, all finite; the advances sum to the width `measure` gives; wrapped lines are the text in order, cut only at a space or a line break |
| `layout` | A JSON node, put in a screen as the root's only child, built into a headless window, laid out at two sizes, and its geometry recorded (`GeometryCache`, no OpenGL): flex and grid keys, tracks, cells, aspect ratios | Errors are `DocumentError`s; a shown panel's bounds aren't inverted and lie within its parent's |
| `richtext` | Runs of UTF-8 text cut at U+001F, each led by a style byte (bold, larger, italic, a link), laid out by `TextLayout` across fonts; the first byte picks the width and the alignment | Lines cover the text in order and stack up, every piece is on its line and placed, positions and hits stay in the text, carets are finite |
| `path` | SVG path data after a byte that picks the drawing (the fill rule, the join, the cap, a dash, the scale), parsed by `VectorPath`, flattened and meshed as a fill and a stroke | Refused data throws `std::invalid_argument` only; accepted data flattens within the point limit and meshes within the quad limit, in whole quads, with finite vertices and coverage in [0, 1] |
| `text_model` | Edit commands (inserts of any bytes, erases, undo, redo, edits outside an undo step) run on `TextModel`, the piece tree under the text editor | After every command the text is valid UTF-8, its size and lines match the line index, and a position survives its offset; undoing every step and redoing them all gives the same text |
| `find_pattern` | A pattern (plain, whole word or a regular expression), a replacement and a subject of any bytes, as the editor's find and the search service get them from peers | A pattern compiles or gives an error, never both; patterns over the length limit are refused; every match lies inside the subject at or after where the search started, and a plain pattern's replacement always expands. The match limits end catastrophic backtracking |
| `search_glob` | A glob list, a `.gitignore` text and paths, as the search service gets "include", "exclude" and the ignore files of a searched folder (`services/src/SearchGlob.hpp`) | A list compiles or gives an error; the rules kept are capped; a path matches the same way twice, and whatever a matching folder holds matches too |
| `cli` | A screen file, as the `affrescoui` tool reads it (`tools/affrescoui`): the text reader and printer behind `fmt` and `ids`, the places `ids` names, and the schema check behind `check` | `fmt` writes the same JSON value, and formatting its output again changes nothing; the schema check reports problems, never throws |
| `actions` | A JSON value, run as the steps of an action of the Kanban template (its state and actions, no window) | Every event stops within its work budget (the run's `-timeout` catches one that doesn't), and writes no more into the state than the budget lets it |
| `image` | A file's bytes, decoded by `Bitmap::decode` (stb_image for PNG and JPEG, the SVG subset rasterised with vector paths) within small limits: a revision can name any file in an allowed folder | What decodes is within the limits, four bytes a pixel, premultiplied (no channel above its alpha); anything else is a `std::runtime_error` |
| `lookups` | A font's GSUB or GPOS table (the first byte: odd for GPOS), read by `text/LookupSpans` for the fast path of plain text: a revision can name any font file | Every span is at most `unbounded`, and the same when read again |
| `http_response` | A response head, to the network module's parsers (`net/src/Http.hpp`): status lines, header lines and blocks (the WebSocket handshake), Retry-After | A status is 100..999, header names are lower-case tokens, a block holds at most 256 headers and no value spans lines, a retry delay is within an hour |
| `sse` | A streamed body cut into chunks (the first byte picks their size), to the server-sent events and line stream parsers (`net/src/Streams.hpp`), with a 64-byte limit | Every event and line is within the limit, a line is one line and not blank, and a parser over its limit stops |
| `peer_handshake` | A remote peer's opening on the server side (`net/src/PeerHandshake.hpp`): an upgrade request, or a first message; the first byte picks which | A refusal has a known status and a reason without the token, an accepted request has a key, and a token is never empty or over 8 KiB |
| `websocket` | WebSocket frames, cut into pieces, to the frame reader (`net/src/WebSocketFrames.hpp`) as a client (unmasked) or a server (masked) reads them; the first byte picks the side | A message is UTF-8 within the 256-byte limit, control payloads are at most 125 bytes, an error carries a close code and nothing is read after it |
| `frames` | What a service process writes (`services/src/Framing.hpp`), cut into pieces: Content-Length frames or lines; the first byte picks the framing and the piece size | Every body is within the 64-byte limit, a line holds no line feed, and a refused stream stays refused with no body after |
| `feature_manifest` | A feature's manifest (RFC 0004), to `checkManifest` (`format/src/FeatureManifest.hpp`), and if it passes, its record composed into the Kanban template | Refused with `json::parse_error` or `DocumentError`; a manifest that passes composes into a screen or problems |
| `catalog_index` | A catalogue's index, signed at start-up by a trusted key made then, to `readCatalog` (`format/src/FeatureCatalog.hpp`) | An index or a `DocumentError`; releases newest first and once each, keys well formed, URLs inside the folder |
| `signature` | Any text, to the signature parsers (`format/src/FeatureSignatures.hpp`): base64url, public keys, digests, key files, canonical JSON, verifying | Fixed-length parsing gives that length or nothing and writes back as the same text; canonical JSON is its own canonical form; verifying gives the named key or a `DocumentError` |
| `adapters` | Any text, to `convertForeign` (`format/include/affresco/format/Adapters.hpp`): an A2UI message stream or an Adaptive Card, as the screen loader converts it | Refused with `json::parse_error` or `DocumentError`, or not another format's; a conversion is a screen the format accepts, within the limits |

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

Use `json`, `expression`, `revision`, `delta`, `session`, `text`, `richtext`, `cli`, `actions`, `layout`, `lookups`, `image`, `path`, `text_model`, `find_pattern`, `search_glob`, `scenarios`, `png`, `http_response`, `sse`, `websocket`, `peer_handshake` or `frames` in place of `session`. `-fork=4 -ignore_crashes=1`
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
| `json.cpp`, `expression.cpp`, `revision.cpp`, `session.cpp`, `delta.cpp`, `text.cpp`, `richtext.cpp`, `actions.cpp`, `path.cpp`, `text_model.cpp`, `find_pattern.cpp`, `search_glob.cpp`, `scenarios.cpp`, `png.cpp`, `http_response.cpp`, `sse.cpp`, `websocket.cpp`, `peer_handshake.cpp`, `frames.cpp` | The targets |
| `Fuzz.hpp` | What they share: the input as text, the Kanban template, `broken()` |
| `corpus.py` | Writes the seeds and dictionaries: from the templates in `examples/assets/screens/`, the examples and the protocol table in `SCREEN_FORMAT.md` and `VERSION_CONTROL.md`, and the operators in `Expression.cpp`. Generated rather than checked in, so they follow the docs |
| `CMakeLists.txt` | The targets, added when `AFFRESCOUI_BUILD_FUZZERS` is on |

To add a target, for a new kind of input: a `<name>.cpp` with `LLVMFuzzerTestOneInput`, one `affrescoui_fuzzer()`
line in `CMakeLists.txt`, and its seeds in `corpus.py`; CI finds it in the build.
