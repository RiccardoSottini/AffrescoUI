# The documentation site

A static site built with [MkDocs](https://www.mkdocs.org/) and [Material for MkDocs](https://squidfunk.github.io/mkdocs-material/).
The tutorials, the cookbook and the comparisons are written here; the references are generated from the repository
each time the site is built, so nothing is copied by hand.

| Path | What it is |
|---|---|
| `docs/` | The pages written by hand |
| `recipes/` | The cookbook's screens and scenarios, each checked by the build; pages include them, never copy them |
| `tools/check_snippets.py` | Checks every recipe (`affrescoui check`), plays every scenarios file (`affrescoui test`), and fails on a page that shows a screen inline instead of including a recipe |
| `tools/generate.py` | Stages the site in `build/docs`: the pages, the repository documents it includes (`SCREEN_FORMAT.md`, `VERSION_CONTROL.md`, `SECURITY.md`, the CLI's README), the catalog from `affrescoui schema`, the MCP tools from the server's tool list, the JSON Schemas, and the C++ API reference |
| `requirements.in` / `requirements.txt` | MkDocs and its plugins; `requirements.txt` pins every package by version and hash |
| `build.sh` | The whole build |

## Building

You need Python 3.10 or later (with `venv`), Doxygen, and the `affrescoui` tool:

```sh
cmake -S . -B build && cmake --build build --target affrescoui -j 4
website/build.sh
```

`build.sh` makes a virtual environment in `website/build/venv`, installs `requirements.txt` with `--require-hashes`,
checks every recipe, runs Doxygen with the repository's `Doxyfile` (its output folder changed, and its warnings shown but not fatal), generates the
references and builds with `mkdocs build --strict`: a failing recipe, a broken link or a missing page fails it. The
site is in `website/build/site`. `PYTHON` picks the interpreter and `AFFRESCOUI_CLI` the tool (else `build/affrescoui`,
else `affrescoui` on `PATH`).

To preview while writing, after one build: `cd website && build/venv/bin/mkdocs serve` (the generated pages are
those of the last build).

## Adding a recipe

Write `recipes/<name>.screen.json` with `"$schema": "../../schema/affrescoui-screen.schema.json"` and fonts from
`../../examples/shared/fonts/`, check it with `affrescoui check`, look at `affrescoui render`, and include it in a page:

````md
```json
--8<-- "recipes/<name>.screen.json"
```
````

## Updating the pinned packages

Edit `requirements.in`, then regenerate the pins with hashes, for example with
`uv pip compile --generate-hashes requirements.in -o requirements.txt` or pip-tools'
`pip-compile --generate-hashes`, and build again.

## Getting the site

Nothing deploys the site: not GitHub Pages, not a `gh-pages` branch, not a CDN. The output is plain static files,
and [`.github/workflows/release.yml`](../.github/workflows/release.yml) attaches them to the maintainer's GitHub
Release as `affrescoui-site-<version>.zip` (checksummed and attested). Download it from the release, unzip it and
open `index.html`, or serve the folder with any static server you run yourself. A pull request or push to `master`
only builds it ([`site.yml`](../.github/workflows/site.yml)) and keeps it as the run's artifact `site`, with read-only
permissions.
