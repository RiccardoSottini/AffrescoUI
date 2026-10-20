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
| `text` | UTF-8 text, shaped, measured and wrapped by a `Font` with fallbacks for Arabic, Devanagari, Chinese and colour emoji (HarfBuzz and FreeType); the first byte picks the width | One advance per code point and one caret more, all finite; the advances sum to the width `measure` gives; wrapped lines are the text in order, cut only at a space or a line break |
| `cli` | A screen file, as the `affrescoui` tool reads it (`tools/affrescoui`): the text reader and printer behind `fmt` and `ids`, the places `ids` names, and the schema check behind `check` | `fmt` writes the same JSON value, and formatting its output again changes nothing; the schema check reports problems, never throws |
| `actions` | A JSON value, run as the steps of an action of the Kanban template (its state and actions, no window) | Every event stops within its work budget (the run's `-timeout` catches one that doesn't), and writes no more into the state than the budget lets it |

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

Use `json`, `expression`, `revision`, `delta`, `session`, `text` or `cli` in place of `session`. `-fork=4 -ignore_crashes=1`
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

`.github/workflows/fuzz.yml` runs each target for 60 seconds on every pull request, and for 15 minutes every night.
The nightly runs keep the corpus they grew in the Actions cache, and every run starts from the latest one, so the
search goes on where it stopped. It also runs on demand (Actions, "Fuzz", "Run workflow"), for as long as you ask.

## Files

| File | What it is |
|---|---|
| `json.cpp`, `expression.cpp`, `revision.cpp`, `session.cpp`, `delta.cpp`, `text.cpp`, `actions.cpp` | The targets |
| `Fuzz.hpp` | What they share: the input as text, the Kanban template, `broken()` |
| `corpus.py` | Writes the seeds and dictionaries: from the templates in `examples/assets/screens/`, the examples and the protocol table in `SCREEN_FORMAT.md` and `VERSION_CONTROL.md`, and the operators in `Expression.cpp`. Generated rather than checked in, so they follow the docs |
| `CMakeLists.txt` | The targets, added when `AFFRESCOUI_BUILD_FUZZERS` is on |

To add a target, for a new kind of input: a `<name>.cpp` with `LLVMFuzzerTestOneInput`, one `affrescoui_fuzzer()`
line in `CMakeLists.txt`, its seeds in `corpus.py`, and its name in the matrix of `fuzz.yml`.
