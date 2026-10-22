# The document server

`affrescoui_server` keeps screen documents and their history on disk and shares them, live, with the people its
token file names: no window, no GPU. Every person connects over WebSocket with a token, opens a document and speaks
the live protocol on it (the same one a host's peers speak, `modules/channel`), with the document's permissions
(SCREEN_FORMAT.md, "permissions"), plus presence (who is where) and leases (a part of the document one person holds
for a while).

```sh
cmake --build build --target affrescoui_server
./build/affrescoui_server --folder data --add-token ada   # prints ada's token, once
./build/affrescoui_server --folder data                   # serves on 127.0.0.1:7420
```

Options: `--listen ADDRESS:PORT` (default `127.0.0.1:7420`), `--allow-remote` (accept peers from other machines: only
behind a proxy that terminates TLS, see [`examples/deploy`](../deploy/README.md)), `--origin URL` (repeatable: the
browser origins allowed). It stops on SIGINT or SIGTERM. [`Dockerfile`](Dockerfile) builds a container of it.

## The folder

| Path | Holds |
|---|---|
| `tokens` | One line per token: `<SHA-256 of the token, hex> <name>`. `--add-token` appends one; deleting the line (and restarting) revokes it |
| `documents/<name>.screen.json` | The shared documents, as loaded at start (the history holds what changed since) |
| `history/<name>/` | Each shared document's history (`Repository::openHistory`) |
| `people/<person>/<name>.screen.json`, `.history/` | Each person's own documents, made when first opened (e.g. Forge's settings) |

A name is 1 to 64 of `A-Z a-z 0-9 _ -`. The token file keeps only hashes, so reading it reveals no token; keep it
readable by the server's account alone (`--add-token` makes it so).

## The protocol

On connecting, a peer is told the documents and who its token says it is:
`{"type": "documents", "documents": ["kanban"], "author": "ada"}`. A connection opens one document:

| Peer sends | Server answers |
|---|---|
| `{"type": "open", "document": "kanban"}` | The live protocol's `hello`, then `presence` and `leases`. `"~settings"` is the person's own document of that name |
| `{"type": "presence", "where": "#board", "doing": "editing"}` | `presence` to everyone on the document. `where` ≤ 256 bytes, `doing` ≤ 64 |
| `{"type": "getPresence"}` | `presence` and `leases` |
| `{"type": "lease", "target": "#board", "seconds": 300, "id": 1}` | `leased`, or `rejected` with `error`; then `leases` to everyone |
| `{"type": "release", "target": "#board", "id": 2}` | `released`, or `error`; then `leases` to everyone |
| Anything else (`revision`, `accept`, `history`, ...) | The live protocol (channel::Session), with the token's name as author |

`presence` is `{"type": "presence", "peers": [{"author", "where", "doing"}]}`; `leases` is
`{"type": "leases", "leases": [{"target", "holder", "seconds"}]}`. A lease's target is an `#id` or a JSON pointer;
while it runs, nobody but its holder may change what is inside it (a deny rule each, checked like the document's
own), and it ends when its time is up, when released, or when its holder's connection closes. Leases are bounded
(1 hour each, 16 per person, 256 per document) and so are presence entries (256 per document); the server's own
messages are limited to 20 a second per connection.

`tests/unit/document_server.cpp` runs it end to end: two people and a reviewer, a lease, a proposal accepted, and
the history kept across a restart.
