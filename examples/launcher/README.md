# Launcher

The home of AffrescoUI's apps: every example as a card to open with a click, the apps the person made, a form to
create a new one, and the settings. It is a screen document like the others, with no C++ of its own: `affrescoui_launcher`
opens it when it is started without a file.

**JSON or Lisp:** it has real behaviour (listing, creating, duplicating and deleting apps, the assistant's settings), so it is also written as a Lisp program (RFC 0005), by hand beside its JSON, as every example is. It opens every example from the folder of the notation its Settings choose, but one with a single folder (Calamaio, `notation` in `apps.json`), and makes a person's app from one the same way. A copy of a `lisp/` folder holding a program is retitled and renamed by a keyword edit of its program's text (`setFormKeyword`), every other character as it was.

```sh
./build/affrescoui_launcher                      # the Launcher; your apps go to ~/AffrescoUI
./build/affrescoui_launcher --apps /some/folder  # another apps folder
./build/affrescoui test examples/launcher/json/index.scenarios.json
```

## What it shows

- **Apps:** a welcome for a newcomer (the mark, what the apps are, New app) while they have no app of their own; then
  every app as a grid of cards, the person's own first, then the examples of [`apps.json`](json/apps.json) (cover,
  name, a tagline in the person's language). Each card is tagged **Yours** or with its example's category
  (Productivity, Creative, Developer, Home & leisure, Forms: `categories` and `category` in `apps.json`), and the
  categories, with **Your apps** once there are some, filter the grid. A search field, and Enter or a click to open
  one. A host-run example can also start a new app.
- **My apps:** each folder of the apps folder that holds an `index.screen.json` (or a `json/` folder with one, or a `lisp/`
  folder with an `app.lisp`: a copy of an example, or of a program), named by its window's title (a program's `:name`), the last
  changed first (an app just made or duplicated is at the top at once, before the folder is read again): open,
  duplicate, show in its folder, and delete after a confirm dialog (Delete on a focused row asks too).
- **Share:** each My apps row has an Export button: a dialog with the version's name, description and author, what
  the file holds (your changes to the app it was made on, or the whole app), and what is never included (records,
  history, permissions given, keys and tokens, where your files are); Save asks where (`<id>.affresco.json`), and the
  toast names the file, never its folder. **Install from a file** opens one, and a review page shows what installing
  it would do, from its content: its name, release, description and who it says it is by, its signature, what it was
  made on, the changes per layer and definition, its hooks and advice, what it writes and uses, the capabilities it
  asks for (new ones marked), permissions, state, pictures and records; a file with problems lists them in red and
  can't be installed. Install, with a name checked as Create checks it, makes a new app at the top of My apps and
  opens it, its changes waiting for the person's review.
- **Catalogue:** shown only when the host grants a catalogue whose index it trusts (`catalogue/README.md`): cards for
  the versions of apps and the features it lists, with a search. A card opens the same review page, with the line
  "Listed in the AffrescoUI catalogue, checked on <date>". A version installs as a file does. A feature is reviewed
  there, with what it needs, and is installed from inside an app.
- **Commands** an agent can run, each once the person says yes, as it starts a program: `appOpen` (an app by its
  name or folder) and `appCreate` (a name, the example it starts from, and what its assistant is asked first), which
  fills in the Create page and makes the app as Create does, a name it refuses staying there with its reason.
- **Create:** a name (checked as it is typed: its folder's name, the names the examples keep, the apps there already,
  no double curly braces, which the new app's file would read as an expression; a folder of that name holding
  something else is said to be there, and kept),
  a starting point (a blank app, [`starters/blank.screen.json`](json/starters/blank.screen.json), or a copy of a host-run
  example's `json/` or `lisp/` folder, as Settings choose, every one shown at once, without its C++, except those `apps.json` marks `"starter": false`), an optional description and features from other apps, which become the first request
  to the new app's assistant (`--ask`). The app opens at once.
- **Settings:** theme and language (kept with `persist`), whether the examples open from their `json` or their `lisp`
  folder, and which one a new app is copied from (also kept; Calamaio, which `apps.json` marks `"notation": "lisp"`,
  has only its `lisp` folder and always opens from it), the apps folder, the keyboard shortcuts, and the container
  `settingsIntelligence` for the intelligence settings.
- English, Spanish and French ([`locales/`](json/locales)), light and dark, the keyboard
  throughout (Cmd+1, Cmd+2, Cmd+N, Cmd+, and Cmd+F), from 800 × 600 up.

## How it holds together

- **What the host grants** (`grantLauncher` in [`host.cpp`](../../apps/host/host.cpp)), and only when it opens the Launcher:
  the examples folder, read-only (the service `examples`); the apps folder (`apps`, which may copy from `examples`);
  and the app-programs grant `affresco` (the service `launch`), which starts `affrescoui_launcher` on a `.screen.json`
  inside one of those two folders (with `--ask TEXT`), found next to the host, each detached,
  with the host's `--data` and a socket folder of its own (`<socket folder>/<root>-<app>`), so several apps run at once. The screen names roots and programs, never a path.
  It also grants the `packages` service (`export`, `inspect`, `install`, `catalogue`, `setAside`; SECURITY.md, "The `packages` service") on
  those two roots, `apps` the only writable one, with the data folder, and the file dialogs (the capability `files`).
- **A name used again starts empty.** Once an app's folder is deleted, and once a new app's folder is made (by Create,
  Duplicate or Install, which covers a folder deleted in Finder), the data the data folder keeps for it (its state,
  the assistant's chat, records, history, usage and permissions) is set aside as `<name>.replaced-<UTC time>`, never
  erased (`packages.setAside`, `Screen::setDataAside()`). If some of it can't be moved, deleting goes on (the next app
  made there tries again), but the new app isn't made, as it would open with the old one's data; a host without the
  `packages` service makes it as before.
- **The apps folder** holds one folder per app and `shared/`, a copy of `examples/shared` without its C++ (fonts,
  add-ons, components, languages), brought up to date at each create, so the apps' `../shared/...` paths hold as in
  `examples/`. The names of the examples' folders are kept: an app's state and history in the data folder are named
  after its folder.
- **Without the grants** (the rendering check, `affrescoui test`, a screenshot run), the services are refused: the
  examples come from `apps.json`, My apps says the folder isn't available, and scenarios play the folder's answer with
  the action `launcher.show`, the service's with `launcher.export` and `launcher.review` (the summary in the event), and the catalogue's with
  `launcher.catalogue` (its rows) and `launcher.reviewListed` (an entry's summary and where it was listed).

| File | What it is |
|---|---|
| `index.screen.json` | The screen document |
| `locales/` | Its text, one file per language |
| `apps.json` | The examples: id, name, tagline, category, folder, program, cover; `notation` for one with a single folder, `starter: false` for one a new app isn't copied from; and `categories`, in the order the Apps page shows them |
| `covers/` | The examples' pictures, 480 pixels wide (from `.github/media/`) |
| `starters/blank.screen.json` | A new blank app, with the assistant open; its changes wait for the person's review |
| `index.scenarios.json` | Its scenarios; its pictures are in `tests/render/scenarios/launcher.json` |

Its fonts are in [`../shared/fonts`](../shared/fonts/README.md).
