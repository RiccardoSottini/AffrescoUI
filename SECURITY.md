# Security

AffrescoUI shows a screen document that other programs change while it runs. This file says what the library
trusts, what it doesn't, the limits it puts on untrusted input, and the gaps it knows about. Version control and
permissions are in [`VERSION_CONTROL.md`](VERSION_CONTROL.md); the format is in [`SCREEN_FORMAT.md`](SCREEN_FORMAT.md).

## Reporting a problem

Please report a vulnerability privately to the maintainer, through a GitHub security advisory on this repository,
not in a public issue. Say what an attacker can do, the steps, and the version.

## Threat model

**Trusted:** the application's own code (native types, `onCall`, `onAction`), its command line and environment, and
the user at the keyboard.

**Untrusted, and checked:**

| Input | Where it arrives | What guards it |
|---|---|---|
| Peer messages | `Screen::listen`, `Screen::connect` (`channel::Session`) | `parseUntrusted`; the message limit; every field read without throwing |
| Revisions and change sets | Peers, `submitRevision`, `importChanges`, `--plugin` | `parseUntrusted`; validation; permissions judged by effect; `Rules::checkChange` |
| Screen files and their `include`s | `Screen::fromFile`, `Screen::fromString` | `parseUntrusted`; validation; `include` at most 8 deep |
| The `persist` state file | `~/.affrescoui/<screen>.state.json` | `parseUntrusted`; an unreadable file is ignored |
| Messages on declared channels | A screen's `channels` | `parseUntrusted`; only actions the screen declares run |

**What a peer is.** Peers are programs of the same user on the same machine. The host listens on Unix sockets in a
directory that only the user can use: the examples refuse a socket directory that another user owns or that is not
a directory, and create it with mode 0700; each socket is made 0600. There is no other authentication, and
no network listener. The author of a change is set by the socket it came through ("studio", "agent"); a name the
peer gives itself is only appended after a `/`, so it can't become another author.

**What a revision may not do.** It may not change `permissions`, `channels` or `persist`, and it may not add or
change control steps (`accept`, `reject`, `preview`, `endPreview`, `restore`, `revert`, `undo`, `patch`, `call`,
`send`, `connect`). An author never approves its own proposal, and a peer's approval names the head it reviewed.

## Limits

| What | Limit | Where |
|---|---|---|
| JSON nesting (peers, revisions, change sets, screens, includes, the state file) | 256 levels | `core/Json.hpp`, `maxNesting` |
| One message on a channel | 64 MiB; a longer line closes the channel | `core/Channel.hpp`, `maxMessageBytes` |
| `include` inside `include` | 8 levels | `screen/src/Screen.cpp` |
| Styles using styles | 16 levels | `format/src/Runtime.cpp` |
| Components inside components | 32 levels | `format/src/RuntimeImpl.hpp` |
| Actions running actions | 32 levels | `format/src/RuntimeImpl.hpp`, `maxActionDepth` |
| `while` iterations, and `forEach` list length | 10,000 | `format/src/RuntimeImpl.hpp`, `maxLoopIterations` |
| `range` and `pad` | 10,000 | `format/src/Expression.cpp` |
| Expression nesting | 100 levels | `format/src/Expression.cpp` |
| Open proposals per channel | 16 | `vcs/Repository.hpp`, `maxOpenProposals` |

Log messages are escaped before they reach the log sink: control characters become `\n`, `\x1b` and so on, so text
from a peer or a document can't forge a log line or send escape sequences to a terminal.

## Known gaps

- **No total work budget.** Each loop is limited, but loops inside loops multiply, so a document or a revision can
  run many millions of steps in one event, set as many timers, or build deeply nested state. The application stops
  responding; nothing is corrupted.
- **Unbounded queues.** A peer that sends messages faster than the main thread handles them makes the queue of
  messages grow; each message is limited, the queue is not.
- **A screen destroyed while its window lives on.** Its timers and deferred tasks are cancelled, but its panels and
  the window's signals still call into it, so input delivered to that window afterwards is a use after free. Destroy
  a screen only when its window gets no more events (after `Application::run` returns).
- **Fonts.** A revision may change `fonts`, so it can make the host read any file the user can read as a font.
  Fonts are read with stb_truetype, which doesn't check font files against malicious input.
- **Paths in screen files.** `include` and font paths may be absolute or contain `..`: a screen file can read any
  file the user can read. Load screens you trust, or check their paths first.
- **Windows.** The socket channels are POSIX only; there is no Windows transport, so no Windows access control
  either.
