# Security

This file says what AffrescoUI protects against, how, and what it doesn't protect against yet. It describes the code
as it is, not as it is planned.

## Reporting a vulnerability

Please report vulnerabilities privately through GitHub's private security advisories for this repository: open the
**Security** tab and choose **Report a vulnerability** ([new advisory](../../security/advisories/new)). Don't open a
public issue, pull request or discussion for a vulnerability.

A useful report says which version or commit you tested, what an attacker controls (a peer, a revision, a screen file,
a change set), what they gain, and the smallest screen or message that shows it. You will get an answer in the
advisory. A fix is released, and the advisory published, once users can update.

## Supported versions

AffrescoUI is at 0.x. Only the latest 0.x release gets security fixes. Older releases don't. Fixes are made on
`master` and ship in the next release.

| Version | Supported |
|---|---|
| Latest 0.x release | Yes |
| Earlier releases | No |

## What AffrescoUI protects

AffrescoUI shows a screen document and lets other programs change it while it runs. Those programs are often AI
agents, and an agent's decisions can be wrong or steered by what it reads. The protections below are about **what a
change may do**: who may change which parts, who approves it, and what can never be changed from outside.

They are not a sandbox between processes. A peer is a program running as the same user (see "Who a peer is"), so it
could already read and write anything that user can. Version control stops the app from accepting a change nobody
approved. It does not stop a hostile program that runs as you from attacking you some other way.

## The trust model

### Trusted: the host and what it loads
The program that embeds AffrescoUI (the host) is trusted, and so is everything it loads on its own authority:
- the screen file and the files it `include`s;
- the state file that `persist` writes (in `~/.affrescoui/`);
- change sets passed to `affrescoui_host --plugin` (these still go through the deny rules and the protected sections
  below, but they are not reviewed);
- the native code it registers (`registerType`, `onAction`, `onCall`).

The screen file, its includes and the state file are read with the JSON nesting limit (256 levels), but none of the
other checks applied to peers. Don't load a screen file or change set you wouldn't run as a program. The state file
is the exception to "trusted": it holds the persisted parts of the live state, which peers can change, and it is read
back at the next start with only that nesting limit. Keep the paths a screen persists out of reach of agents you
don't trust.

The history of a screen opened from a file is kept next to the state file, in `<file name>.history/` (the data
folder: `~/.affrescoui`, `AFFRESCOUI_DATA_DIR` or `--data`; see VERSION_CONTROL.md, "History on disk"). It is read
back at the next start with the limits below, and every file in it must match its SHA-256, so damage is never
loaded. Hashes don't prove that a review made it, so a history is also checked as if written by hand:
- `main`'s head must pass the same check as a revision against the screen file (no new channels, `persist`,
  permissions or control steps);
- grants must be the narrow rules that **Always allow** gives;
- what each open proposal touches, and what **Always allow** would grant for it, are worked out again from its
  commits.

The folder is private to its owner, and a lock keeps a second host from writing it.

System fonts are read only when the host asks: with `AFFRESCOUI_SYSTEM_FONTS=1` in its environment (or a `FontCache`
given `FontCache::systemFonts()`), every font also falls back on a fixed list of the system's fonts for scripts and
emoji (`FontCache::systemFonts()` names them). A screen or a revision can't turn this on, nor name a folder to search.

### Who a peer is
A **peer** is a program connected to the running screen over a local socket. The host decides who each peer is:
- The host listens on one socket per role. The example host listens on `studio.sock` and `agent.sock` in its socket
  directory, so a program that connects to `agent.sock` is the author `agent`, whatever it says.
- The name a peer gives itself (`"author"` in a message) is only kept after a slash, for display: `agent/claude-code`.
  Any program on the same socket can give itself the same name, so rules and reviewers should name channels
  (`agent`), not full names.
- **The peer must run as the same user.** When a connection is accepted, the host asks the operating system which
  user the program on the other end runs as (`SO_PEERCRED` on Linux, `getpeereid` on macOS and the BSDs; on Windows,
  the peer's process id from `SIO_AF_UNIX_GETPEERPID`, whose token's user is compared). A program of another user,
  or one the system can't tell about, is refused, its connection closed, and the refusal logged. An embedding
  application can pass its own check to `SocketServer` (`PeerCheck`).
