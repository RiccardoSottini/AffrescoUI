# Cookbook

32 small screens, one per common task. Each passes `affrescoui check`; the site's build checks every one,
so they stay true to the code. They are in `website/recipes/`: open one with
`./build/affrescoui run website/recipes/<name>.screen.json`, or copy the part you need.

## Layout

### Anchors

Nodes placed against the edges of their parent, so they follow its size. (`anchors-layout.screen.json`)

```json
--8<-- "recipes/anchors-layout.screen.json"
```

### A grid of cards

A `grid` repeated over a list of items. (`grid-cards.screen.json`)

```json
--8<-- "recipes/grid-cards.screen.json"
```

### Split panes

A `split` with dividers the user drags; the pane sizes are bound to state. (`split-panes.screen.json`)

```json
--8<-- "recipes/split-panes.screen.json"
```

### Tabs

A `tabs` control bound to the open tab, with `onClose`. (`tabs.screen.json`)

```json
--8<-- "recipes/tabs.screen.json"
```

### Screens and navigation

Several `screens` shown in an `outlet`, switched by `navigate`. (`screens-navigation.screen.json`)

```json
--8<-- "recipes/screens-navigation.screen.json"
```

## Data and text

### Counter

State shown in a label and changed by a click. (`counter.screen.json`)

```json
--8<-- "recipes/counter.screen.json"
```

### Bound text

A text area bound to state both ways, with a label that follows it. (`text-binding.screen.json`)

```json
--8<-- "recipes/text-binding.screen.json"
```

### To-do list

A list in state, `repeat`, and an action that adds the typed item. (`todo-list.screen.json`)

```json
--8<-- "recipes/todo-list.screen.json"
```

### A slider and a computed value

A slider bound to state, a `computed` value from it, and `stateTypes`. (`slider-computed.screen.json`)

```json
--8<-- "recipes/slider-computed.screen.json"
```

### Search as you type

A query filtering a list through a computed value, applied after a `debounce` pause in typing. (`debounced-search.screen.json`)

```json
--8<-- "recipes/debounced-search.screen.json"
```

### Form validation

A `check` rule on a field, and its message shown with `when`. (`form-validation.screen.json`)

```json
--8<-- "recipes/form-validation.screen.json"
```

### Rich text and links

Bold, larger and coloured runs in one label, and `onLink` for its links. (`rich-text.screen.json`)

```json
--8<-- "recipes/rich-text.screen.json"
```

### Watch a value

`watch` runs steps whenever a state value changes, with the previous value. (`watch-changes.screen.json`)

```json
--8<-- "recipes/watch-changes.screen.json"
```

### Remember state

`persist` keeps state across restarts; `onLoad` runs at start. (`persist-state.screen.json`)

```json
--8<-- "recipes/persist-state.screen.json"
```

### A timer

`every` ticks while a node is shown, started and stopped with `when`. (`timer.screen.json`)

```json
--8<-- "recipes/timer.screen.json"
```

## Lists and trees

### A list with a selection

A `list` with multiple selection, bound to the picked items. (`list-selection.screen.json`)

```json
--8<-- "recipes/list-selection.screen.json"
```

### A sortable table

A `table` whose `onSort` records the sort column and direction in state. (`sortable-table.screen.json`)

```json
--8<-- "recipes/sortable-table.screen.json"
```

### A tree

A `tree` with `onExpand` and `onOpen`. (`file-tree.screen.json`)

```json
--8<-- "recipes/file-tree.screen.json"
```

### Drag and drop

Cards dragged between two lanes, moved by `onDrop`. (`drag-and-drop.screen.json`)

```json
--8<-- "recipes/drag-and-drop.screen.json"
```

### Filter chips

A component with parameters, repeated, choosing its style. (`component-chips.screen.json`)

```json
--8<-- "recipes/component-chips.screen.json"
```

## Commands, menus and dialogs

### Commands and keys

`keys` binds shortcuts to actions, and the list shows them. (`commands-keys.screen.json`)

```json
--8<-- "recipes/commands-keys.screen.json"
```

### A menu bar

Actions with titles and `enablement`, `menus`, the native menu bar or a drawn one. (`menu-bar.screen.json`)

```json
--8<-- "recipes/menu-bar.screen.json"
```

### A context menu

`onContextMenu` opens a menu where the user clicked. (`context-menu.screen.json`)

```json
--8<-- "recipes/context-menu.screen.json"
```

### A confirmation dialog

A `dialog` component that asks before an action runs. (`confirm-dialog.screen.json`)

```json
--8<-- "recipes/confirm-dialog.screen.json"
```

### A toast

A message that hides itself, with `debounce`. (`toast.screen.json`)

```json
--8<-- "recipes/toast.screen.json"
```

### Settings

`settings` declared with types and defaults, changed by an action and a shortcut. (`settings-page.screen.json`)

```json
--8<-- "recipes/settings-page.screen.json"
```

## Look

### Light and dark themes

Tokens with a value per theme, and a toggle that switches them. (`light-dark-tokens.screen.json`)

```json
--8<-- "recipes/light-dark-tokens.screen.json"
```

### Included parts

Colours and styles shared through `include`. (`include-parts.screen.json`)

```json
--8<-- "recipes/include-parts.screen.json"
```

## Outside the screen

### Call a service

`invoke` a method of a local service the host grants, with loading and errors. (`service-invoke.screen.json`)

```json
--8<-- "recipes/service-invoke.screen.json"
```

### Call a server

`request` a declared operation, with a `mock` for previews and scenarios. (`connection-request.screen.json`)

```json
--8<-- "recipes/connection-request.screen.json"
```

### Permission rules

What the person's client and agents may change, what waits for review, what nobody may. (`permissions.screen.json`)

```json
--8<-- "recipes/permissions.screen.json"
```

### An installed feature

A feature adding a computed value and a view at a point of the screen. (`feature-installed.screen.json`)

```json
--8<-- "recipes/feature-installed.screen.json"
```
