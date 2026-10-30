# Features as packages

A **feature** adds state, computed values, actions, menu items and views to a screen, at named **points** the screen
offers, reading only the values the screen **exports**. Installed features are records under `/features/<name>` in the
document, so enabling, disabling, ordering and removing them are ordinary revisions: history, blame, undo and the
permission rules cover them.

## A feature in a screen

```json
--8<-- "recipes/feature-installed.screen.json"
```

with the status bar, which offers the point `statusBar.right`, in `parts/status-bar.json`:

```json
--8<-- "recipes/parts/status-bar.json"
```

- `exports` publishes `state.text` as `app.editor.text`, the only thing the feature may read from the app.
- `requires` names the format version, the points and the exports the feature needs; `provides` is what it adds.
- The feature's computed value is `computed.wordCount.words`, and its view joins the node offering
  `statusBar.right`. Turn it off with a revision: `{ "set": { "/features/wordCount/enabled": false } }`.

## A manifest

A feature is published as a manifest. This is the word count of the example IDE, whose toggle goes in its View menu:

```json
--8<-- "examples/forge/json/features/word-count/feature.json"
```

## Installing from a catalogue

A catalogue (`examples/forge/json/catalog/index.json`) lists each feature's releases with a digest and the publisher's
key, and is signed. The host decides which publishers and catalogues it trusts (`screen.features().trustPublisher(...)`,
then `addCatalog(...)`); nothing is trusted until it does. The example catalogue is signed with a throwaway key that
only the rendering check trusts: the stock host doesn't offer catalogues yet. The `features` service installs and
upgrades (`features.install {name, version}`, `features.upgrade {name}`): it fetches the manifest, checks its digest and
signature, and submits the record as a revision by the author `features`, which the permissions judge like any other.
An upgrade never changes the publisher, and a feature can never widen a capability or bring a control step.

`affrescoui sign` signs a manifest or a catalogue index, and `affrescoui sign --new-key` makes a key.

A feature written for one app can also be installed into another: its manifest names what it **needs**, the host
binds those needs to what it offers, and `affrescoui check --compose <screen> <feature>` proves the composition before
it is installed (“Features from another app” in [the screen format](../reference/screen-format.md)).

The design is [RFC 0004](https://github.com/RiccardoSottini/AffrescoUI/blob/master/rfcs/0004-features.md), and its
prototype (`rfcs/prototypes/0004-features`) composes features with only the Python standard library.

Next: [scenarios and the rendering check](scenarios.md).
