# channel: peers and the live protocol

`channel` connects the app to other programs. Its transports implement `core`'s `Channel` (one message per line, both
ways): `SocketChannel` and `SocketServer` over a local socket that only this user may open, `ProcessChannel` to a
child process, and `StdioChannel` over the app's own stdin and stdout (how MCP clients start servers). On top of any
of them, `Session` speaks the live-document protocol (`SCREEN_FORMAT.md`, "Peers and the live protocol") for one
peer: it reads the document and its history, sends revisions, and, if the document lets it review, decides on
proposals. It only sees `core` interfaces (`VersionControl`, `ScreenImages`), so it works with any document format.
Target: `affrescoui::channel` (uses `core` and nlohmann/json; Winsock on Windows).

## Public headers (`include/affresco/channel/`)

| Header | Offers |
|---|---|
| `Sockets.hpp` | `SocketChannel` (`connect` to a path, or wrap a connected socket) and `SocketServer` (listen; `onConnection`) |
| `Channels.hpp` | `ProcessChannel` (a child process's stdin and stdout) and `StdioChannel` (this process's) |
| `Session.hpp` | `Session`: the protocol for one peer over a `Channel`; `send`, `author`, `onRelay` |

## Invariants

- **A peer is who it was connected as.** Everything it sends is authored by the session's `author` (followed by the
  name it gives itself after a `/`), so a peer can't claim to be someone else. Review messages are honoured only for
  authors the document lets review.
- **Untrusted input has limits:** a line longer than `maxMessageBytes` (64 MiB) closes the channel; every message is
  parsed with `parseUntrusted` (nesting 256); a malformed or refused message gets an `error` or `rejected` answer and
  never ends the session; `render` takes a scale from 1 to `ScreenImages::maxScale` (2).
- **Rates (`PeerLimits`):** messages of any kind, changes (they rebuild the screen) and pictures (`render` draws it, and
  builds a proposal's and main's again) each have a token bucket of their own; pictures use the changes' rate.
- **Only this user can connect.** On POSIX, the socket file is `0600`; on Windows, its access list has one entry, for
  this user. (The programs in `examples/` also check that the socket folder belongs to this user:
  `examples/SocketDir.hpp`.)
- **`request` and `reply` aren't interpreted:** they come out of `onRelay`, stamped with `from`, for whoever connects
  peers to each other (`affresco::Screen` passes them to the other peers).
- **Closing a `ProcessChannel` ends the child and everything it started** (a process group on POSIX, a job object on
  Windows), and the child inherits no other pipe.

## Threads

Each transport has a thread of its own that only reads (or accepts) and posts what arrives to the `Executor`.
Everything else runs on the main thread: `onMessage`, `onClosed`, `onConnection`, `send` and the whole `Session`.
A channel being destroyed stops its thread; deliveries still queued for it do nothing.

## Errors

- Failing to start (connect, listen, start a process, a socket path too long) throws `std::runtime_error` with the
  reason.
- After that, transport problems don't throw: the channel closes (`onClosed`) and the owner decides what to do.
- Protocol problems are answers to the peer: `{"type": "error", "message": ...}` for a malformed message,
  `{"type": "rejected", ...}` for a refused change, with the `id` the peer gave. A refused revision's `error` names
  the operation, the path and a hint from the format (`... no node with id 'logoo' (did you mean '#logo'?)`), so a
  peer can fix it without reading the screen again.
- The session knows nothing of nodes: narrow reads (an outline, one node by `#id`, a search) are made by the peer on
  the document `getDocument` returns, as the MCP server does (`find_nodes`, `get_screen` with `outline`, `at`,
  `depth`). Only what crosses the local socket grows with the document, not what an agent reads.
- The peer's user id isn't checked yet, and there is no rate limit (`SECURITY.md`, known gaps; 1.8b).

## Extension points

- **A new transport:** implement `Channel` (`core`). `Lines.hpp` has the line framing the transports share (a reader
  thread that posts each line, and the length limit).
- **A new protocol message:** see the recipe below. `Session::handle` passes each message to its group:
  `handleChange` (revisions, restore, revert, undo), `handleReview` (accept, reject, preview, mode, revoke) or
  `handleQuery` (document, catalog, history, diff, change sets, render).

## Files

```
include/affresco/channel/   Sockets.hpp, Channels.hpp, Session.hpp
src/
  Sockets.cpp               SocketChannel and SocketServer; the POSIX and Winsock differences in one block
  Channels.cpp              ProcessChannel and StdioChannel; the process platform layer (POSIX and Windows)
  Lines.hpp                 line framing shared by the transports
  Session.cpp               the live protocol: reading, answering, base64 pictures
CMakeLists.txt              the affrescoui_channel target and its sources
```

## Recipes

**Add a protocol message.**
1. Handle it in the right group in `Session.cpp`; read its fields with `expect::` so errors name the place.
2. If it changes anything or reaches outside the document, check who may do it (as `handleReview` does for
   reviewers), and ask the maintainer: it touches the security model.
3. Document it in the protocol table of `SCREEN_FORMAT.md`, and in the MCP server (`mcp/`) if agents use it.
4. Add unit tests to `tests/unit/channel.cpp` (the `MemoryChannel` in `tests/unit/Helpers.hpp` needs no socket), and
   seeds for the `session` fuzz target (`tests/fuzz/corpus.py`).

**Talk to the app from another program.** Connect to the host's socket (`affrescoui_host --socket <folder>`), send
one JSON object per line, and read the answers: `hello` first, then `committed`, `review` and `event` messages as
they happen. `mcp/affrescoui_mcp.py` is a complete peer in Python.
