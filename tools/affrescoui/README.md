# `affrescoui`: the command-line tool for screens

One tool for screen files ([SCREEN_FORMAT.md](../../SCREEN_FORMAT.md)), for people, agents and CI. It is built with
the library (`cmake --build build` makes `build/affrescoui`; `-DAFFRESCOUI_BUILD_TOOLS=OFF` leaves it out) and installed
with it (`cmake --install`).

```sh
affrescoui check screen.json            # the schema, the format's rules and the checker
affrescoui fmt screen.json              # write it the way AffrescoUI files are written
affrescoui render screen.json -o out.png --scale 2
affrescoui test screen.scenarios.json   # play the screen's scenarios: what passed, what failed
affrescoui run screen.json              # show it, and let people's clients and agents change it
affrescoui help                         # every command; affrescoui <command> --help for one
```

## Commands

| Command | What it does |
|---|---|
| `run <file> [--socket DIR] [--data DIR] [--mode default\|acceptEdits\|plan] [--plugin changes.json]...` | Shows a screen and lets peers change it, as `affrescoui_launcher` does: `studio.sock` and `agent.sock` in the socket folder, the history and `persist` state in the data folder, `--plugin` change sets applied once. `--mode` holds for this run only. |
| `check <file> [--json]` | Checks a screen without opening a window: the JSON, the schema (`schema/affrescoui-screen.schema.json`, built into the tool), the format's rules as the library loads it, and the checker (an `id` two nodes have, and what the screen refers to but doesn't define). One line per problem, or with `--json` one object: `{"file", "ok", "problems": [{"source", "place", "message"}]}`, `source` being `json`, `schema`, `format` or `checker`. An app's folder or its `app.json` (SCREEN_FORMAT.md, "Apps: app.json") is checked whole: the manifest, its entry screen with its themes, models and seeds merged in as the host loads it (nothing is written), and source `capabilities` for each one the screen uses that the manifest doesn't declare. |
| `render <file> -o out.png [--scale 1\|2] [--size WIDTHxHEIGHT]` | Draws the screen's first frame offscreen, as the rendering check does (a hidden window, the file as it is, no saved history or state, channels finding nobody), and writes a PNG. `--size` resizes the window first; a frame over 16,384 pixels a side (size times scale) is refused. Even a channel at an absolute path finds nobody. Needs OpenGL: a display, or Xvfb. |
| `test <scenarios.json>... [--json] [--only NAME] [--goldens DIR] [--update] [--out DIR] [--document FILE]` | Plays a screen's scenarios (SCREEN_FORMAT.md, "Scenarios"; `modules/automation`), headless unless one expects a picture, and reports compactly: `index.scenarios.json: 7 passed, 0 failed`, then `FAILED <scenario>, step <n>: <what>` per failure; with `--json` one line per file, `{"file", "passed", "failed", "failures": [{"scenario", "step", "problem"}]}`. `--goldens` is where the golden images of `expect image` are, `--update` records them all from this run, `--out` receives captures and frames that differ with their diffs. `--document` plays on that document instead of the scenarios' screen file (the running screen's, a proposal applied), its fonts and includes still found next to the screen file. Exits with 1 if a scenario failed or the file is malformed. The library's log is quiet unless `--verbose`. |
| `policy test <file.policy.json>... [--json]` | Judges each case's revision, by its author, against the screen the file names (VERSION_CONTROL.md, "Policy tests"), applying nothing, and compares the decision (`allow`, `ask` or `deny`) and a part of the reason: `<file>: FAIL <case>: expected <decision>, got <decision> (<reason>)` per failure, then `index.policy.json: 8 passed, 0 failed`; with `--json` one line per file. Exits with 1 if a case failed or a file is malformed. |
| `fmt <file> [--check]` | Writes the file the way AffrescoUI files are written: tabs; an object written over several lines stays so, one member per line; anything else on one line, `{ "key": value }` and `[a, b]`, when it fits in 120 columns (a tab counts 4); empty lines between members kept; keys in their order; numbers and strings exactly as written. Formatting twice changes nothing. `--check` changes nothing and exits with 1 if the file isn't formatted. |
| `ids <file> [--check]` | Gives a readable id to every node that has none (`label-blocked`, `panel-top-bar`; SCREEN_FORMAT.md, "Every type"), writing only the new `"id"` members into the file, and lists them. `--check` changes nothing and exits with 1 if a node has none. |
| `export <file> [--from REVISION] [--name NAME] [-o changes.json] [--data DIR]` | Writes the revisions after `--from` (default 0, the file as loaded) of the screen's history on disk as a change set ("plugin"), to stdout or `-o`. |
| `schema [-o schema.json] [--doc SCREEN_FORMAT.md] [--types builtin\|examples] [--check]` | Generates the format's JSON Schema from the registries (the types, sections and steps the library has) and the reference in `modules/format/src/reference/Reference*.cpp`, to `-o` or to stdout, and with `--doc` the tables of `SCREEN_FORMAT.md` between its `<!-- generated: name -->` and `<!-- end generated -->` markers. `--types examples` adds the example apps' own types (`code`), the way an application's schema has its registered types. `--check` writes nothing and exits with 1 if a file is out of date: ctest `cli-reference` runs it on the repository's files. |
| `sign <file> --key KEYFILE [-o out.json] \| --new-key KEYFILE` | Signs a feature's manifest or a catalogue's index (RFC 0004) with an Ed25519 key, in place or to `-o`; an index first gets each release's `digest` and `size` from its file, so sign the manifests first. `--new-key` makes a key file only its owner can read (never over an existing file) and prints its public key, `ed25519:...`, which a host trusts with `Features::trustPublisher`. |
| `import <file> <changes.json> [--data DIR]` | Applies a change set to the screen's history on disk, all or nothing, once: a change set the history already has is skipped. Refused while another program (a running host) writes that history. |
| `migrate <file>... [--check] [--write]` | Rewrites version 1 files (screens, scenarios, change sets, feature packages, catalogue indexes) as version 2, changing only the keys and values that changed, and lists each rewrite; without `--write` nothing is written. `--check` only names the files that need it. Signed files are left as they are (SCREEN_FORMAT.md, "Migrating from version 1"). |

The history on disk is the one the host keeps: `<data folder>/<file name>.history`, the data folder being `--data`,
else `$AFFRESCOUI_DATA_DIR`, else `~/.affrescoui` (`%APPDATA%\affrescoui` on Windows). See
[VERSION_CONTROL.md](../../VERSION_CONTROL.md), "History on disk" and "Change sets".

When the tool is built with the examples (the default), it knows their application types, as `affrescoui_launcher` does
(the Forge template's `code`); other unknown types show as plain panels.

## Exit codes

| Code | Meaning |
|---|---|
| 0 | Done. For `check`, `fmt --check` and `ids --check`: nothing to report. |
| 1 | The file has problems: `check` found some, `fmt --check`, `ids --check`, `schema --check` or `migrate --check` would change it, `import` was refused, or the file isn't JSON for `fmt` and `ids`. |
| 2 | The command line is wrong: an unknown command or option, a missing file or value. |
| 3 | It couldn't be done: a file can't be read or written, no window could be opened, the history is damaged or in use. |

## Source

| File | What it holds |
|---|---|
| `main.cpp` | The command line: picks the command and prints `help` |
| `Cli.*` | What the commands share: exit codes, arguments, reading and writing files |
| `commands/` | One file per command, or per pair of commands that share their work: `check`, `fmt` and `ids`, `export` and `import`, `import --from`, `migrate`, `policy test`, `schema`, `sign`, `test`, `run` and `render` |
| `ComposeCommand.cpp` | `contributions` and `check --compose` |
| `JsonText.*` | JSON as AffrescoUI files are written (`fmt`) |
| `Schema.*` | The JSON Schema check (`check`, `schema`) |
| `Embed.cmake` | Embeds a file (the schema) in the tool as a function |

`Cli.cpp` and `ComposeCommand.cpp` stay beside `main.cpp`: Bottega's build compiles them by path.

## Tests

`check.py` here (ctest `cli`) runs every command on every template: `check` passes, `fmt --check` passes after
`fmt` and the JSON reads the same, every node has an id, `render` draws the same pixels as the rendering check's
runner for each template's first frame, every template's scenarios pass with `test` (and a failing expectation or a
malformed scenarios file exits with 1), a change set goes into a history on disk once and comes back out, and a wrong
command line exits with 2 and a missing file with 3. It needs a window, like the rendering check:

```sh
python3 tools/affrescoui/check.py     # or: ctest --test-dir build -R cli
```