- The socket file is made readable and writable only by its owner, just after it is created; until then it has the
  process's default permissions. The example host, the Studio and the MCP server keep the sockets in a directory
  (`/tmp/affrescoui-<uid>` by default, or `--socket DIR`) that must be a real directory owned by the current user,
  not a symbolic link (only the last part of the path is checked); the host makes it accessible only to that user,
  and the Studio and the MCP server don't check its permissions. The directory and the file keep other users from
  connecting at all; the user check above refuses them if they get through.

### What a peer may send
Each peer is held to limits (`channel::PeerLimits`; the defaults are in "Limits" below), so one peer can't take the
app's time or memory from the others:
- a message larger than its limit is refused before it is parsed;
- messages of any kind, and changes (`revision`, `restore`, `revert`, `undo`, which rebuild the screen), each have a
  rate, kept by a token bucket: a burst after a quiet time, then a steady rate. A message over the rate gets an error
  and costs no more than that; the first refusal in a row is logged;
- what the host sends a socket peer is written by a thread of its own, never on the main thread. A peer that falls
  further behind in reading than the send queue's limit is disconnected, not buffered without end, and can't block
  the app.

### What every peer can read
Permissions govern changes, not reads. Every peer gets the whole document and the live state when it connects
(`hello`), hears about every commit and every change to the proposals, and can ask for the history, the open
proposals, the grants and diffs at any time. Don't put secrets in a screen's document or state.

### What a revision can and can't change
A revision is checked twice: once by what it does (the permission rules), and once against parts no revision may touch.

**What it can change**, subject to the permission rules: the layout, styles, colours, fonts, components, templates,
screens, actions, and the live state.

**What it can never change**, whoever sends it, including the app's own `patch` steps:
- `permissions`, the rules themselves;
- `channels`, the programs the screen talks to;
- `persist`, what the screen writes to disk;
- **control steps**: `accept`, `reject`, `preview`, `endPreview`, `restore`, `revert`, `undo` and `patch` (which
  control changes), and `call`, `send` and `connect` (which reach outside the screen). A revision can't add, remove,
  alter or move one to another trigger (a handler, a timer, a key, a `watch`, `onLoad`). Otherwise an agent could
  hide an "accept" inside a button and approve itself, or give the screen new access to the outside world.
- **ways to run a control step**: a revision can't add a `run`, a handler or a `do` that names an action holding
  control steps (found through the actions it runs in turn), an action the document doesn't define (the
  application's native actions), or a computed action name such as `{"run": "{{state.name}}"}`.

A revision also can't add an `include` (the section only exists while the file is loaded) or a new section the format
doesn't know.

**The permission rules** (`permissions`: `allow`, `ask`, `deny`) are judged by the change's effect, by comparing the
screen before and after, not by how the patch is written:
- a matching `deny` refuses it, for everyone, including reviewers and the app itself; deny rules are checked again
  when a proposal is merged;
- otherwise a matching `ask` sends it to review;
- otherwise it applies only if `allow` rules and grants cover everything it changes;
- anything else goes to review.

A node with `"locked": true` and an `id`, in `root`, `screens` or `templates`, is denied to everyone. Without an
`id`, or inside a component, `locked` protects nothing. A screen without a `permissions` section applies every
change and lets anyone review (the `sign-in` example is one): give a screen permissions before you let an agent
connect to it.

### Reviewers, and why an author can't approve its own change
A change that needs review becomes a **proposal**. Of the peers, only the authors listed in `permissions.reviewers`
may accept or reject proposals, preview them, change the mode or revoke grants. Review steps in the screen file
itself are not checked against that list: they act as `user`, for whoever uses the app. Several rules keep an author
from approving itself:
- a reviewer can't accept a proposal whose author is on the reviewer's own channel;
- a revision can't add, change or reach review controls (see above), so a proposal can't approve itself from inside
  the screen;
- the MCP server for agents has no accept tool, and the example host gives agents their own socket, which the
  templates don't list as a reviewer.

**Modes and grants** are set by reviewers. `acceptEdits` applies changes that only wait because no rule allows them
(`ask` and `deny` still hold). `plan` suspends the `allow` rules and grants of authors who aren't reviewers. "Always
allow" adds a grant for exactly what the accepted proposal touched, for its author's channel, until the host quits.
Grants are allow rules, so `ask` and `deny` still win.

