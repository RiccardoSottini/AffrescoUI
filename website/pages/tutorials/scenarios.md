# Scenarios and the rendering check

A **scenarios file** plays a screen the way a person would, headless, and says what failed. Agents run the same
scenarios (`run_scenarios`) to prove a change before you review it.

```json
--8<-- "recipes/counter.scenarios.json"
```

```sh
affrescoui test counter.scenarios.json
```

```text
counter.scenarios.json: 2 passed, 0 failed
```

- Steps: `click`, `drag`, `scroll`, `type`, `key`, `wait`, `resize`, `action`, `revision` (a change the application
  makes), `propose` (a change by an author, judged by the permissions like a peer's), `choose` (the answer to a file
  dialog), `expect` and `capture`.
- Expectations: a state value (`state` and `is`), a record (`data`), a node that exists, its text or a property, or an
  image compared with a golden one (`--goldens DIR`). A node can be found by `#id` or, as a screen reader finds it, by
  `role` and `name`.
- `--json` writes the results for tools, `--only NAME` plays one scenario.
- A connection's `mock` answers its requests, so scenarios never reach a server.

A second example, for the to-do list:

```json
--8<-- "recipes/todo-list.scenarios.json"
```

## The rendering check

The repository's own check plays every template, scripted, and compares the frames pixel for pixel with golden
images:

```sh
python3 tests/render/check.py
```

The golden images are not in git: the first run records them from the code you have, so record them before you
change anything, and rewrite them (`--update`) only for an intended visual change. The site's own build checks every
recipe on this site with `affrescoui check` and plays these scenarios files.

Next: [from C++](from-cpp.md).
