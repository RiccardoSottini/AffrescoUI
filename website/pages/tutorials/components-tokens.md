# Components and tokens

## Styles and components

A **style** is a named set of properties a node takes with `"style": "name"`; a style can build on another. A
**component** is a node with parameters, used with `"use": "name"` and given values with `with`:

```json
--8<-- "recipes/component-chips.screen.json"
```

- `chip` and `chipOn` are styles; `chipOn` builds on `chip`.
- The `chip` component takes `label` and `on`, and chooses its style from `on`.
- The row repeats the component once per tag. A click sets the filter, and `computed.shown` filters the cards.

## Tokens and themes

**Tokens** are named values: numbers (spacing, radii, type sizes) and colours, written `"@name"` wherever such a
value fits. A token can have a value per theme, and `theme` picks the one in use:

```json
--8<-- "recipes/light-dark-tokens.screen.json"
```

The toggle sets `state.theme`, and every `@color.*` switches with it.

## Includes

`include` merges other files into the screen: colours, styles, components and actions kept in parts that several
screens share.

```json
--8<-- "recipes/include-parts.screen.json"
```

with `parts/palette.json`:

```json
--8<-- "recipes/parts/palette.json"
```

Next: [an agent changes a running app](agent-review.md).
