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

The folder (`0700`) and every file in it (`0600`, never written through a symbolic link) are private to their owner,
and a lock keeps a second host from writing it. A history folder that is a symbolic link, or on POSIX belongs to
another user (as one in a shared data folder can), is neither read nor written: the history stays in memory, and a
warning says why.

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

### Remote peers
A host may also accept peers over WebSocket (`Screen::listenWebSocket`, `net::PeerServer`), from other machines:
- **Every peer brings a token**, as `Authorization: Bearer <token>` on the upgrade request, or as a first message
  `{"type": "auth", "token": "<token>"}` within 5 seconds. The host's `TokenVerifier` checks it and names the author
  the peer's changes are made as; the token is never kept, logged or passed on. A peer with no token, or one the
  verifier refuses, is closed with a reason (401 on the upgrade, or close code 1008), and the refusal is logged with
  its address, never with the token. There is no listening socket without authentication.
- **Browsers are held to an allowlist of origins.** A request with an `Origin` header (every browser sends one) is
  refused with 403 unless the origin is in `PeerServerOptions::allowedOrigins`; the list is empty by default, so no
  web page can connect. Programs that send no `Origin` are judged by their token alone.
- **On the loopback by default.** The server refuses to listen on any other address unless `allowRemote` is set. It
  has no TLS code of its own: put a TLS proxy in front of it (`examples/deploy/Caddyfile`) and keep it on the loopback,
  so tokens never cross the network in the clear.
- **Connections are capped**: how many peers are connected or opening at once, and how fast one address may open
  connections (a token bucket per address); over either, the connection is refused (503 or 429) before it is read.
  Behind a proxy every connection comes from the proxy's address, so the rate is shared by all its clients.
- Once accepted, a remote peer is held to the same `PeerLimits` and permission rules as any other peer.
- `net::connectPeer` connects as a remote peer: to `wss://` (TLS verified), or to `ws://` on the loopback only.

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

A peer can also ask for a picture of the screen (`render`), as it is or with any open proposal applied. A picture
shows what the window would show, which can be more than the document and the state hold: the text in an `editor` or
a `textarea` that isn't bound to the state, and whatever the host's own node types draw. A host whose screens show
anything peers shouldn't see gives `Session` no `ScreenImages` (a `nullptr`), and the peers' `render` is refused.

### What a revision can and can't change
A revision is checked twice: once by what it does (the permission rules), and once against parts no revision may touch.

**What it can change**, subject to the permission rules: the layout, styles, colours, fonts, components, templates,
screens, actions, and the live state.

**What it can never change**, whoever sends it, including the app's own `patch` steps:
- `permissions`, the rules themselves;
- `channels`, the programs the screen talks to;
- `persist`, what the screen writes to disk;
- `services`, the local programs and folders the screen uses (and the host has granted);
- **control steps**: `accept`, `reject`, `preview`, `endPreview`, `restore`, `revert`, `undo` and `patch` (which
  control changes), and `call`, `send` and `connect` (which reach outside the screen). A revision can't add, remove,
  alter or move one to another trigger (a handler, a timer, a key, a `watch`, `onLoad`). Otherwise an agent could
  hide an "accept" inside a button and approve itself, or give the screen new access to the outside world.
- **ways to run a control step**: a revision can't add a `run`, a handler or a `do` that names an action holding
  control steps (found through the actions it runs in turn), an action the document doesn't define (the
  application's native actions), or a computed action name such as `{"run": "{{state.name}}"}`. Nor can it add a
  use of a component that runs the actions its parameters name with computed parameters: the built-in `menu`,
  `submenu`, `contextMenu`, `menuBar`, `dialog` and `toast` fed their items, buttons or actions from the state, a
  `quickPick` at all (its recent items are kept in the state), or the document's own component holding a computed
  `run`. A use of the document's own component (which wins over a built-in of the same name) is scanned with its
  defaults and literal parameters in place: `{"use": "go", "with": {"act": "approve"}}` is refused when `go` runs
  `"{{act}}"` and `approve` holds a control step, as is one expanded through more than 64 nested uses.
  application's native actions), or a computed action name such as `{"run": "{{state.name}}"}`.
