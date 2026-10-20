# Prototype for RFC 0004: features as packages

What [RFC 0004](../../0004-features.md) claims about composing features, tried out:

| File | What it is |
|---|---|
| `app.screen.json` | A small editor app in the RFC's syntax: two contribution points (`"point": "statusBar.right"`, `"point": "sidebar.views"`) and two `exports` (`editor.text`, `workspace.files`) |
| `word-count.feature.json` | A word count for the status bar: computed values, a command, a menu item and a view, reading `app.editor.text` |
| `todo-tree.feature.json` | A TODO tree for the side bar, grouped by file, with a command and a shortcut; it requires `wordCount` `^1.0.0` to show a dependency being checked |
| `broken.feature.json` | A package that is wrong in every way the checks look for |
| `compose.py` | The composer: `check` lists what keeps each package from installing, with its place; `install` writes the install revision (one record per feature under `/features`); `expand` writes the merged screen the runtime would build, in today's format |
| `install.revision.json` | What `install` writes for the two good features |
| `edits.json` | What an agent sends to add, switch off, order and remove the word count, today and with the RFC |

Try it from this folder, after building (only the standard library):

```sh
python3 compose.py check app.screen.json word-count.feature.json todo-tree.feature.json broken.feature.json
python3 compose.py install app.screen.json word-count.feature.json todo-tree.feature.json -o install.revision.json
python3 compose.py expand app.screen.json word-count.feature.json todo-tree.feature.json -o merged.screen.json
../../../build/affrescoui check merged.screen.json
../../../build/affrescoui render merged.screen.json -o merged.png
```

`check` says both good features can be installed and lists the broken one's 9 problems; installing the TODO tree
alone fails because it requires the word count. The merged screen passes `affrescoui check` (the checker's proof that
every reference resolves) and shows "19 words" in the status bar and a TODO tree of three items under the explorer.
With the render runner's scripted input, a click on the word count switches it to characters, a click on a TODO line
opens its file, and Shift+Cmd+T hides the tree.

Count the tokens:

```sh
pip install -r ../../../tests/perf/requirements.txt
python3 ../tokens.py .
```

The prototype doesn't verify signatures or read a catalogue: those are specified in the RFC and built in 5.5.