### You accept what you saw
An author can keep pushing commits to its open proposal. Every approval must name the proposal's head commit (at
least its first 7 characters): the version the reviewer looked at. If the author pushed since, the approval is
refused and the proposal stays open for another look. An approval without a head is refused. This holds for all
three ways to accept: an `accept` message from a peer such as the Studio, the C++ call
`Screen::acceptProposal(id, head, error)`, and an `accept` step in the screen, which names it with `"head"` (for
example `"{{proposals[0].head}}"`).

### Previews
Previewing a proposal makes its document live before anyone approves it, timers and all. While a proposal is shown,
control steps run only from the user's own input (a click, a key), never from timers or from handlers that fire by
themselves (`onLoad`, `onShow`, `watch`). The screen's own timers that use control steps pause during the preview
and run again after it.

### What `onCall` and `onAction` expose
A host can register native code that documents use:
- `onCall(name, function)` makes `name` callable from `{"call": name, "with": ..., "into": "state.x"}` steps. The
  function gets the `with` value, which the screen computes, often from the live state or the event. Treat it as
  untrusted input: check it and limit it. Revisions can't add, alter or move `call` steps, or add a way to run
  one, so only calls the screen file already contains can reach it, from the triggers it gave them. But a revision
  can change the state its `with` reads, and add new uses of a component or template that holds it, each with its
  own parameters: the function can get any argument an author chooses.
- `onAction(name, action)` registers an action by name. A document reaches it wherever it names an action that the
  file doesn't define: `"onClick": "name"`, `{"run": "name"}`, or a `call` step. A revision can't add such a
  reference (see "What it can never change"), so native actions run only from the places the screen file gave them.
  Still, register as native actions only things the person using the app may trigger.

Whatever a native function or action does, it does with the host's full rights. Version control can't see inside it.

### Channels a screen declares
A screen's `channels` section names local sockets the screen connects to. Relative paths are resolved in
`AFFRESCOUI_SOCKET_DIR`, or `/tmp/affrescoui-<uid>`, or the directory the host sets. Every message that arrives
there runs the screen's `<name>:<type>` action, if it has one, with the message as its event, so the program at the
other end can run those actions with any data. Revisions can't change `channels` or add `send` and `connect` steps.

### Requests relayed between peers
`request` and `reply` messages aren't interpreted. The host passes them to every other peer, with `from` set to the
sender's author. A request is how a person asks an agent for a change, so an agent should treat it as instructions
from the channel `from` starts with (`studio` in `studio/riccardo`), and nothing more: the part after the slash is
the sender's own claim. Any peer can send one.

### What documents can't do
A document has no scripts. Expressions are evaluated by the engine, and steps are a fixed set. A document can't start
a process (the `ProcessChannel` and `StdioChannel` classes are only opened by host code), open a network connection,
or write files, apart from the parts of the state its `persist` section lists, which go to the state file the host
chose.

### The `affrescoui` tool and the MCP server
The command-line tool (`tools/affrescoui`) reads screen files people, agents and CI hand it:
- `check`, `fmt` and `ids` read a file as untrusted text: the JSON nesting limit, a text reader of their own that
  keeps to the same limit, and schema patterns matched only on texts up to the limit below (the `cli` fuzz target).
  `check` also loads the screen as the library does, `include`s included, and an `include` may name any file (an
  absolute path too): checking a file reads every JSON object file it names, and the checker's messages can quote
  parts of them. Check a file from someone you don't trust where that is fine, such as a CI runner.
- `run`, `render`, `import` and `export` treat the file as a host does: the screen file is trusted, and a change set
  given to `import` or `run --plugin` is trusted like one given to `affrescoui_host --plugin`. `import` refuses a
  history another program is writing.