- **service steps out of bounds**: `respond` is a control step. An `invoke` or `subscribe` a revision adds must name
  a declared service and a method of it the provider lets revisions use, by a fixed name; and when the method writes
  or runs something it is always reviewed: the change becomes a proposal whatever the allow rules, grants or mode
  say, and a person must approve it.

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

### Review policies, rebases and expiring grants
- **Policies** (`permissions.review`) are part of the screen's permissions, which no revision can change. An approval
  counts once per reviewer and never from the author's own channel; the policy is read from `main` when each approval
  arrives, and applied to the change as it would merge then.
- **A rebase can't bypass a policy.** A proposal rebased onto a newer `main` keeps its approvals only if replaying its
  commits gives exactly the three-way merge, that is, the changes that were reviewed. Otherwise its approvals are
  dismissed, and approvals that name an older head are refused. A proposal with conflicts can't be accepted.
- **A merge never applies more than was reviewed.** What merges is the proposal's reviewed changes on top of `main`;
  deny rules are checked again at the merge.
- **Grants can expire** (`"for"` on an accept, capped by `grantsLast`). An expired grant no longer allows; it stays in
  the history until revoked.
- **Evidence is the host's.** A proposal's pictures, changed region, behaviour lines, scenario results and changed
  texts are gathered by the host when a reviewer asks (`getEvidence`), from the proposal as it is then; a revision
  carrying `evidence` or `behaviour` is refused. The scenarios are played headless, on a screen connected to nobody,
  and gathering is rate limited like `render`.
- **The inspector shows no secret state.** It is off by default and only a peer that may review turns it on
  (`inspect`). It shows bindings as written, never their values; its trace names keys that type nothing and the
  channel of a send, never a typed character or a message; and it is hidden while a picture is drawn for a peer.

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

### Connections a screen declares

A screen's `connections` section names servers and the operations the screen may ask them (RFC 0003). It is a
capability, like `services`: a revision can't change it, and may add `request` and `subscribe` steps only for
operations it declares, written out (a computed name is refused). A step a revision adds that writes or runs
(`effect` other than `read`) is always reviewed. The host allows each origin first (`Screen::network().allow`); until
then every request is refused. A document never holds a credential: the headers that carry one are refused, and sign-in
sets them (next section). While a proposal is previewed, only a read without
a `mock` reaches a server; any other operation answers from its mock, or fails with `preview`, so a preview has no
side effects.

### Sign-in

A connection's `auth` declares an OAuth 2 / OpenID Connect provider for a public client (no client secret: none is
ever in a document or in this repository), or names another connection's. The host signs in when the person runs a
`signIn` step: the authorization code with PKCE (S256) and a redirect to a listener bound to 127.0.0.1 on a random
port, matched exactly and closed after one answer; or the device flow. The `state` and the OpenID `nonce` are
checked, and compared in constant time; every answer from the provider is bounded in size and time. The tokens stay
in the host's `SignIn` object, and the refresh token in its `SecretStore` (the Keychain, the Credential Manager or the
Secret Service when the application passes `net::systemSecretStore`; in memory by default). The host adds the
`Authorization` header to each request of a connection that signs in, after the document's headers, which can't
carry one. What a document sees is `auth.<name>`: a state, an account and an expiry, never a token, so a token
reaches no document, state, history, log or screenshot (`tests/unit/auth.cpp` signs in through a mock provider and
looks for every token it issued in all of them). A 401 refreshes once; a refused refresh signs out and erases the
stored token. `signIn` and `signOut` are control steps: a revision can't add or move them, nor change a provider, and
they don't run while a proposal is previewed. The id token is read for its claims (account, expiry) from the token
endpoint over TLS, not verified as a signature: it decides what is shown, never what is allowed.

### The document server

