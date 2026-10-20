# `affrescoui`: the command-line tool for screens

One tool for screen files ([SCREEN_FORMAT.md](../../SCREEN_FORMAT.md)), for people, agents and CI. It is built with
the library (`cmake --build build` makes `build/affrescoui`; `-DAFFRESCOUI_BUILD_TOOLS=OFF` leaves it out) and installed
with it (`cmake --install`).

```sh
affrescoui check screen.json            # the schema, the format's rules and the checker
affrescoui fmt screen.json              # write it the way AffrescoUI files are written
affrescoui render screen.json -o out.png --scale 2
affrescoui run screen.json              # show it, and let the Studio and agents change it
affrescoui help                         # every command; affrescoui <command> --help for one
```

## Commands

| Command | What it does |
|---|---|
| `run <file> [--socket DIR] [--data DIR] [--mode default\|acceptEdits\|plan] [--plugin changes.json]...` | Shows a screen and lets peers change it, as `affrescoui_host` does: `studio.sock` and `agent.sock` in the socket folder, the history and `persist` state in the data folder, `--plugin` change sets applied once. `--mode` holds for this run only. |
| `check <file> [--json]` | Checks a screen without opening a window: the JSON, the schema (`schema/affrescoui-screen.schema.json`, built into the tool), the format's rules as the library loads it, and the checker (an `id` two nodes have, and what the screen refers to but doesn't define). One line per problem, or with `--json` one object: `{"file", "ok", "problems": [{"source", "place", "message"}]}`, `source` being `json`, `schema`, `format` or `checker`. |
| `render <file> -o out.png [--scale 1\|2] [--size WIDTHxHEIGHT]` | Draws the screen's first frame offscreen, as the rendering check does (a hidden window, the file as it is, no saved history or state, channels finding nobody), and writes a PNG. `--size` resizes the window first; a frame over 16,384 pixels a side (size times scale) is refused. Even a channel at an absolute path finds nobody. Needs OpenGL: a display, or Xvfb. |
| `fmt <file> [--check]` | Writes the file the way AffrescoUI files are written: tabs; an object written over several lines stays so, one member per line; anything else on one line, `{ "key": value }` and `[a, b]`, when it fits in 120 columns (a tab counts 4); empty lines between members kept; keys in their order; numbers and strings exactly as written. Formatting twice changes nothing. `--check` changes nothing and exits with 1 if the file isn't formatted. |
| `ids <file> [--check]` | Gives a readable id to every node that has none (`label-blocked`, `panel-top-bar`; SCREEN_FORMAT.md, "Every type"), writing only the new `"id"` members into the file, and lists them. `--check` changes nothing and exits with 1 if a node has none. |
| `export <file> [--from REVISION] [--name NAME] [-o changes.json] [--data DIR]` | Writes the revisions after `--from` (default 0, the file as loaded) of the screen's history on disk as a change set ("plugin"), to stdout or `-o`. |
| `import <file> <changes.json> [--data DIR]` | Applies a change set to the screen's history on disk, all or nothing, once: a change set the history already has is skipped. Refused while another program (a running host) writes that history. |

The history on disk is the one the host keeps: `<data folder>/<file name>.history`, the data folder being `--data`,
else `$AFFRESCOUI_DATA_DIR`, else `~/.affrescoui` (`%APPDATA%\affrescoui` on Windows). See
[VERSION_CONTROL.md](../../VERSION_CONTROL.md), "History on disk" and "Change sets".

When the tool is built with the examples (the default), it knows their application types, as `affrescoui_host` does
(the Forge template's `code`); other unknown types show as plain panels.

## Exit codes

| Code | Meaning |
|---|---|
| 0 | Done. For `check`, `fmt --check` and `ids --check`: nothing to report. |
| 1 | The file has problems: `check` found some, `fmt --check` or `ids --check` would change it, `import` was refused, or the file isn't JSON for `fmt` and `ids`. |
| 2 | The command line is wrong: an unknown command or option, a missing file or value. |
| 3 | It couldn't be done: a file can't be read or written, no window could be opened, the history is damaged or in use. |

## Tests

`check.py` here (ctest `cli`) runs every command on every template: `check` passes, `fmt --check` passes after
`fmt` and the JSON reads the same, every node has an id, `render` draws the same pixels as the rendering check's
runner for each template's first frame, a change set goes into a history on disk once and comes back out, and a wrong
command line exits with 2 and a missing file with 3. It needs a window, like the rendering check:

```sh
python3 tools/affrescoui/check.py     # or: ctest --test-dir build -R cli
```
