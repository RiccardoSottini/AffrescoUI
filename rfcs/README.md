# RFCs

Design documents for the big changes. An RFC ("request for comments") describes a change before it is built, so the
people who depend on AffrescoUI can see it and the design can be argued over while it is still cheap to change.

## When an RFC is needed

Write one before you change a **public contract** or the shape of the project:

- **The screen format:** a new section, node type, key, step, event or expression function, or a change to what an
  existing one means. Screens written today must keep working.
- **The live protocol or the MCP tools:** a new message or tool, or a change to an existing one.
- **The security model:** capabilities, permissions, peers, the network, secrets, sandboxing or limits.
- **The modules:** a new module, code moved between modules, or a public C++ API change that isn't
  source-compatible.

Bug fixes, new tests, docs, refactors that keep behaviour, and features built entirely in `examples/` don't need one.
When you aren't sure, ask in an issue.

## How it works

1. **Discuss first.** Open an issue with the *Format-change RFC* template (labelled `rfc`) and describe the problem
   and a sketch of the design.
2. **Write it.** Copy [`0000-template.md`](0000-template.md) to `rfcs/0000-short-name.md` and fill in every section.
   Open a pull request with the RFC alone, and link the issue.
3. **Number it.** RFCs are named `NNNN-short-name.md`: four digits, then a few lowercase words joined by hyphens
   (`0007-image-nodes.md`). Before it is merged, the maintainer gives it the next free number. `0000` is the
   template.
4. **The maintainer's merge is the approval.** Review happens on the pull request, and the RFC changes until it is
   right. A merged RFC is accepted. A closed one is declined, and the reason is written on the pull request.
5. **Implementation waits for the merge.** It comes in its own pull request (or several), each linking the RFC. If
   the design has to change while it is built, the RFC is updated in that pull request so it stays true.

An accepted RFC is a record of a decision, not documentation: once it is built, `SCREEN_FORMAT.md`,
`VERSION_CONTROL.md` and the READMEs next to the code describe how things work.

## Prototypes

An RFC may come with a prototype in `prototypes/<number>-<name>/`: screens, scripts or checks that try the design
before anything is built in the library (a screen that runs today, the proposed syntax side by side, the edits an
agent would send, an executable specification of a protocol). Prototypes use only what exists and the Python standard
library, change nothing outside their folder, and aren't part of the build. `prototypes/tokens.py` counts the tokens
of a prototype's documents and edits, the way `tests/perf/tokens.py` does for the templates.

## Accepted RFCs

None yet.

## Under review

| RFC | Decides |
|---|---|
| [0001: Contributions](0001-contributions.md) | Commands, keybindings with chords and `when`, context keys, menus, settings |
| [0002: Services and data models](0002-services.md) | Native providers, out-of-process JSON-RPC services, `invoke` and `subscribe`, host data models (D2) |
| [0003: Connections, sign-in and servers](0003-network.md) | Declared connections and operations, OAuth sign-in, secrets, remote peers, a document server (D3) |
| [0004: Features as packages](0004-features.md) | Manifests, namespaces, installs as revisions, composition checks, signatures, a catalogue |
| [0005: Lisp as a programming language](0005-lisp-language.md) | A runtime for Lisp apps, with JSON staying as the format for static apps; run-time checks by author; diffs and review of programs |
