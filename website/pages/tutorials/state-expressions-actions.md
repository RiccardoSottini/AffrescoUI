# State, expressions and actions

Behaviour that usually needs code is part of the document: a list bound to data, a field that adds to it, a value
computed from another. Because it is data, an agent can change it as safely as a colour.

## A to-do list

```json
--8<-- "recipes/todo-list.screen.json"
```

- **State** holds the data: `state.draft` (what is typed) and `state.items`.
- **`bind`** ties the text area to `state.draft` both ways.
- **`repeat`** builds the row once per item, with `item` and its index `i` in scope. Rows update in place when the
  list changes.
- **Actions** are named lists of steps. `onSubmit: "add"` runs `add` when Enter is pressed. Its steps stop early on an
  empty draft (`if`, `stop`), `push` the trimmed text and clear the field.
- **Expressions** are `{{...}}` in JavaScript-style syntax, with functions such as `trim`, `empty`, `filter`, `map`,
  `sum`, `sort` and `plural`. Only `state` paths can be changed: steps write `state.items`, never a computed value.

## Computed values and typed state

```json
--8<-- "recipes/slider-computed.screen.json"
```

- `computed.fahrenheit` is recalculated whenever `state.celsius` changes.
- `stateTypes` declares what a state key holds, as JSON Schema. The screen's own state must match, a revision may not
  bring a value that doesn't, and the checker reports a step or a `bind` that can't match. While the screen runs,
  steps aren't stopped by the types: they are a contract for revisions and a description for agents and tools.

## Steps

A step is one object whose first key names it: `set`, `toggle`, `increment`, `push`, `remove`, `if`, `run`, `stop`,
`focus`, `navigate`, `request`, `invoke` and more. [The catalog](../reference/catalog.md) lists every step with its
arguments; [the screen format](../reference/screen-format.md) (“Actions”) explains each.

Events give steps their data: `event.value` for a control's new value, `event.data` for what was dropped, `event.column`
for a table's sort. The [cookbook](../cookbook.md) has a recipe for each common event.

Next: [components and tokens](components-tokens.md).