`affrescoui_server` (examples/server) accepts no peer without a token: its `TokenFile` keeps each token's SHA-256 and
a name, so the file reveals no token; a token is 256 bits from the system's random source, shown once when added, and
checked by hashing it and comparing it with every entry in constant time. It listens on the loopback unless told
otherwise, and is meant to sit behind a proxy that terminates TLS (examples/deploy). The token's name is the author
of every revision, so the document's permissions apply per person; a person's own documents (`~name`) are found by
that name, never by a path the peer sends. A lease is a deny rule for everyone but its holder, checked with the
document's own rules; it grants its holder nothing more. Presence is what a peer says about itself: shown, bounded,
never used to decide anything. Leases, presence entries and the server's messages are all bounded (examples/server). With `--userinfo` it also admits OAuth access
tokens: each is checked at the provider's https userinfo endpoint, and only the subjects of the people file the
operator writes get in, by the name that file gives them; a token that worked is remembered by its hash for five
minutes (a revoked token keeps working that long at most), a refused one isn't, and each question is bounded in time
and size. The server never keeps, logs or forwards a token anywhere but to that endpoint.

### Watched folders and terminals

The `watch` provider and `DirectoryModel` see only a folder the host granted, like `fs`: paths are reported relative
to it, and symbolic links are listed but never followed or watched, so nothing outside it is reported. What they
send is bounded: at most 1,000 paths a batch (more becomes one `rescan`), one batch per debounce interval (20 ms at
least), 100,000 watched folders, 200,000 rows, 4 subscriptions per service. Ignore globs only narrow what is reported.

The `pty` provider starts only a program the host allowed (`allowProcess`, as for `process`), from an argument vector:
a shell runs only if the host allows a shell as the program. The terminal's output reaches the screen as text, and
it is bounded: 8 terminals per service, 64 KiB a write and 1 MiB waiting, 16 KiB an event and 4 MiB a second. Escape
sequences in that text are the terminal widget's to interpret; the provider acts on none of them.

Forge's terminal widget (`examples/forge/features/terminal`, on libvterm) keeps every escape sequence inside itself:
a program can't set the window title unless the node's `allowTitle` is true (and then only the session's, at most
256 bytes), clipboard writes (OSC 52) and the bell are dropped, and its replies to queries go back to the program
only. Its scrollback is bounded (10,000 lines at most). Forge grants the user's login shell, started in the open
folder; the screen document only names the service.

### Channels a screen declares
A screen's `channels` section names local sockets the screen connects to. Relative paths are resolved in
`AFFRESCOUI_SOCKET_DIR`, or `/tmp/affrescoui-<uid>`, or the directory the host sets; an absolute path is used as it
is, so a screen file can name any socket, a running host's `studio.sock` too, and connects to it as soon as its window
opens. Every message that arrives
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

### Features and catalogues
A feature package or a catalogue's index (RFC 0004) is untrusted input, read like a screen file: size limits (an
index or a manifest over its limit is refused before it is parsed), the JSON nesting limit, and keys and digests
parsed at their fixed lengths and compared in constant time. Each is signed with Ed25519 (Monocypher) over its
canonical JSON (RFC 8785); its releases are pinned by SHA-512 digest and size. The host trusts no publisher until its
code calls `Features::trustPublisher`; a catalogue installs only if its index and each manifest are signed by trusted
keys, the manifest by the very publisher the index names, and an upgrade never changes the publisher. A release's
URL is a path inside the catalogue's folder. Whatever is installed is a revision under the document's
`permissions`, and a feature can't bring a capability (`channels`, `services`, `connections`, `persist`) or a
control step, so a feature never widens what the app can do. `affrescoui sign --new-key` writes a key only its owner
can read and never over an existing file; keep keys out of repositories (Forge's example catalogue was signed by a
key that was then deleted).

### The `affrescoui` tool and the MCP server
The command-line tool (`tools/affrescoui`) reads screen files people, agents and CI hand it:
- `check`, `fmt` and `ids` read a file as untrusted text: the JSON nesting limit, a text reader of their own that
  keeps to the same limit, and schema patterns matched only on texts up to the limit below (the `cli` fuzz target).
  `check` also loads the screen as the library does, `include`s included, and an `include` may name any file (an
  absolute path too): checking a file reads every JSON object file it names, and the checker's messages can quote
  parts of them. Check a file from someone you don't trust where that is fine, such as a CI runner.
