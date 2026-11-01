# RFC 0005: Lisp as a programming language

- **Issue:** none yet
- **Author:** Claude, for the maintainer
- **Changes:** the screen format; the live protocol and the MCP tools; the security model; the modules (a new one)
- **Builds on:** the Lisp notation (#97, #108), per-file diffs (#99), RFC 0004 (features)

## The problem

AffrescoUI has two ways to write an app, and today they mean the same thing. A `.lisp` file is another spelling of the
JSON tree: it is read, never evaluated, and everything that computes is JS-style text in a string. That was the right
first step, and it has reached its limit:

```lisp
:other "{{find(state.order, it != state.win[state.sel].buf && state.bufs[it].kind != 'splash') ?? state.win[state.sel].buf}}"
```

- **The same logic is repeated.** A document has no general functions, so that expression is copied wherever it's
  needed.
- **Apps can't be extended from inside.** Calamaio, the Emacs-like example, offers a `*scratch*` buffer "for Lisp you
  want to try out", and nothing can run what is typed there. In Emacs, extending the editor by evaluating code inside it
  is the point.
- **It looks like Lisp and isn't.** `:when (not state.x)` is accepted, and reads as a node of type `not`. Lisp
  programmers expect it to run.

Out of scope: running existing Emacs Lisp packages, and any change to how JSON apps work.

## The design

**Two kinds of app, each good at its job.**

| | JSON | Lisp |
|---|---|---|
| What it is | A format, for static apps | A programming language, with a runtime |
| What happens to the file | Parsed and displayed, **exactly as it is today** | Run: it defines functions, commands, screens |
| Logic | Expressions and steps | Functions, closures, macros |
| Diffs, proposals, permissions, undo, blame | Yes | Yes |

```lisp
;;; killring.lisp --- the kill ring

(defcustom kill-ring-max 60 "How many kills are kept." :type integer)

(defn other-buffer
  "The most recent buffer that isn't this one or the splash screen."
  []
  (or (find (fn [b] (and (not= b (current-buffer))
                         (not= (buffer-kind b) "splash")))
            (buffers))
      (current-buffer)))

(defcommand kill-line
  "Kills the rest of the line, or its line break at the end; with an argument, that many times."
  :title "Kill Line" :category "Killing"
  [times]
  (kill :span "lineEnd" :times times))

(defkey "ctrl+k" kill-line)

(add-hook after-save notify-saved)

(defn room-card [room]
  `(panel :id ~(str "room-" (:id room))
     (label :text ~(:name room))))

