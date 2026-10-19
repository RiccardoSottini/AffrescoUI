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

The screen file, its includes and the state file are read without the limits applied to peers. Don't load a screen
file or change set you wouldn't run as a program. The state file is the exception to "trusted": it holds the
persisted parts of the live state, which peers can change, and it is read back at the next start without those
limits. Keep the paths a screen persists out of reach of agents you don't trust.

### Who a peer is
A **peer** is a program connected to the running screen over a local socket. The host decides who each peer is:
- The host listens on one socket per role. The example host listens on `studio.sock` and `agent.sock` in its socket
  directory, so a program that connects to `agent.sock` is the author `agent`, whatever it says.
- The name a peer gives itself (`"author"` in a message) is only kept after a slash, for display: `agent/claude-code`.
  Any program on the same socket can give itself the same name, so rules and reviewers should name channels
  (`agent`), not full names.
- The socket file is made readable and writable only by its owner, just after it is created; until then it has the
  process's default permissions. The example host, the Studio and the MCP server keep the sockets in a directory
  (`/tmp/affrescoui-<uid>` by default, or `--socket DIR`) that must be a real directory owned by the current user,
  not a symbolic link (only the last part of the path is checked); the host makes it accessible only to that user,
  and the Studio and the MCP server don't check its permissions. The directory is what keeps other users out (see
  "Known gaps"), so pass a `--socket` directory whose parents only you control.

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
  control changes), and `call`, `send` and `connect` (which reach outside the screen). A revision can't add, remove
  or alter one. Otherwise an agent could hide an "accept" inside a button and approve itself, or give the screen new
  access to the outside world.

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
`id`, or inside a component, `locked` protects nothing. A screen without a `permissions` section applies every change and
lets anyone review (the `sign-in` example is one): give a screen permissions before you let an agent connect to it.

### Reviewers, and why an author can't approve its own change
A change that needs review becomes a **proposal**. Of the peers, only the authors listed in `permissions.reviewers`
may accept or reject proposals, preview them, change the mode or revoke grants. Review steps in the screen file
itself are not checked against that list: they act as `user`, for whoever uses the app. Several rules keep an author from approving itself:
- a reviewer can't accept a proposal whose author is on the reviewer's own channel;
- a revision can't add or change review controls (see above), so a proposal can't approve itself from inside the
  screen;
- the MCP server for agents has no accept tool, and the example host gives agents their own socket, which the
  templates don't list as a reviewer.

**Modes and grants** are set by reviewers. `acceptEdits` applies changes that only wait because no rule allows them
(`ask` and `deny` still hold). `plan` suspends the `allow` rules and grants of authors who aren't reviewers. "Always
allow" adds a grant for exactly what the accepted proposal touched, for its author's channel, until the host quits.
Grants are allow rules, so `ask` and `deny` still win.

### You accept what you saw
An author can keep pushing commits to its open proposal. Over the live protocol, an `accept` must name the proposal's
head commit (at least its first 7 characters): the version the reviewer looked at. If the author pushed since, the
approval is refused and the proposal stays open for another look. The pin protects reviewers that send it: an
`accept` whose `head` is an empty string is not checked, so a reviewer program must always send the head it showed.

This holds for reviewers that connect as peers, like the Studio. The two other ways to accept, the C++ call
`Screen::acceptProposal` and an `accept` step in the screen itself, accept the proposal as it is at that moment,
because the code that calls them is trusted and sees the current version.

### What `onCall` and `onAction` expose
A host can register native code that documents use:
- `onCall(name, function)` makes `name` callable from `{"call": name, "with": ..., "into": "state.x"}` steps. The
  function gets the `with` value, which the screen computes, often from the live state or the event. Treat it as
  untrusted input: check it and limit it. Revisions can't add or alter `call` steps, so only calls the screen file
  already contains can reach it. But a revision can make an existing call run at other times (see "Known gaps"),
  change the state its `with` reads, and add new uses of a component or template that holds it, each with its own
  parameters: the function can get any argument an author chooses.
- `onAction(name, action)` registers an action by name. A document reaches it wherever it names an action that the
  file doesn't define: `"onClick": "name"`, `{"run": "name"}`, or a `call` step. Those references are not control
  steps, so **a revision can attach any registered native action to a new button or event**. Register as native
  actions only things any author may trigger.

