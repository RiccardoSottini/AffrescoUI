# themes: kits and themes

`themes` is the middle of the three layers (apps, themes, engine; `ARCHITECTURE.md`): the components a document may
`use` without defining them (the kits: menus, toolbars, docks, dialogs, the chat components) and the design tokens its
colours may name without defining them (the themes: `@kit.surface` and the others, in a dark and a light variant).
They are JSON files, built into the library when it is compiled; the only C++ is the loader that reads them into one
`ComponentLibrary` (`affresco/core/ComponentLibrary.hpp`). Target: `affrescoui::themes` (uses `core` and
nlohmann/json).

The engine never knows these by name. `format` looks a name up in whichever library is installed and never links this
module; `affresco::Screen` (the composition root) installs it before it checks a document, and so does each other
entry point: the `affrescoui` command-line tool, the document server, the unit tests, the benchmarks and the fuzz
targets. A document's own components and tokens always win over the library's.

## Public header (`include/affresco/themes/`)

| Header | Offers |
|---|---|
| `Themes.hpp` | `library()`, the shipped kits and themes as a `ComponentLibrary`; `install()`, which makes them the library every document falls back on |

## Files

```
components/core.json      menu, submenu, contextMenu, menuBar, dialog, toast, hoverCard, settingsPage, quickPick
components/form.json      comboBox and passwordMeter
components/bars.json      the desktop kit's toolbar and statusBar
components/overlays.json  popover and toasts
components/controls.json  segmentedControl, searchField, breadcrumbs and tabStrip
components/dock.json      dock
components/panels.json    inspector and ribbon
components/chat.json      chatBubble, typingIndicator, proposalCard, chatComposer and chatLauncher
tokens/kit.json           the kit's tokens: kit.background, kit.surface, ..., kit.radius, kit.borderWidth
src/Themes.cpp            the loader
src/Embed.cmake           writes the files into a generated Embedded.cpp, as bytes
```

A components file is `{"note": "...", "components": {name: definition}}`, each definition what a document's
`components` section holds (`SCREEN_FORMAT.md`, Components): `params`, `node`, and `runsActions` when the component
runs the actions its parameters name. A tokens file is `{"note": "...", "tokens": {name: token}}`, a token a colour
`{"dark", "light", "about"}` or a size `{"value", "about"}`. A document that defines any name in a token's namespace
(`kit.`) gets both variants and follows its theme; one that doesn't gets the dark values.

## Invariants

- **Only what a document may write.** The components use no service, connection or native call: they are reviewed as
  a document's own would be, and a component that runs actions its parameters name says which (`runsActions`), so the
  control scan holds its uses to that (`modules/format/src/rules/RulesControl.cpp`).
- **One name, one definition.** Two files defining one component or token is an error when the library is first read
  (the unit tests read it).
- **Pictures.** The components' pictures are in the render check's golden images, so a change to one is a visual
  change.
- **Threads.** `library()` is read once, on first use from any thread, and never changes; `install()` publishes it
  with release ordering, and the format reads it with acquire ordering.

## Recipes

**Add a component to a kit.** Add it to the kit's file under `components/` (or a new file, listed in
`CMakeLists.txt`), give it `runsActions` if it runs actions its parameters name, rebuild, and add a scenario to the
render check. `affrescoui schema` is not needed: components aren't part of the schema.

**Add a theme's tokens.** Add a file under `tokens/`, listed in `CMakeLists.txt`, its names in a namespace of their
own (`brand.accent`), and run `affrescoui schema --doc SCREEN_FORMAT.md`: its tokens table lists them.

Tests: `tests/unit/themes.cpp` (the library and the lookup), `themes_kit.cpp`, `themes_dock.cpp`,
`themes_panels.cpp`, `themes_chat.cpp`, `overlays.cpp`, `quickpick.cpp`, `form_inputs.cpp`.