(defscreen main
  (column :repeat {:for room :in state.rooms}
    ~(room-card 'room)))
```

Function names such as `kill`, `buffers` and `current-buffer` are an app's own or the library's, and are illustrative
here. The syntax is what this RFC decides.

### The language

- **Clojure's syntax, Emacs's vocabulary.** `defn`, `fn`, `let`, maps, vectors, keywords and immutable values come from
  the Clojure family (the EDN data syntax), because its values are JSON's: distinct `nil` and `false`, real maps.
  `defcustom`, `defcommand`, `defkey`, `add-hook`, `advice` and `describe-function` come from Emacs, because that is the
  environment it follows. It is neither Clojure nor Emacs Lisp, and says so.
- **Not Emacs Lisp itself:** `nil`, false and `()` are one value there and it has no map literal, so JSON values
  wouldn't survive a round trip. Running real packages would also need much of Emacs's own primitive set.
- **The core:** lexical closures, proper tail calls, `defmacro` (load time only), destructuring, `~`-unquoting inside
  backquote, conditions (errors you can catch), docstrings on every definition form.
- **A new module, `modules/lisp`, on `core`.** Values are the JSON-shaped ones the library already uses, so nothing
  converts at the boundary. No third-party runtime: limits, values and capability checks have to be ours in any case
  (see Alternatives).
- **Library agnostic:** the language has no app features. What makes Calamaio like Emacs is Calamaio's own Lisp.

### Running a program

Top-level forms are **definitions, each with a name**. Their order doesn't matter, except that macros are expanded
first, so a change to one definition never reorders another.

1. **Load.** Every file is split into its top-level forms. Each form's text, including the comment above it, is one
   entry, and that keeps the file's words and comments exactly.
2. **Build.** The definitions run and produce the tree the renderer already understands: `defscreen` gives `root`,
   `defcommand` gives `actions`, `defkey` gives `keys`, `defcustom` gives `settings`. Build code is **pure**: no state,
   clock, randomness or outside world. The same source always builds the same tree. UI that depends on state uses the
   existing `repeat` and bindings.
3. **Run.** Handlers and bindings call functions. Functions read state, and change the world only through the steps
   apps already have (`set`, `call`, `send`...), so one rule set governs both.

Bindings keep the expression machine: `~form` compiles to the same operators when it can, and otherwise calls the
interpreter.

Files of the current notation still load: **a file holding one data value is read as a template, as if backquoted.**
Words stay data and `~` drops into code, so `(label :text ~(t "burndown.title"))` works inside it, and all 180 generated
files keep their meaning. A bare `(not x)` in a data position becomes an error: `line 3: "not" is not a node type; to
run it, write ~(not ...)`.

### Diffs and every other capability

The source is what is versioned, one entry per definition, inside the document history:

```json
{ "program": { "killring.lisp": { "kill-line": "(defcommand kill-line\n  \"Kills...\"\n  ...)" } } }
```

| Capability | How it works for a program |
|---|---|
| **History, hashes, revert** | Entries are ordinary document values, so RFC 6902 patches, commits and exact undo don't change |
| **Diffs** | The per-file view shows the changed definitions of `killring.lisp`, with their comments. The built tree's diff sits beside it |
| **Blame** | Per definition, from the entry's commit |
| **Permissions** | `read`, `write` and `never` work on `/program/...` paths, and on the built tree (below) |
| **Proposals, preview** | Accepting builds the proposed source; the preview is what accepting produces, because build code is pure |
| **Features, change sets** | A feature's definitions are entries under its namespace (RFC 0004). `provides` and `needs` are worked out from the built tree |
| **Agents** | `get_screen` reads definitions, `submit_revision` replaces them. Every definition has a stable address |

**Review by effect keeps working for what a program builds.** A revision is applied to a copy, the copy is built in the
sandbox, and today's rules judge the built tree. A proposal whose `defscreen` adds a `permissions` section is refused as
it is now, however it was computed.

**Function bodies are checked when they run**, because a language's effects can't all be known from reading it. See the
security review, which is the heart of this RFC.

### The Emacs loop

Evaluating a form is submitting a revision.

- **`eval-defun` in a running app** replaces that definition, and the next call uses it. It lands in history, so it can
  be undone or exported as a change set. `init.lisp` is a change set.
- **`describe-function`, `describe-key`, `apropos`** read docstrings and the entries of the environment.
- **`add-hook` and `advice`** extend a function without replacing it. Each is its own entry, so several authors' hooks
  merge cleanly and blame shows who added what.
- **Who may do it:** a person's own input runs with the person's authority. An agent's code is always a proposal.

This is the one deliberate difference from Emacs: its full power goes to the owner, without its whole-trust model applying
to everyone else.

## The alternatives

- **Do nothing, keep the notation.** Simple and safe, and it leaves the problem above. Syntax alone was measured at −5%
  to −6% of tokens in #97, so the notation's gain doesn't grow by itself.
- **Forms that compile to expressions, with no runtime.** `~(not x)` over today's machine: cheap, and most of the
  readability, with no functions, closures or live redefinition. It stays in this design as how simple bindings run.
- **Real Emacs Lisp.** Instantly familiar, but it can't hold JSON's values and needs hundreds of Emacs's primitives.
- **Embed an existing runtime** (Janet, Guile, ECL, a Scheme). Saves writing an evaluator, but each still needs the work
  budget, JSON values, per-author capability checks and a removed standard library, and adds a dependency that
  documents from strangers can reach. Stage 1 may revisit this if a small candidate fits, measured.
- **Lua or JavaScript.** Well known and embeddable, and code isn't data, so a program can't print itself as stable
  definitions or build trees with macros. The maintainer's direction is Lisp.
- **Keep Lisp's generated folders as the source of truth from JSON.** A program can't be generated from what it
  builds, so apps written in the language have Lisp as their source.

## The cost in tokens

Measured with tiktoken `o200k_base` (the stand-in `tests/perf/tokens.py` uses; compare ratios, not totals) on Calamaio's
real `kill-line` and `other` definitions, with the Lisp written by hand for this comparison.

| Agent edit | Today (JSON) | With this RFC |
|---|---|---|
| Read one definition | 60 | 55 |
| One expression | 33 | 37 |
| The same logic used at three sites | 97 | 55 (one function, three calls) |
| **Change only a command's title** | **21** | **77** (the whole form is replaced) |

**Functions win when logic repeats; small edits lose if a form can only be replaced whole.** That is a real regression for
agents, so stage 2 must add an edit of one keyword inside a form (`{"set": "/program/killring.lisp/kill-line/:title",
...}`), and measure it. Whole-app numbers come at stage 6, with an edit-accuracy test of agents writing Lisp against
JSON, before Lisp is recommended to agents.

## Performance

Nothing here has been measured yet; there is no runtime.

- **JSON apps:** no new code on their path; the benchmarks must show no change.
- **Build:** runs on load and on a revision. The budget is the Forge cold start, recorded before and after.
- **Bindings:** the hot path. Simple `~form`s use the existing expression machine at its current speed. Calling
  interpreted functions per frame is the risk: each stage measures it with the `Benchmarks (callgrind)` job, and a
  regression beyond the existing limits blocks the stage.
- **Memory:** definitions keep their source text in the document, and the environment adds the compiled forms; measured
  at stage 2.

## Security review

The language is what makes this the largest security change since version control. Documents and revisions stay
untrusted.

- **New input and limits.** Source text keeps the reader's limits (256 levels, 64 MiB, bounded numbers). Running uses the
  existing `WorkBudget` (steps, operations, bytes per event), plus a call-depth limit and a limit on allocation, so an
  endless loop or recursion stops with an error naming its place. Macro expansion has its own budget. The reader,
  evaluator and macro expander are fuzzed from stage 1.
- **Capabilities.** The language has **no** file, process, network, foreign-function or `eval`-of-the-host primitive.
  The outside world is reachable only through `call`, `send`, `connect` and the other existing steps, with existing
  grants. A revision can't widen them, and that is checked on the built tree by today's rules.
- **Doing more than the author may: the new mechanism.** Today, `Protected controls` refuses a computed `run` by reading
  the document. That can't hold for a language, since `(apply (lookup name))` hides the target. So **each definition
  carries its author, from the commit, and that author is checked at the moment a control or capability step runs**,
  however it got there: a computed call, a macro's output, a hook or advice. When functions by different authors are on
  the stack, the **least authority on the stack** applies, so a person's command calling an agent's function can't lend
  the agent the person's authority, and the agent's hook can't borrow it. Closures keep their author. This has to be
  proved with attack tests before stage 3 merges.
- **Approving itself.** `accept`, `reject`, `restore` and the other review steps stay callable only from the person's own
  input. Timers, `watch`, hooks and agent-authored code can't reach them, by the same check.
- **Preview.** While a proposal is previewed, its functions run under the rule that exists today: control steps only from
  the person's own input.
- **Build code** is pure and has no step access. A build that exceeds its budget refuses the revision.
- **Hooks and advice** can wrap a protected action. They run with their own author's authority, under the least-authority
  rule, and adding one to a protected action needs the same review as changing that action.
- **Secrets.** Functions reach secrets only as the existing services do; a value printed by `describe-*` or an error
  message goes through the same masking as state. An error's text must not include values read from masked paths.
- **Timers and callbacks.** A function can only start the timers steps can. Hooks belong to their entry, and removing
  the entry removes them, including when a revision is undone.
- **`SECURITY.md` changes.** "A file in the Lisp notation is data only, never evaluated" stops being true for programs,
  and the guidance on loading untrusted files has to say so.

## Compatibility

- **JSON apps:** nothing changes, in format, behaviour or speed.
- **Existing `.lisp` files:** keep loading as templates and give identical trees, checked by the existing test that
  compares each app's two loaded documents.
- **Two things that aren't additive:** a bare `(not x)` in a data position stops being read as a node (a node type of
  that name still works when written as a type), and `SECURITY.md`'s promise above.
- **"Both spell the same document"** remains true for data files and stops being true for programs. A program can't be
  converted to JSON and back: `convert` refuses one, naming the first definition that needs running.
- **Examples:** today `json/` is the source and `lisp/` is generated. An app rewritten as a program has `lisp/` as its
  only source, its `json/` removed, and its Launcher entry says so. The others stay as they are.
- **History** stays RFC 6902. `program` is a new top-level key; the schema, `SCREEN_FORMAT.md` and the catalog change
  together.
- **Public C++ API:** additive, a new module.

## Rollout, one pull request each

1. **The language, pure.** Reader additions, evaluator, closures, macros, budget, templates. `affrescoui run` and a REPL
   in the CLI, fuzzing. Every existing `lisp/` file gives an identical tree. No live part.
2. **Definitions in the document.** `program`, version control and per-file diffs on definitions, review by the built
   tree, the keyword edit, comments kept. One small app (Streak) rewritten.
3. **The live runtime.** Handlers and bindings call functions, authors tracked, run-time checks. Its own security
   review and attack tests.
4. **The Emacs loop.** `eval-defun`, `describe-*`, commands from functions; Calamaio's `*scratch*` and `init.lisp` really
   work.
5. **Hooks and advice,** with their review.
6. **Roll out** app by app, and the agents' edit-accuracy test.

## Decisions taken by the maintainer

1. **Build the language.** This reverses the choice in #97 that Lisp stays a notation.
2. **Clojure's syntax with Emacs's vocabulary.**
3. **Examples move app by app,** Streak and Calamaio first. JSON stays as the format for static apps.
4. **Undo restores definitions exactly;** data a function already changed stays. This matches today's rule that state
   changes aren't undone (`VERSION_CONTROL.md`).
5. **`*scratch*` has the person's full authority** for what they type themselves, never from a timer or a handler that
   fires by itself, as the `patch` step already is.

## Open questions

- **Least authority on the stack:** is it enough for callbacks an agent's code schedules, and does it make a person's
  commands too weak when they use agent-written helpers? Stage 3's attack tests and real use should settle it.
- **The keyword edit's address:** the `:title` path above is a sketch. It must not make a form's text and its structure
  disagree.
- **`@word` tokens:** a stock EDN reader rejects them. Kept unless third-party EDN tooling turns out to matter.
- **Conventional-syntax fixes** from the notation review (`type` as the head, `panel#id`, quoted display text, indentation,
  `fmt` keeping comments) go in their own PR before stage 1, as they apply either way. `fmt` currently deletes comments.
- **Whether JS-style expressions retire in Lisp apps** is left for after stage 6.
