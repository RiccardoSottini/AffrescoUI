# The AffrescoUI catalogue

The versions of apps and the features that the Launcher lists on its Catalogue page. Each one is added by a pull
request and checked by `affrescoui catalogue check`. The Launcher installs an entry with the same review as a file
opened from disk.

| Path | Holds |
|---|---|
| `index.json` | What is listed: `features` and `packages`, each release with its file, SHA-512 digest and size |
| `<id>/<release>.affresco.json` | A package: a version of an app of `examples/`, as `affrescoui pack` writes it |
| `<id>/<release>.json` | A feature's manifest (RFC 0004) |

The index in the repository is **unsigned**, and a host refuses an unsigned index. The release job signs it with the
release key. The Launcher of a release trusts only that key's public half, which is built into it
(`AFFRESCOUI_CATALOGUE_KEY`). A development build has no key built in, so it lists no catalogue. To try a change
locally, pass the folder and say that it is unsigned:

```sh
./build/affrescoui catalogue refresh catalogue   # writes each release's digest and size into index.json
./build/affrescoui catalogue check catalogue     # one line per problem, exit 1 on any
./build/affrescoui_launcher --catalogue catalogue --trust-unsigned-catalogue   # prints a warning
```

A signature says who published the index and that it is unchanged since they did. It is no promise about what an
entry does. Installing a package still makes a proposal that only the person accepts. A feature from the catalogue is
reviewed in the Launcher and installed from inside the app that binds what it needs.

How to add an entry, and what the check and a reviewer look at: `CONTRIBUTING.md`, "Adding to the catalogue".
