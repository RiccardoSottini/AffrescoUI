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
| `Sockets.hpp` | `SocketChannel` (`connect` to a path, or wrap a connected socket; a bounded send queue) and `SocketServer` (listen; `onConnection`; a `PeerCheck`, `peerIsThisUser` by default) |
| `Channels.hpp` | `ProcessChannel` (a child process's stdin and stdout) and `StdioChannel` (this process's) |
| `Session.hpp` | `Session`: the protocol for one peer over a `Channel`; `send`, `author`, `onRelay` |
| `Limits.hpp` | `PeerLimits` (what one peer may send: message size, messages and changes a second) and `TokenBucket` |

## Invariants

- **A peer is who it was connected as.** Everything it sends is authored by the session's `author` (followed by the
  name it gives itself after a `/`), so a peer can't claim to be someone else. Review messages are honoured only for
  authors the document lets review.
- **Untrusted input has limits:** a line longer than `maxMessageBytes` (64 MiB) closes the channel, on every
  transport and system (they share `Lines.hpp`); every message is parsed with `parseUntrusted` (nesting 256, in time
  in proportion to its text); a malformed or refused message gets an `error` or `rejected` answer and never ends the
  session; `render` and `getEvidence` take a scale from 1 to `ScreenImages::maxScale` (2).
- **Evidence comes from the host:** `getEvidence` answers with `ScreenImages::evidence` (pictures, region, behaviour
  lines, scenario results, changed texts); nothing in it comes from the proposal's author.
- **Rates (`PeerLimits`):** messages of any kind, changes (they rebuild the screen) and pictures (`render` draws it, and
  builds a proposal's and main's again; `getEvidence` draws two) each have a token bucket of their own; pictures use the changes' rate.
- **Only this user can connect.** On POSIX, the socket file is `0600`; on Windows, its access list has one entry, for
  this user. Each connection is checked when it is accepted: a program of another user is refused, closed and
  logged (`SO_PEERCRED`, `getpeereid`, or on Windows the peer's process and its token). (The programs in
  `examples/` also check that the socket folder belongs to this user: `examples/SocketDir.hpp`.)
- **A peer is held to `PeerLimits`** (8 MiB a message, refused unparsed; 100 messages a second; 120 changes a
  minute, and as many pictures; see the rates above), and one that reads slower than the app writes to it is
  disconnected once 64 MiB wait for it.
- **`request` and `reply` aren't interpreted:** they come out of `onRelay`, stamped with `from`, for whoever connects
  peers to each other (`affresco::Screen` passes them to the other peers).
- **Closing a `ProcessChannel` ends the child and everything it started** (a process group on POSIX, a job object on
  Windows), and the child inherits no other pipe.

## Threads

Each transport has a thread of its own that only reads (or accepts) and posts what arrives to the `Executor`; a
`SocketChannel` also has a writer thread, which writes what `send` queued, so the main thread never waits for a peer.
Everything else runs on the main thread: `onMessage`, `onClosed`, `onConnection`, `send` and the whole `Session`.
A channel being destroyed stops its threads; deliveries still queued for it do nothing.

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
- The number of peers isn't bounded, and neither is the queue a socket's reader hands lines to the main thread
  through (`SECURITY.md`, known gaps).

## Extension points

- **A new transport:** implement `Channel` (`core`). `Lines.hpp` has the line framing the transports share (a reader
  thread that posts each line, the length limit, and `dropCarriageReturns` for CRLF line breaks).
- **A new protocol message:** see the recipe below. `Session::handle` passes each message to its group:
  `handleChange` (revisions, restore, revert, undo), `handleReview` (accept, reject, preview, mode, revoke: it checks
  that the peer may review, then `decide` carries the decision out) or `handleQuery` (document, catalog, history,
  diff, change sets, render).

## Files

```
include/affresco/channel/   Sockets.hpp, Channels.hpp, Session.hpp, Limits.hpp
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
