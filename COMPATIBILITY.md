# Compatibility

What AffrescoUI keeps stable, and how it changes what it can't keep. Four things have a promise: the screen format,
the C++ API, the socket protocol and the MCP tools. Each one is versioned on its own. A release says in CHANGELOG.md
which of them changed.

## The screen format

Every file says which format version it was written for (`"version": 2`). The current version is in
[SCREEN_FORMAT.md](SCREEN_FORMAT.md). The schema in `schema/` keeps a definition for each version (`$defs/v2`).

- **Additions don't need a new version.** A new node type, key, step, function or section is added to the current
  version. Older files stay valid. A file that uses the new feature needs a library that has it, and an older library
  refuses that file with "unknown property" or "unknown function".
- **A breaking change needs a new version and a migration.** Removing or renaming something, or changing what a value
  means, waits for the next version, and that version ships with `affrescoui migrate`:
  - `affrescoui migrate` rewrites the files of the version before it;
  - the loader migrates those files in memory;
  - the checker refuses the old form in new revisions, with a hint naming the new form.

  [SCREEN_FORMAT.md, "Migrating from version 1"](SCREEN_FORMAT.md#migrating-from-version-1) covers version 2.
- **A key is deprecated before it is removed.** The checker warns about a deprecated key for at least one minor
  release before the version that removes it. The warning names the replacement.
- **A history isn't rewritten.** When a new version arrives, the commits already on disk stay as they were recorded.
  A screen whose head is older gets one commit on top that holds the migration ([VERSION_CONTROL.md](VERSION_CONTROL.md)).
- **Signed files stay as signed.** A feature package or a catalogue index is migrated in memory after its signature is
  checked. `affrescoui migrate` leaves a signed file as it is, because rewriting it would break the signature.
- Before 1.0, the version changes only for a consolidation like version 2. From 1.0, a new format version comes only
  with a new major release of the library.

## The C++ API

The library follows [semantic versioning](https://semver.org). The public API is every header under
`modules/*/include/affresco/`, installed as `<affresco/...>`. Code under `src/` and in a `detail` namespace isn't part of
it, even when a public header has to show it.

- **Source compatible within a major release.** Code that builds against 1.x also builds against a later 1.y. A
  minor release may add functions, types, members with defaults and enumerators at the end.
- **ABI kept within patch releases.** A patch release changes no class layout, virtual table, inline function or
  exported symbol. Rebuild against a new minor release. The libraries are static by default.
- **Classes that will grow hide their members.** `Screen` keeps its state behind a pointer (pimpl), so new members
  don't change its layout. The plain value types (`Vec2`, `Rect`, `Color`, `Mods`, `KeyEvent`, `format::Rewrite`, and
  the other structs whose fields are public) are frozen. Their fields don't change within a major release.
- **Deprecated before removal.** A function to be removed is marked `[[deprecated("use ...")]]` for at least one minor
  release, and is removed only in a major release.
- `tests/package/headers.cpp` builds each public header on its own against the installed package, so a header can't
  depend on another header that it doesn't include. `tests/package/headers.py` regenerates it when headers are added.
- Before 1.0, a minor release (0.x) may break the API. CHANGELOG.md lists what changed. The CMake package accepts only
  the same minor version (`SameMinorVersion`) while the version is 0.x.

## The socket protocol

Peers talk to a screen with one JSON object per line ([SCREEN_FORMAT.md, "Peers and the live
protocol"](SCREEN_FORMAT.md#peers-and-the-live-protocol)).

- **Additions are compatible.** A new message type, or a new member in a message, can arrive in any release. Peers
  ignore message types and members they don't know. The host answers an unknown request with `error`, and the
  connection stays open.
- **Nothing is removed or redefined within a major release.** A message keeps its members and their meaning. The
  `hello` message carries the screen's `format` version, so a peer can tell which document format it will receive.
- The limits per peer (`channel::PeerLimits`) are the host's to set, and may change in any release.

## The MCP tools

`mcp/affrescoui_mcp.py` exposes the screen to AI agents as MCP tools.

- **Tool names and required arguments are stable within a major release.** New tools, and new optional arguments,
  can come in any release.
- The guide text and the descriptions the server sends may change in any release. They describe the current format
  version.
- The server negotiates the MCP protocol version with the client, and it answers with the newest version it supports
  when the client asks for one it doesn't know.
