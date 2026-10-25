# A first screen

A screen is one JSON file. This one shows a counter and a button:

```json
--8<-- "recipes/counter.screen.json"
```

Read it from the top:

- `format` and `version` say what the file is. `$schema` lets an editor validate and complete it
  ([JSON Schemas](../reference/schemas.md)).
- `window` gives the title and the size in window units (points on a HiDPI display).
- `fonts` names TrueType or OpenType files, relative to the screen file. `default` is used by text without a `font`.
- `state` is the live data. `{{state.count}}` in a text shows it, and updates when it changes.
- `root` is the node tree. A node has a `type` (`label` here), a position and size, its look (`color`, `radius`,
  `textColor`) and its behaviour (`onClick`, a list of steps).
- `id` names a node, so revisions, scenarios and the checker can find it.

## Check it, draw it, run it

Save the file as `counter.screen.json` (the recipes are in `website/recipes`, so their font paths start with
`../../`), then:

```sh
affrescoui check counter.screen.json          # the JSON, the schema, the format's rules and the checker
affrescoui render counter.screen.json -o counter.png
affrescoui run counter.screen.json            # a window; peers can change it while it runs
```

`check` reports each problem with its place and a hint, for example a misspelt state path:

```text
counter.screen.json: checker: root.children[0].text: 'state.cuont' doesn't exist (did you mean 'state.count'?)
```

`affrescoui fmt` writes the file the way AffrescoUI files are written, and `affrescoui ids` gives every node without
an `id` a readable one.

## Layout

Positions can be fixed (`x`, `y`, `width`, `height`), anchored to the parent's edges (`anchors`), or left to a `row`,
`column` or `grid`:

```json
--8<-- "recipes/anchors-layout.screen.json"
```

Next: [state, expressions and actions](state-expressions-actions.md). Every key is in
[the screen format](../reference/screen-format.md).
