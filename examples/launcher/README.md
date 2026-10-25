# Launcher

The home of AffrescoUI's apps: every example as a card to open with a click, the apps the person made, a form to
create a new one, and the settings. It is a screen document like the others, with no C++ of its own: `affrescoui_launcher`
opens it when it is started without a file.

```sh
./build/affrescoui_launcher                      # the Launcher; your apps go to ~/AffrescoUI
./build/affrescoui_launcher --apps /some/folder  # another apps folder
./build/affrescoui test examples/launcher/index.scenarios.json
```

## What it shows

- **Apps:** every app as a grid of cards, the person's own first, then the examples of [`apps.json`](apps.json) (cover,
  name, a tagline in the person's language). Each card is tagged **Yours** or **Example**. A search field, and Enter or a
  click to open one. A host-run example can also start a new app.
- **My apps:** each folder of the apps folder that holds an `index.screen.json`, named by its window's title: open,
  duplicate, show in its folder, and delete after a confirm dialog (Delete on a focused row asks too).
- **Create:** a name (checked as it is typed: its folder's name, the names the examples keep, the apps there already),
  a starting point (a blank app, [`starters/blank.screen.json`](starters/blank.screen.json), or a copy of a host-run
  example, without its C++), an optional description and features from other apps, which become the first request
  to the new app's assistant (`--ask`). The app opens at once.
- **Settings:** theme and language (kept with `persist`), the apps folder, the keyboard shortcuts, and the container
  `settingsIntelligence` for the intelligence settings.
- English, Spanish and French ([`launcher-strings.json`](launcher-strings.json)), light and dark, the keyboard
  throughout (Cmd+1, Cmd+2, Cmd+N, Cmd+, and Cmd+F), from 800 × 600 up.

## How it holds together

- **What the host grants** (`grantLauncher` in [`host.cpp`](../host/host.cpp)), and only when it opens the Launcher:
  the examples folder, read-only (the service `examples`); the apps folder (`apps`, which may copy from `examples`);
  and the app-programs grant `affresco` (the service `launch`), which starts `affrescoui_launcher` on a `.screen.json`
  inside one of those two folders (with `--ask TEXT`), or `affrescoui_forge`, found next to the host, each detached,
  with the host's `--data` and a socket folder of its own (`<socket folder>/<root>-<app>`), so several apps run at once. The screen names roots and programs, never a path.
- **The apps folder** holds one folder per app and `shared/`, a copy of `examples/shared` without its C++ (fonts,
  add-ons, components, languages), brought up to date at each create, so the apps' `../shared/...` paths hold as in
  `examples/`. The names of the examples' folders are kept: an app's state and history in the data folder are named
  after its folder.
- **Without the grants** (the rendering check, `affrescoui test`, a screenshot run), the services are refused: the
  examples come from `apps.json`, My apps says the folder isn't available, and scenarios play the folder's answer with
  the action `launcher.show`.

| File | What it is |
|---|---|
| `index.screen.json` | The screen document |
| `launcher-strings.json` | Its text in three languages |
| `apps.json` | The examples: id, name, tagline, folder, program, cover |
| `covers/` | The examples' pictures, 480 pixels wide (from `.github/media/`) |
| `starters/blank.screen.json` | A new blank app, with the assistant open |
| `index.scenarios.json` | Its scenarios; its pictures are in `tests/render/scenarios/launcher.json` |

Its fonts are in [`../shared/fonts`](../shared/fonts/README.md).
