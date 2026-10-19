# Contributing to AffrescoUI

Thank you for helping. AffrescoUI is a C++23 / OpenGL 4.1 GUI library whose UIs are JSON screen documents that people
and AI agents change live, under version control. This page explains how to build it, how to check a change, and how
a change gets in. Everyone who takes part follows the [Code of Conduct](CODE_OF_CONDUCT.md).

- [Before you start](#before-you-start)
- [Build](#build)
- [Check your change](#check-your-change)
- [Code style](#code-style)
- [One change per pull request](#one-change-per-pull-request)
- [Changes to the format, the protocol or the security model](#changes-to-the-format-the-protocol-or-the-security-model)
- [License and the NOTICE file](#license-and-the-notice-file)

## Before you start

- **A bug:** open a [bug report][bug report] with your OS, GPU and the screen file that shows it. A small fix can
  come as a pull request straight away.
- **A feature:** open a [feature request][feature request] first, so we can agree on the shape before you write it.
- **A change to the screen format, the live protocol, the security model or the module layout:** start with a
  [format-change RFC][format RFC]. See [below](#changes-to-the-format-the-protocol-or-the-security-model).
- **A security problem:** don't open a public issue. Report it privately through the repository's **Security** tab
  (*Report a vulnerability*).

[bug report]: https://github.com/RiccardoSottini/AffrescoUI/issues/new?template=bug_report.yml
[feature request]: https://github.com/RiccardoSottini/AffrescoUI/issues/new?template=feature_request.yml
[format RFC]: https://github.com/RiccardoSottini/AffrescoUI/issues/new?template=format_rfc.yml

Read [`README.md`](README.md) for the overview and the architecture, [`SCREEN_FORMAT.md`](SCREEN_FORMAT.md) for the
format and [`VERSION_CONTROL.md`](VERSION_CONTROL.md) for permissions, proposals and history.

## Build

You need CMake 3.25 or newer, a C++23 compiler, Ninja, and Python 3 for the rendering check and the MCP server. GLFW,
nlohmann/json and doctest are downloaded by CMake.
On Linux, install the X11 and Wayland headers first, as CI does:

```sh
sudo apt-get install ninja-build libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libxext-dev \
  libwayland-dev libxkbcommon-dev wayland-protocols
```

Build with warnings as errors, the way CI does. A pull request must build with 0 warnings.

```sh
cmake -S . -B build -G Ninja -DCMAKE_COMPILE_WARNING_AS_ERROR=ON
cmake --build build
```

Then try it:

```sh
./build/affrescoui_host                                     # the Kanban template
AFFRESCOUI_SCREENSHOT=out.ppm ./build/affrescoui_host       # renders its first frame to out.ppm
./build/affrescoui_studio                                   # the Studio client
```

The library is tested on macOS and Linux. Windows is not supported yet: the socket and process channels are POSIX.

### Live tests

When you run the host, the Studio and the MCP server together, give them the same short folder of your own, so they
don't meet anything else running on your machine:

```sh
export AFFRESCOUI_SOCKET_DIR=out/s                 # also covers the relative channel paths inside screens
./build/affrescoui_host --socket out/s &
./build/affrescoui_studio --socket out/s &
claude mcp add affrescoui -- python3 mcp/affrescoui_mcp.py --socket out/s
```

The MCP server talks over stdio, so Claude Code starts it: register it as above and start Claude Code from the
repository root, so that `out/s` is the same folder. [`mcp/README.md`](mcp/README.md) has the rest of the setup.

## Check your change

Every pull request passes two checks, and CI runs both.

**The rendering check** plays scripted clicks, typing, drags and revisions on every template and compares each frame
with a golden image, pixel for pixel. The golden images aren't in git: the first run records them from the code you
have. So record them on a clean checkout **before** you change anything, then run the check again after your change:

```sh
python3 tests/render/check.py                # or: ctest --test-dir build -R rendering
python3 tests/render/check.py --only kanban  # one scenario
```

- Run `--update` only for a visual change you intend, and say so in the pull request. The maintainer decides whether
  existing pixels may change.
- A new behaviour gets a scenario in `tests/render/scenarios.json`. Open the frames it records and check that they
  show what you expect.
- Details: [`tests/render/README.md`](tests/render/README.md).

**The unit tests** cover the logic that needs no window (`core`, `vcs`, `format` and `channel`) and run in well under
a second:

```sh
ctest --test-dir build -R unit --output-on-failure
./build/affrescoui_unit_tests --test-case="vcs*"   # some of them
```

- New logic gets unit tests, and every bug fix gets a test that would have caught it.
- Never weaken, skip or delete a test to get green. A test that finds a bug you can't fix is marked as skipped, with a
  comment that describes the bug.
- Details: [`tests/unit/README.md`](tests/unit/README.md).

## Code style

- **Keep it simple** and match the surrounding code: its naming, its comment density and its idioms.
- **Format** with [`.clang-format`](.clang-format): tabs for indentation, 120 columns.
- **Doxygen on every function**, class and member, and an `@file` brief in every file. Comment why, not what.
- **Plain English names,** never abbreviations: functions are verbs, queries are nouns. The format uses full English
  words too (`textColor`, not `tc`).
- **The library stays general.** `modules/` and the format serve any app. Anything only one kind of app needs, such as
  a language's syntax colours, goes in `examples/`, built on the extension points (`Screen::registerType`, `onCall`,
  subclassing).
- **Modern C++, the way the code already does it:** `std::unique_ptr` owns, references and raw pointers never do, no
  `new` or `delete`, RAII for every resource, `const` by default.
- **Untrusted input** (documents, peers) gets limits and a capped recursion depth. Secrets never go into documents,
  state, history or logs.
- When you change a behaviour or an extension point, update its docs in the same pull request:
  `SCREEN_FORMAT.md`, `VERSION_CONTROL.md` or the README next to the code (such as `mcp/README.md`).

## One change per pull request

A pull request does one thing: one fix, one feature or one RFC. Small pull requests are reviewed faster, and the
history stays readable.

1. Branch from `master` on your fork.
2. Touch only the files your change needs.
3. Leave `README.md` and `CHANGELOG.md` to the maintainer. Describe user-visible changes in the pull request
   instead, and they are added at release.
4. Before you mark the pull request ready, rebase on the latest `master`, build with warnings as errors, and pass the
   rendering check (without `--update`) and the unit tests.
5. Open the pull request against `master` and fill in the [template](.github/pull_request_template.md): what and why,
   how you verified it, and the checklist.

Commit messages: a subject that says what the change does, and a body that says why and how.

Labels such as `visual-change` tell CI that the maintainer accepted a change in pixels. Only the maintainer sets them.

## Changes to the format, the protocol or the security model

Screen documents, the live protocol and the MCP tools are a **public contract**: people and agents write screens
against them, and a change can break screens that already exist. So these changes are agreed before they are built:

1. **Open an issue** with the [format-change RFC][format RFC] template. It is labelled `rfc`. Describe the problem and
   a sketch of the design, and discuss it there.
2. **Write the RFC** when the idea is worth pursuing: copy [`rfcs/0000-template.md`](rfcs/0000-template.md) and open a
   pull request with it alone. [`rfcs/README.md`](rfcs/README.md) says when an RFC is needed and how it is numbered.
3. **The maintainer approves the RFC by merging it.** Implementation waits for that merge, and comes in its own pull
   request that links the RFC.

What a format change must keep:

- **Additive within version 1.** A change that alters what an existing screen means needs the maintainer's decision.
- **The design rules** in [`SCREEN_FORMAT.md`](SCREEN_FORMAT.md#design-rules): words, not codes; familiar notation;
  one key, one meaning; named arguments beyond two; locality first; reviewable changes; notes; errors that name the
  place.
- **Updated together:** the schema (`schema/affrescoui-screen.schema.json`), `SCREEN_FORMAT.md` and the catalog that
  `get_catalog` returns change in the same pull request.
- **History stays RFC 6902** (JSON Patch). New edit syntax is sugar that normalises to it.
- **Documents stay declarative:** no scripts inside documents.

## License and the NOTICE file

AffrescoUI is licensed under the [Apache License 2.0](LICENSE). By contributing, you agree that your contribution is
licensed under it too (section 5 of the license).

The [`NOTICE`](NOTICE) file and its credit lines must stay, unchanged, in the repository and in anything that
redistributes it. Third-party code and assets are listed in [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md): a new
dependency, font or icon set is discussed in an issue first, with its license and size.
