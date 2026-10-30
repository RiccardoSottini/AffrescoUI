# CI

Four workflows. Every job that builds builds once and runs everything that build can: one build per kind of
compiler and flags, kept warm by a compile cache (ccache) shared across runs.

| Workflow | When | Jobs |
|---|---|---|
| [`ci.yml`](ci.yml) | Every pull request (opened, pushed to, labelled), every push to `master` | `Changes`, `Linux`, `Linux (address + undefined sanitizers)`, `macOS (build only)`, `Windows`, `Agent benchmark (oracle)`; `Fuzz` and `Benchmarks (callgrind)` on a labelled pull request only |
| [`gles.yml`](gles.yml) | Every pull request, every push to `master` | `Linux (OpenGL ES 3.0, Mesa EGL)`: the gles3 backend's build, unit and window tests, and the rendering check against its own golden images (recorded from the base commit's gles3 build); the summary lists the frames that differ from gl41 |
| [`web.yml`](web.yml) | Every pull request, every push to `master` | `Web (Emscripten, WebGL2)`: the browser build with emsdk at a pinned release ([`web/README.md`](../../web/README.md)), the Kanban's scenarios under Node on its WebAssembly, and the site kept as the `web-site` run artifact (nothing is deployed; the release carries it as a zip) |
| [`nightly.yml`](nightly.yml) | Every night at 02:23 UTC, and on demand; a `v*` tag runs its fuzzing only | `Fuzz (nightly)`, `Scaling (nightly)`, `Benchmarks (nightly)` (the last commit against the one before), `Quality report` |
| [`bindings.yml`](bindings.yml) | Pull requests that change the code, pushes to `master` | `C API and Python`: the C11 test of every `afr_` function (Xvfb), the Python binding's tests against `libaffresco`, a frame of each mode of `modules/capi/example` |
| [`release.yml`](release.yml) | A `v*` tag | The release builds, and every artefact (binaries, MCP package, `.vsix`, web build, docs site, AppImage, package recipes) as an asset of the draft GitHub Release; the only job that uploads anywhere |
| [`site.yml`](site.yml) | Pull requests that change the site or what it is generated from, every push to `master`, and on demand | `Site`: builds the documentation site strictly ([`website/README.md`](../../website/README.md)) and uploads it (artifact `site`); no deploy (the release carries it as a zip) |
| [`vscode.yml`](vscode.yml) | Pull requests that change `editors/vscode/` or the schemas, every push to `master`, and on demand | `VS Code extension`: the extension's unit tests and its `.vsix` package ([`editors/vscode/README.md`](../../editors/vscode/README.md), artifact `vscode-extension`); no publish (the release carries the `.vsix`) |

CodeQL's analyses (`Analyze (actions)`, `Analyze (c-cpp)`, `Analyze (python)`) come from the repository's CodeQL
default setup, a setting, not a workflow here.

## The jobs of `ci.yml`

