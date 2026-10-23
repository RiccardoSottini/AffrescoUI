# AffrescoUI for VS Code

Editor support for AffrescoUI screen documents: schemas, problems from the `affrescoui` tool, a live preview,
navigation, hovers and snippets. The extension is not on any marketplace: download `affrescoui-vscode-<version>.vsix` from the
GitHub Release (or build the `.vsix` below) and install it with **Extensions: Install from VSIX...**.

## What it does

- **Schemas.** `*.screen.json` and `*.scenarios.json` files are validated against the schemas in
  [`schema/`](../../schema), and feature manifests (`features/<name>/feature.json`) against the one in
  [`schemas/`](schemas), written from the rules the format module checks. Completion and validation come from VS
  Code's JSON support.
- **Problems.** When a screen is opened or saved, the extension runs `affrescoui check --json <file>` and shows what
  it reports in the Problems view, each at the property it is about. The tool checks the file on disk, so problems
  follow saves, not typing. A run is debounced and stopped after `affrescoui.check.timeoutMs`.
- **Preview.** **AffrescoUI: Open Preview** (also in the editor title bar of a screen) runs `affrescoui render` and shows
  the PNG of the first frame beside the editor, drawn again on every save. The webview runs no script and loads
  nothing: its content security policy allows only the image, inlined as a `data:` URI. Renders larger than
  `affrescoui.preview.maxPixels` on a side, or slower than `affrescoui.preview.timeoutMs`, are refused.
- **Navigation.** Go to Definition and Find All References for components (`"use"`), included files, nodes
  (`"#id"`, and the `"id"` that defines them), tokens and colours (`"@name"`), styles, actions (handlers and `keys`),
  screens (`"navigate"`), and `state.` and `computed.` paths in expressions, across a screen and the parts it
  includes. In a scenarios file, `"#id"` and paths resolve in the screen its `"screen"` member names.
- **Hovers.** A screen section, a node property or a step shows its description from the catalog (the screen
  schema); a name shows its definition.
- **Snippets.** Type `affrescoui-` in a JSON file: a screen, a label, a button, a row, a column, a text area, a
  repeated list, an action, a component, a scenarios file and a feature manifest.

The extension runs the `affrescoui` tool for `check --json` and `render` only, never through a shell, and nothing
else.

## Settings

| Setting | Default | What it does |
| --- | --- | --- |
| `affrescoui.cliPath` | `affrescoui` | The tool: a name looked up on `PATH`, or a path. Machine-wide only, never taken from a workspace. |
| `affrescoui.check.enabled` | `true` | Runs `affrescoui check` on open and save. |
| `affrescoui.check.timeoutMs` | `15000` | How long a check may run. |
| `affrescoui.preview.scale` | `1` | `1` or `2`: the scale the preview renders at. |
| `affrescoui.preview.size` | (empty) | `WxH` to render at that window size; empty for the screen's own. |
| `affrescoui.preview.timeoutMs` | `20000` | How long a render may run. |
| `affrescoui.preview.maxPixels` | `4096` | The largest width or height the preview shows. |

## Workspace trust

Checks and previews run a program, so they run only in a trusted workspace. In Restricted Mode, schemas,
navigation, hovers and snippets still work, and `affrescoui.cliPath` can't be set by the workspace.

## Building

Node.js 20 or newer. The devDependencies are pinned by `package-lock.json`.

```sh
cd editors/vscode
npm ci
npm test            # compiles, then runs the unit tests of the resolver, the problem placement and the catalog
npm run package     # writes affrescoui.vsix
```

`affrescoui` comes from the main build (`cmake --build build --target affrescoui`); put `build/affrescoui` on `PATH`
or set `affrescoui.cliPath` to it. `affrescoui render` needs OpenGL.