- `render` draws the first frame offscreen without the history or saved state, and connects to nobody: every channel
  the screen declares, by an absolute path too, is pointed at a folder that is never created. A frame over 16,384
  pixels a side is refused before anything is drawn.
- `test` plays a scenarios file the same way: every channel the screen declares, by an absolute path or from an
  `include` too, is pointed at a folder that is never created, so a played screen never reaches a running host.
- `run`, `import` and `export` treat the file as a host does: the screen file is trusted, and a change set given to
  `import` or `run --plugin` is trusted like one given to `affrescoui_host --plugin`. `import` refuses a history
  another program is writing.

The MCP server (`mcp/affrescoui_mcp.py`) is started by the agent's client and talks to the host over `agent.sock`, so
the host makes it the author `agent` whatever name the client gives. It only passes on what the agent asks for:
it has no tool to approve, or to write files, and reads only the format guide and scenarios files: `run_scenarios`
reads only files named `*.scenarios.json`, and runs the `affrescoui` tool (`test --json`) as a list of arguments,
with a timeout of 300 s. It runs no other program but `--launch`, a command on its own command line. A message from either side that it can't use gets an error and
costs only that message, and its log escapes what isn't printable, as the host's does. What it hands the agent (a person's request, the document, events) was written by peers
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
| Pictures for one peer (`render`): each draws the whole screen, and builds a proposal's and then main's again | 120 a minute, 30 at once, apart from the changes |
| Sent to one socket peer and not yet read by it | 64 MiB; then the peer is disconnected |
| Nesting inside one expression (`{{ ... }}`) | 100 levels |
| `while` loop iterations | 10,000 |
| Items in a `forEach` list | 10,000 |
| `range(n)` | n up to 10,000 |
| `pad(text, width)` | width up to 10,000 |
| Step lists inside step lists and actions (`if`, `forEach`, `while`, `run`, named handlers) | 32 levels |
| Components using components | 32 levels |
| Computed values reading computed values | 32 levels |
| Panels built inside each other (components and outlets included; a component or a screen inside itself stops here) | 256 levels |
| A step writing to a list position past the list's end (the positions between become `null`) | 10,000 positions past it |
| Hints ("did you mean ...") the checker gives in one pass, each comparing the name with the first names that exist | 50 findings, 1,000 names |
| Styles using styles | 17 levels |
| Files including files (`include`, at load time) | 9 levels below the screen file |
| Open proposals per channel | 16 |
| Operations in one revision (its compact edits and its JSON Patch), or in one proposal with all its commits; counted before any is applied | 10,000 |
| Values the `copy` operations of one revision copy, in all (a copy of a list into itself doubles it) | 100,000 |
| Members a revision may give one object (a screen file may hold a wider one: a revision may change it, not widen it) | 10,000 |
| A grid's column or row tracks (`columns`, `rows`), and the column, row or span a child's `cell` names | 1,000 (a document asking for more is refused; columns and rows a layout pass makes stop there). Placing a child looks at each of the grid's cells about once |
| A commit named by its id or a prefix of it | 4 to 64 hexadecimal characters |
| A proposal named by `proposal/<n>` | n up to 1,000,000 |
| An approval's head commit | at least 7 characters |
| Undo steps kept by an `editor` (each copies the whole text) | 500, and 256 MiB in all: the oldest go first |
| A font's size (a document can ask for any: a larger one is refused) | 4,096 pixels |
| A font's glyph atlas (a larger font size is refused) | Pages of 4,096 × 4,096 pixels at most, 64 MiB in all (colour pages count four bytes a pixel) |
| A font file (regular files only: no devices or pipes) | 256 MiB |
| An image file (`image` nodes; regular files only, inside the screen's folder or one the host allows) | 64 MiB |
| A decoded image (from its header, before any pixel is decoded) | 8,192 pixels a side, 4,096 × 4,096 pixels in all |
| What a PNG inflates to while it is decoded (from its header) | Twice its compressed bytes, plus twice its rows at 16 bits and 4 channels, plus 1 MiB; stb_image's buffers can't grow past it, so a small file can't inflate to gigabytes |
| A font's GSUB and GPOS lookups, read by the plain-text fast path | A million rules, subtables, lookup records and glyphs; past it, a lookup is treated as unbounded and its text is shaped by HarfBuzz |
| Image files waiting to be decoded | 256; more show nothing |
| Decoded image files kept that nothing shows (least recently used dropped first) | 256 MiB |
| A glyph name an `icon` looks up | 128 bytes; 1,024 names remembered per font |
| Fonts kept that no screen, panel or window uses (`FontCache`, least recently used dropped first) | 32 fonts and 128 MiB |
| Shaped text kept per font (least recently used dropped first) | 4,096 lines of 128 Ki code points in all, 8,192 words of 64 Ki |
| A file of a history on disk, read at start (nothing larger or deeper is written) | 64 MiB, and 256 levels of JSON |
| A pack of a history's objects, written | 16 MiB (more objects go into another pack) |
| A text or key `affrescoui check` matches against a schema pattern (`std::regex` recurses on each character: a longer text is reported as too long to check, a longer key matches no pattern) | 10,000 characters |
| The work of one event (a handler, an action run by code, a timer, `onLoad`, `watch`, a drag's `data`, the state path a `caret`, a `bind` or an overlay writes) or one update, nested handlers sharing the outermost one's (`WorkBudget`, `Expression.hpp`) | 100,000 steps (each `forEach` item and `while` turn is one, even with nothing to `do`); 10,000,000 expression operations (each item a list function visits counts); 64 MiB written into the state by `set` and `push`. Past one, what is running stops with an error naming its place; an update that stops refuses the version that caused it |
| One revision (`Rules::checkRevision`, before any of it is applied) | 100,000 operations, and 16 MiB as JSON |
| A document, without its state (`Rules::validate`, at load and after every revision) | 32 MiB as JSON |
| The state: the document's own, and the live state after a revision (unless it was already larger) | 32 MiB as JSON |
| A scenarios file (`affrescoui test`, `run_scenarios`; an agent can write one) | 1,000 scenarios, 10,000 steps each, 100,000 bytes a `type`, a `wait` of 10 minutes of the scenario's clock and 1,000,000 timer runs in one, 100 moves a `drag`, a `resize` side of 16,384; names of letters, digits, `-`, `_` and `.` (they become file names) |
| A golden image read by an image expectation | An 8-bit RGB or RGBA PNG, not interlaced, of 32 Mi pixels at most; anything else is refused |
| Design tokens (`tokens`) | 4,096 tokens, 32 themes, aliases 16 deep (a circle is an error); more is refused |
| Sizes and styles of fonts one document derives (rich text runs) | 32 |
| One label's rich text (`runs`) | 256 runs, 64 Ki code points in all, 2,048 bytes a link |
| Items a list, a tree or a table reads | 1,000,000 items, trees 256 deep, 256 cells an item, 256 columns of 10,000 pixels at most, 16 sticky folders |
| Tabs a strip reads, panes a split reads | 10,000 tabs and 1,000 panes (more are ignored); a split's sizes 0 to 1,000,000 pixels |
| A menu inside menus (`menus`, contributed items) | 8 levels: deeper submenus are left out |
| Slow calls (`Screen::onAsyncCall`) running or waiting for a worker at once | 64; more end with an error (`onError`) |
| A vector path (`path` nodes and SVG files) | Coordinates up to 1,000,000 either sign (a contour scaled past 10^12 on screen isn't drawn), 64 dash and gap lengths, 65,536 points flattened, 65,536 quads and 16 Mi edge visits a fill or stroke makes (past them, the rest isn't drawn) |
| A response body, a server-sent event or line of a stream, or a WebSocket message the `net` module receives (`Request::maxBodyBytes`) | 8 MiB by default; a larger one fails with `tooLarge` (a WebSocket is closed with 1009) |
| A response's headers, or a WebSocket handshake's answer | 64 KiB and 256 lines |
| Headers a request adds (`checkHeaders`) | 100, 64 KiB in all; no line breaks or NUL, none the transport sets |
| A URL the `net` module reaches | 8,192 bytes; https and wss to an allowed origin, http and ws to the loopback only; no user info |
| Requests in flight to one origin, and started and not finished in all | 6 (more wait), 256 (more fail with `refused`) |
| Bytes a WebSocket holds unsent, and events or messages posted to the main thread and not yet handled | 16 MiB (`send` refuses more), 4,096 (more fail with `tooLarge`) |
| Remote peers (`PeerServerOptions`) connected or opening at once | 32; more are refused with 503 |
| Connections one address may open | 5 a second, 10 at once; more are refused with 429. 4,096 addresses are tracked (past that, those back to a full burst are forgotten) |
| A remote peer's token | 5 s to arrive (as a first message); 8 KiB at most |
| A remote peer's upgrade request | 16 KiB and 64 header lines; a larger one is refused with 431 |
| One message from a remote peer, and what waits to be written to it | 8 MiB a message (a larger one closes it with 1009); 64 MiB queued, or a write stalled for 10 s, closes it |
| Where a peer server listens | The loopback, unless `allowRemote` is set |
| Redirects a request follows | 5, within its origin only |
| Retries of a request | 5, GET only, after a network error, 429, 502, 503 or 504; a server's `Retry-After` up to an hour |
| TLS | 1.2 or later; the certificate and the host name are always verified (no option turns it off) |
| What a service program writes (`services`: a JSON-RPC frame or line) | Headers 8 KiB, a body or a line 8 MiB; a larger one stops the service. An error message from it is cut to 1,000 bytes |
| A JSON-RPC service | 256 calls waiting for an answer, 256 subscriptions, 16 MiB waiting to be written to it |
| A `process` service | 16 runs at once; its input 1 MiB; a line of its output 64 KiB (a longer one arrives in pieces) |
| An `fs` service | 16 calls at once; a file read or written 8 MiB, UTF-8 text only; a listing 10,000 entries; paths that resolve inside its folder only |
| Service calls of one screen (`invoke`, `subscribe`, `respond`) | 1,024 calls running, 256 subscriptions, 256 requests from services waiting for `respond`; a call's `keep` and the events it queues 10,000 |
| A screen's `connections` | 64 connections, 256 operations and messages each; a `timeout` of 10 minutes at most (30 s by default), `retry.times` 5 at most |
| Requests one connection starts (`request`, `subscribe`) | 50 a second; more fail with `refused`. Each also has the `net` module's limits above |
| Headers a connection or an operation declares | Names that are HTTP tokens of 64 bytes at most; never `Authorization`, `Proxy-Authorization`, `Cookie`, `Host`, `Content-Length` or `Transfer-Encoding` (credentials come from sign-in, never from the document) |
| Usage summaries (`affrescoui_host --usage`, VERSION_CONTROL.md "Usage"): privacy | Off by default; on only with the host's setting and the person's one-time consent, asked on the terminal (no terminal, no consent). Node ids, action names, screen names, counts and times only: never text, values, the state or pictures. Ids as the document writes them, never evaluated |
| Usage summaries: where they are kept and who reads them | On the device only, in `<data folder>/<screen>.usage.json` beside the "persist" state; nothing is sent anywhere. Only a peer that may review reads (`getUsage`, MCP `get_usage`) or clears (`clearUsage`) them, and only while they are on: an agent can't grant itself that |
| Usage summaries: bounds | 1,024 node and action pairs, 64 screens, 1,024 controls listed as never used; names of 1 to 64 printable bytes (others aren't counted); counts stop at 10^9; counts not made for 30 days are dropped; a file over 1 MiB isn't read |

A revision's operations are applied before any permission rule judges them, so the limits on revisions above
hold for every peer, whatever the rules allow it. JSON from outside the app (`parseUntrusted`) is read in time in
proportion to its text, however many keys an object has, unless its keys are chosen to collide (Known gaps). A
document's `settings` section is checked the same way, at load and after every revision: a setting that is also a
group of settings (`editor` and `editor.fontSize`) is found in one pass over the keys, so a 32 MiB document of small
settings takes a fraction of a second, not minutes.

A document's numbers are floats of any size, and a size, an index or a step count computed from one is clamped before
it becomes an integer, since a float past the integer's range makes the conversion undefined behaviour. A `tabIndex`
of `1e30` is the last in the tab order, a list `1e30` pixels tall pages to its last row, and a curve under a view box
a millionth of a pixel wide is drawn with at most 64 lines a bend. A call's `timeout` is cancelled when the call ends
first, so many short calls with long timeouts leave no timers behind.

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

- **A signed-in token is as safe as the process.** Anything running in the host process can read the tokens in
  memory, and anything the person runs can ask their secret store for the refresh token (the system may ask them
  first). The browser flow's listener accepts the first answer that carries the right `state`: another program of the
  same user that learns the authorization URL could race it, though PKCE keeps the code useless without the verifier.

- **Services aren't sandboxed.** A program the host allows (`allowProcess`) runs with the user's rights, and a
  JSON-RPC service may do whatever its program does once started; `fs` keeps paths inside its folder, but a program
  writing to that folder at the same time could swap a folder for a link between the check and the use. Grant only
  programs and folders you trust the screen with.
- **Who is at the other end of a declared channel isn't checked.** The user check applies to connections a host
  accepts (`listen`, `SocketServer`). A screen's `channels` connect out to a socket, and the library doesn't check
  who listens there, nor the directory of that socket or of one an embedding application passes to `listen`.
- **The user check on Windows goes by process id.** The peer's process could exit and its id be reused between the
  connection and the check, a narrow race. A process of the same user that runs elevated may not be inspectable from
  a non-elevated host, and is then refused.
- **The rate limits are per peer, and the queue to the main thread isn't bounded.** Over local sockets nothing
  limits how many peers connect, and each new connection gets fresh limits (remote peers over WebSocket are capped in
  number and in connection rate, "Limits" above). A socket's reader thread hands every line to the main thread
  as it arrives, so a peer that sends faster than the main thread refuses its messages still makes that queue grow;
  refusing is cheap (no parsing), so the main thread keeps up in practice. The open-proposal limit caps waiting
  proposals, and the change rate caps how often they are pushed. Each connection also takes a file descriptor and two
  threads: a peer that opens enough connections to use up the descriptors makes the server's accepting thread retry
  without pause until one is closed, and one that uses up the threads stops the app (a thread that can't be started
  ends the process).
- **Large revisions.** A message can be up to 8 MiB, so a revision can still be large, and so can the lists it puts
  in the state, which the screen may render as one panel per item. A long text costs in proportion to its length,
  but a line longer than a font keeps shaped (128 Ki code points) is shaped again every time it is drawn or measured:
  a label of a million characters on one line takes a fraction of a second each time. The limits above bound what
  one revision costs, not to nothing: 10,000 operations that each add to an object of up to 10,000 members took
  about 0.4 s to prepare (Release, Apple silicon), a revision that waits for review is prepared twice, and a peer may
  send 30 changes at once. Reads have only the message rate: `diff` looks up each key of an object among the
  other's, so diffing an object takes time in proportion to the square of its members. A process or stdio channel
  the host opens isn't a peer: it isn't held to these limits, and the host writes to it on the main thread.
- **A revision of many operations costs operations times the document.** Each operation is applied to the whole
  document as it stands: a `#id` path is looked up, and an operation that adds a node without an id collects every id
  of the document to name it. A revision of 8,000 operations that each add a node without an id takes about 6.6
  seconds on the main thread; give nodes their ids, or add them in one operation.
- **Nested components and repeats multiply.** Each use builds its component's node: a component that uses another
  twice, 25 levels down, builds 2^25 panels, and a `repeat` over 10,000 items inside another builds 100 million. The
  depth limits above hold, but nothing limits how many panels a document builds.
- **The name a peer gives itself isn't limited.** The part after the slash in `agent/<name>` is any text up to the
  message size, line breaks included. It is shown to the other peers (`from` on a relayed request, `author` in the
  history) and to the agents that read them; only the channel before the slash says who sent it.
- **The work budget bounds one event, not a stream of them.** An event stops after its steps, operations or bytes
  (above), which takes well under a second of work in a Release build. But `after`, `debounce` and `every` schedule
  more events, each with a budget of its own, and nothing caps how many are waiting; and across events the state can
  keep growing by up to 64 MiB each: only a revision is held to the state's 32 MiB. Values an expression computes
  are bounded by its operations, not by their size (a `let` that doubles a text at each of many nested nodes still
  builds a large text). A revision that adds steps can cause this. Steps live in `/actions` and also inline: in event handlers on
  nodes, components and templates, and in `onLoad`, `watch` and `keys`. An `ask` rule on `/actions` alone doesn't
  cover them; only allow agents the parts of a screen that hold no steps (the templates allow agents nothing, so
  every change of theirs is reviewed).
- **A revision can change the state control steps read.** It can't add, move or newly trigger a control step, but
  it can change the state their expressions read. A screen with its own `accept`, `restore`, `call` or `send` steps
  should deny agents the `/state` paths those steps read. The same holds for a menu, dialog, toast or quick pick the
  screen already feeds from the state: a revision can change the items it runs (a quick pick keeps its recent items
  in `/state/<name>/recent`), so deny agents those paths when the screen has actions that control changes.
- **A revision can add a `copy` step.** `copy` puts text on the system clipboard, and it isn't a control step: an
  allowed revision can make a button, or a timer, copy text the user didn't choose, which they may then paste into a
  terminal. A proposal being previewed runs its timers too, so this can happen before anyone accepts it. Review
  changes to handlers and timers before accepting them.
- **A revision can point a font at any file.** `fonts.<name>.file` can be changed by a revision, and the app then
  reads that file, if it is a regular file of at most 256 MiB, and parses it as a font; the error a peer gets back
  says whether it could be read. The parsers are FreeType and HarfBuzz, which OSS-Fuzz fuzzes all the time, and far
  more hardened than stb_truetype was; still, a malformed font is untrusted input to C and C++ libraries, and the
  `text` fuzz target here fuzzes the text, not the font files (`lookups` fuzzes the GSUB and GPOS tables the fast
  path for plain text reads itself). A picture of a proposal (`render`) loads the proposal's fonts, and any peer may
  ask for one, so the font a proposal names is read and parsed before a reviewer looks at it. Fonts no screen uses
  are bounded (the limits above),
  but the fonts the current document declares are all kept, however many it declares: only the document's size
  bounds them, and each costs at least its first atlas page (up to 16 MiB for very large sizes).
- **Image files are decoded by stb_image.** A revision can point an `image` node at any file inside the screen's
  folder (and the folders the host allows), and the app decodes it, as PNG or JPEG, on a worker thread. Files
  elsewhere are refused, the path resolved with symbolic links followed, so a revision can't show a picture from
  anywhere on the disk; a host that allows a folder (`Screen::allowImageDirectory`) allows every picture in it to
  agents, and a picture of a proposal (`render`) decodes the files the proposal names before a reviewer sees it.
  stb_image is small and widely used but not hardened like libpng: its input is bounded (the limits above,
  read from the header first) and fuzzed (`image`), and only PNG and JPEG are compiled in. Font files are not held
  to folders yet (the gap above).
- **Errors can name files by their full path.** An image or font that can't be read is reported to peers with its
  resolved path, and the MCP server can pass on a system error that names one, so a peer can learn where the screen's
  files are on the disk.
- **Image files are checked by path, then opened.** A file inside an allowed folder is resolved and checked, then
  read by a worker thread, so whoever can write that folder can swap it for a link in between, or put a hard link
  there to a file elsewhere; this needs write access to the folder. An image that fails to decode isn't remembered,
  so it is read and decoded again each time it is asked for.
- **Scratch folders are made private just after they are created.** The folders `render`, `test` and pictures of
  proposals draw into are created under a random name in the system's temporary folder, then limited to their
  owner: under a umask of 000 another user could put a file there in between.
- **A grid's children aren't limited.** Placing one looks at each of the grid's cells about once, up to a million
  for a grid of 1,000 tracks a side, so a revision that adds tens of thousands of children to a full grid makes each
  layout slow.
- **Keys chosen to collide.** `parseUntrusted` indexes an object's keys by a hash with no random seed, so a message
  whose keys were made to collide could take time in proportion to the square of its keys (not demonstrated).
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