The MCP server (`mcp/affrescoui_mcp.py`) is started by the agent's client and talks to the host over `agent.sock`, so
the host makes it the author `agent` whatever name the client gives. It only passes on what the agent asks for:
it has no tool to approve, to read or write files other than the format guide, or to run a program (`--launch`, a
command on its own command line, is the only one). A message from either side that it can't use gets an error and
costs only that message. What it hands the agent (a person's request, the document, events) was written by peers
and people: an agent should treat it as data, and a request as instructions from the channel it came from (see
"Requests relayed between peers").

## Limits

Input from peers, revisions and change sets is limited so that one message can't exhaust the stack or loop forever:

| What | Limit |
|---|---|
| JSON nesting of one message from a peer, a revision, a change set or a declared channel, and of a screen file, an include or the state file | 256 levels |
| The document, with its live state one level inside it, as revisions build it (a trusted screen file may be deeper; then a revision may not make it deeper still) | 200 levels |
| The live state, as steps build it: the place a `set`, `push` or `call` `into` writes, and the value written there | 200 levels |
| One message on a channel (one line) | 64 MiB; a longer line closes the channel |
| One message from a peer (`PeerLimits::maxMessageBytes`) | 8 MiB; a larger one gets an error, unread |
| Messages from one peer, of any kind | 100 a second, 200 at once |
| Changes from one peer (`revision`, `restore`, `revert`, `undo`) | 120 a minute, 30 at once |
| Sent to one socket peer and not yet read by it | 64 MiB; then the peer is disconnected |
| Nesting inside one expression (`{{ ... }}`) | 100 levels |
| `while` loop iterations | 10,000 |
| Items in a `forEach` list | 10,000 |
| `range(n)` | n up to 10,000 |
| `pad(text, width)` | width up to 10,000 |
| Step lists inside step lists and actions (`if`, `forEach`, `while`, `run`, named handlers) | 32 levels |
| Components using components | 32 levels |
| Styles using styles | 17 levels |
| Files including files (`include`, at load time) | 9 levels below the screen file |
| Open proposals per channel | 16 |
| A commit named by its id or a prefix of it | 4 to 64 hexadecimal characters |
| A proposal named by `proposal/<n>` | n up to 1,000,000 |
| An approval's head commit | at least 7 characters |
| Undo steps kept by an `editor` | 500 |
| A font's glyph atlas (a larger font size is refused) | Pages of 4,096 × 4,096 pixels at most, 64 MiB in all (colour pages count four bytes a pixel) |
| A font file (regular files only: no devices or pipes) | 256 MiB |
| Fonts kept that no screen, panel or window uses (`FontCache`, least recently used dropped first) | 32 fonts and 128 MiB |
| Shaped text kept per font (least recently used dropped first) | 4,096 lines of 128 Ki code points in all, 8,192 words of 64 Ki |
| A file of a history on disk, read at start (nothing larger or deeper is written) | 64 MiB, and 256 levels of JSON |
| A pack of a history's objects, written | 16 MiB (more objects go into another pack) |
| A text or key `affrescoui check` matches against a schema pattern (`std::regex` recurses on each character: a longer text is reported as too long to check, a longer key matches no pattern) | 10,000 characters |

Log messages are escaped before they reach the log sink: control characters become `\n`, `\x1b` and so on, the C1
controls (U+0080 to U+009F) `\u0080` to `\u009f`, and a backslash `\\`. So text from a peer or a document can't forge
a log line or send escape sequences to a terminal, and an escape in the log always stands for a control character,
never for text that looks like one.

## CI and releases

The workflows in `.github/workflows/` run a pull request's code as untrusted:
- they run on `pull_request`, never `pull_request_target`, with a read-only token and no secrets, and the checkout
  keeps no credentials;
- every action is pinned by its commit (Dependabot proposes updates), and every Python tool CI installs is pinned by
  hash (`--require-hashes`); the C++ dependencies are fetched by URL and SHA-256;
- the fuzz corpus in the Actions cache is written only by the nightly runs, never by a pull request;
- the labels that accept a change in pixels (`visual-change`) or speed (`perf-change`) are set by the maintainer.

The quality gate reads its rules (`.clang-tidy`, the thresholds, the coverage floor) from the base, so an honest
change can't loosen what judges it. A pull request can still change the workflows and the scripts that run them
(`.github/`, `tools/quality/`, `tests/perf/`, `tests/render/`, `tests/fuzz/`), and its checks then run its own
version: a green check on such a pull request says nothing until someone has read that part of it.

A release is made only from a tag `v*`, which only the maintainer can push (RELEASING.md). It builds in a fresh run,
with no cache, and makes a draft: the archives, `SHA256SUMS` and build-provenance attestations signed through the
run's identity. The MCP package goes to PyPI by trusted publishing, from a protected environment the maintainer
approves.

## Known gaps

These are not protected today. They are stated plainly so that you can decide what to expose.

- **Who is at the other end of a declared channel isn't checked.** The user check applies to connections a host
  accepts (`listen`, `SocketServer`). A screen's `channels` connect out to a socket, and the library doesn't check
  who listens there, nor the directory of that socket or of one an embedding application passes to `listen`.
- **The user check on Windows goes by process id.** The peer's process could exit and its id be reused between the
  connection and the check, a narrow race. A process of the same user that runs elevated may not be inspectable from
  a non-elevated host, and is then refused.
- **The rate limits are per peer, and the queue to the main thread isn't bounded.** Nothing limits how many peers
  connect, and each new connection gets fresh limits. A socket's reader thread hands every line to the main thread
  as it arrives, so a peer that sends faster than the main thread refuses its messages still makes that queue grow;
  refusing is cheap (no parsing), so the main thread keeps up in practice. The open-proposal limit caps waiting
  proposals, and the change rate caps how often they are pushed.
- **Large revisions.** A message can be up to 8 MiB, so a revision can still be large, and so can the lists it puts
  in the state, which the screen may render as one panel per item. A process or stdio channel the host opens isn't a
  peer: it isn't held to these limits, and the host writes to it on the main thread.
- **There is no total work budget per event, so nested loops multiply.** Each limit above holds on its own:
  a `while` of 10,000 iterations inside a `forEach` over a list of 10,000 items runs 100 million steps, and deeper
  nesting multiplies again. `after` and `debounce` steps schedule more work for later, and nothing caps how many are
  waiting. A revision that adds steps can cause this. Steps live in `/actions` and also inline: in event handlers on
  nodes, components and templates, and in `onLoad`, `watch` and `keys`. An `ask` rule on `/actions` alone doesn't
  cover them; only allow agents the parts of a screen that hold no steps (the templates allow agents nothing, so
  every change of theirs is reviewed).
- **A revision can change the state control steps read.** It can't add, move or newly trigger a control step, but
  it can change the state their expressions read. A screen with its own `accept`, `restore`, `call` or `send` steps
  should deny agents the `/state` paths those steps read.
- **A revision can point a font at any file.** `fonts.<name>.file` can be changed by a revision, and the app then
  reads that file, if it is a regular file of at most 256 MiB, and parses it as a font; the error a peer gets back
  says whether it could be read. The parsers are FreeType and HarfBuzz, which OSS-Fuzz fuzzes all the time, and far
  more hardened than stb_truetype was; still, a malformed font is untrusted input to C and C++ libraries, and the
  `text` fuzz target here fuzzes the text, not the font files. Fonts no screen uses are bounded (the limits above),
  but the fonts the current document declares are all kept, however many it declares: only the document's size
  bounds them, and each costs at least its first atlas page (up to 16 MiB for very large sizes).
- **A history on disk is only as safe as its folder.** Anyone who can write the data folder can write a history
  whose hashes are valid. The checks above hold, but deny rules aren't replayed for each commit on `main`, so such a
  history can contain changes the deny rules would have refused, as long as its head passes the revision check. This
  is the same trust as the state file's. A screen loaded from a string (`fromString`) keeps its history in memory
  only, and it is lost when the host quits.
- **A commit's delta isn't covered by its id** (it says how the commit is kept, not what it is). The document deltas
  rebuild is checked against the commit's tree whenever it is rebuilt, and `main`'s head when a history loads, so a
  damaged delta stops that rebuild with an error; but a delta written by hand that rebuilds the right document can't
  be told from one the program wrote, which is the trust above.
- **Windows is new.** The socket channels run on Windows through Winsock's `AF_UNIX`, with an owner-only access list
  on each socket file and the peer's user checked through its process. CI builds it and runs its unit tests, but
  that job may fail without failing CI, and its window tests, rendering check and fuzzing don't run there, so it is
  far less tested than macOS and Linux.
- **The MCP server keeps what waits for the agent without a limit.** The news for `get_events` and the requests not
  yet answered stay in its memory until the agent reads or answers them. The host's limits slow how fast peers can
  add to them, not how much piles up over a long session with an agent that never asks.