Whatever a native function or action does, it does with the host's full rights. Version control can't see inside it.

### Channels a screen declares
A screen's `channels` section names local sockets the screen connects to. Relative paths are resolved in
`AFFRESCOUI_SOCKET_DIR`, or `/tmp/affrescoui-<uid>`, or the directory the host sets. Every message that arrives
there runs the screen's `<name>:<type>` action, if it has one, with the message as its event, so the program at the other end can
run those actions with any data. Revisions can't change `channels` or add `send` and `connect` steps.

### Requests relayed between peers
`request` and `reply` messages aren't interpreted. The host passes them to every other peer, with `from` set to the
sender's author. A request is how a person asks an agent for a change, so an agent should treat it as instructions
from whoever `from` names, and nothing more. Any peer can send one.

### What documents can't do
A document has no scripts. Expressions are evaluated by the engine, and steps are a fixed set. A document can't start
a process (the `ProcessChannel` and `StdioChannel` classes are only opened by host code), open a network connection,
or write files, apart from the parts of the state its `persist` section lists, which go to the state file the host
chose.

## Limits

Input from peers, revisions and change sets is limited so that one message can't exhaust the stack or loop forever:

| What | Limit |
|---|---|
| JSON nesting of one message from a peer, a revision, a change set or a declared channel | 256 levels |
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
| A font's glyph atlas (a larger font size is refused) | 4,096 × 4,096 pixels |

## Known gaps

These are not protected today. They are stated plainly so that you can decide what to expose.

- **The peer's user id isn't checked.** The host doesn't ask the operating system who is on the other end of a socket
  (`SO_PEERCRED`, `getpeereid`). The example programs check that the socket directory belongs to the current user and
  make it private, and that is the only barrier. The library doesn't check the directory of a screen's declared
  `channels`, nor the directory of a socket an embedding application passes to `listen`.
- **There is no rate limit per peer.** A peer can send messages as fast as it likes, and every revision is checked
  and, if it applies, rebuilds the screen. Nothing limits how many peers connect. The open-proposal limit caps
  waiting proposals, not work.
- **There is no limit on the size of a message.** A message is one line of any length, buffered until its line break.
  A revision can be as large as the peer likes, and so can the lists it puts in the state, which the screen may
  render as one panel per item.
- **The document and state have no depth limit.** The 256 levels hold for each message, not for what it builds:
  a revision can add a value deep inside one an earlier revision added, and steps can wrap a value in itself, so
  the document and state can nest deeper with every change. Some of the code that walks them is recursive.
- **There is no total work budget per event, so nested loops multiply.** Each limit above holds on its own:
  a `while` of 10,000 iterations inside a `forEach` over a list of 10,000 items runs 100 million steps, and deeper
  nesting multiplies again. `after` and `debounce` steps schedule more work for later, and nothing caps how many are
  waiting. A revision that adds steps can cause this. Steps live in `/actions` and also inline: in event handlers on
  nodes, components and templates, and in `onLoad`, `watch` and `keys`. An `ask` rule on `/actions` alone doesn't
  cover them; only allow agents the parts of a screen that hold no steps (the templates allow agents nothing, so
  every change of theirs is reviewed).
- **A revision can change when existing control steps run.** It can't add or alter a control step, but it can move
  one unchanged, or run the action that holds it from a new trigger (a timer, a load event, a new button), and it can
  change the state its expressions read. A screen that has its own `accept`, `restore`, `call` or `send` steps should
  send to review every change to the places that hold or trigger steps (see the bullet above), and deny agents the
  `/state` paths those steps read.
- **A revision can point a font at any file.** `fonts.<name>.file` can be changed by a revision, and the app then
  reads that file and parses it as a font; the error a peer gets back says whether it could be read. The font parser
  (stb_truetype) is not hardened against malformed files, and every new file and size bakes and keeps another font
  in memory.
- **The history isn't saved.** Commits, proposals and grants live in memory and are lost when the host quits, so the
  audit trail doesn't survive a restart. Every commit also keeps a full copy of the document, so memory grows with
  the history.
- **Socket channels exist only on POSIX systems.** On Windows the live protocol isn't available yet.