| Job | Runs | Checks | Required |
|---|---|---|---|
| `Changes` | Always, about 10 s | Whether the change needs a build ([`changes.sh`](changes.sh)): a change to `docs/`, `rfcs/` or Markdown no test reads builds nothing (a file moved there from elsewhere is a change to where it was), and the jobs below are skipped (a skipped job counts as passing) | |
| `Linux` | Code changes | GCC 13, warnings as errors. Unit tests (headless windows too); window tests (Xvfb, llvmpipe); the CLI's and the MCP server's tests; the templates' scenarios (`affrescoui test`); every application opens ([`open_apps.py`](../../tests/smoke/open_apps.py)); the rendering check against golden images recorded from the base commit on this runner ([`render-check.sh`](render-check.sh)); the package check; the API reference (Doxygen, warnings as errors); the quality gate (clang-tidy 18 on the changed lines, function length and complexity, with the base's rules) | Yes |
| `Linux (address + undefined sanitizers)` | Code changes | GCC with ASan and UBSan: unit tests and scenarios, window tests, the rendering check (leaks too) | Yes |
| `Fuzz` | Pull requests with code changes and the `full-ci` label (every target fuzzes 15 minutes a night, and on a `v*` tag, in `nightly.yml`) | Every fuzz target for 60 s, one per core at a time, from the nightly corpus ([`fuzz.sh`](fuzz.sh)), built as `nightly.yml` builds them | No |
| `macOS (build only)` | Code changes | Apple clang, warnings as errors; unit tests and the templates' scenarios, headless; every application opens ([`open_apps.py`](../../tests/smoke/open_apps.py), Apple's software OpenGL: the runners have no GPU, so no rendering check) | Yes |
| `Windows` | Code changes | MSVC, warnings as errors; unit tests, the templates' scenarios and the CLI's tests, headless (the runners have no OpenGL 4.1, so `render` isn't checked there) | Yes |
| `Benchmarks (callgrind)` | Pull requests with code changes and the `perf-change` or `full-ci` label (and every night in `nightly.yml`); not on `master` pushes until the commit check is fast again (about 2 hours 20 now) | Base and head built the same way on this runner ([`tests/perf/ci.sh`](../../tests/perf/ci.sh)): instructions under callgrind, frame CPU time, allocations, memory, tokens; fails when a workload's instructions grow over 3%, or when an area's cost grows faster with its n than `tests/perf/scaling-baseline.json` allows ([`tests/perf/README.md`](../../tests/perf/README.md), "Scaling") | No |
| `Quality report` | Every night and on demand, in `nightly.yml` (about 85 minutes) | The coverage of the unit tests against the floor in `tools/quality/coverage-baseline.json`, and the whole-repository report (artifacts `quality-report`, `coverage`) | No |

**Labels**, set by the maintainer only: `visual-change` makes the rendering check record the frames instead of
comparing them (they are uploaded), `perf-change` runs the benchmarks and lets them grow, and `full-ci` runs the
benchmarks and the fuzzing too, which a pull request otherwise leaves to the nightly. Adding or removing a label runs
`ci.yml` again. Every job runs again, not only the one the label decides: a job skipped in the newer run would show
as skipped, which branch protection counts as passing, and would hide a failure of the older run.

**Less waiting.** A new push cancels the run of the one before (`concurrency`). The compile cache is kept per kind of
build (`ccache-<kind>-...`); a pull request reads the base branch's and writes its own, never master's.

**What is protected.** Every workflow has `contents: read` only and no `pull_request_target`; every action is pinned by
its commit; checkouts don't keep the token. A pull request's code is built and run with that read-only token, and
nothing it writes reaches `master`'s caches. The golden images and the base benchmarks are built from the base commit
on the same runner. A fuzz input that fails is uploaded under its target's name (`fuzz-found`).

## Running each check locally

| Check | Command |
|---|---|
| Build, unit, window, CLI, MCP, scenarios, rendering | `cmake -S . -B build -G Ninja -DCMAKE_COMPILE_WARNING_AS_ERROR=ON && cmake --build build && ctest --test-dir build` |
| The templates' scenarios | `build/affrescoui test examples/{code-editor,flow,haven,launcher,sign-in,streak,tempo}/json/*.scenarios.json` |
| Rendering check | `python3 tests/render/check.py` (never `--update` unless the pictures should change) |
| Package check | `python3 tests/package/check.py` |
| API reference | `doxygen Doxyfile` |
| Documentation site | `website/build.sh` ([`website/README.md`](../../website/README.md)) |
| VS Code extension | `cd editors/vscode && npm ci && npm test && npm run package` |
| Quality gate | `tools/quality/run.sh gate origin/master` ([`tools/quality/README.md`](../../tools/quality/README.md)) |
| Sanitizers and fuzzing | `CC=clang CXX=clang++ cmake -S . -B build-fuzz -G Ninja -DCMAKE_BUILD_TYPE=Debug -DAFFRESCOUI_BUILD_FUZZERS=ON`, build, `ctest --test-dir build-fuzz`, then `.github/workflows/fuzz.sh build-fuzz 60` ([`tests/fuzz/README.md`](../../tests/fuzz/README.md) for macOS) |
| Benchmarks | [`tests/perf/README.md`](../../tests/perf/README.md) |
| Coverage and the whole report | `tools/quality/run.sh coverage` and `tools/quality/run.sh report` |
