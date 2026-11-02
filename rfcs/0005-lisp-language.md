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
  (column ~@(map room-card [{:id "kitchen" :name "Kitchen"} {:id "hall" :name "Hall"}])))
```

Function names such as `kill`, `buffers` and `current-buffer` are an app's own or the library's, and are illustrative
here. The syntax is what this RFC decides. Building runs on values known when building: a list from `state` goes through
the existing `repeat` and bindings, written as data, since nothing in a build can read `state`.

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

Files of the current notation still load. The rule:
**a file holding one data value is read as a template, as if backquoted.**
Words stay data and `~` drops into code, so `(label :text ~(t "burndown.title"))` works inside it, and all the generated
files keep their meaning (a file with no `~` still goes through the notation's reader, as it costs less). A bare `(not x)` in a
data position is an error: `line 3: "not" is not a node type; to run it, write ~(not ...)`.

As built, in stage 3, in a top-level template, a `~form` is **worked out at the build unless it needs the running app**: it names a dotted
word no definition has (a path), or what nothing defines, or a function that reaches one of those or a host's primitive that isn't pure. Then
it stays in the tree as a binding: the expression machine's text if it is an operator it has (`~(not state.x)` is `"{{!(state.x)}}"`, with an
expression's truth), or the call `"{{lisp('name', state.x)}}"`, which can only read.

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
| **Features, change sets** | A change set's patches apply to `/program/...`. A feature's definitions would be entries under its namespace (RFC 0004), with `provides` and `needs` worked out from the built tree; stage 2 stops at refusing a manifest with a `program`, as namespacing them needs a pass of its own over a function's names and calls |
| **Agents** | `get_screen` reads definitions, `submit_revision` replaces them. Every definition has a stable address |

**Only an app that has a program can change one.** A revision can't give a static JSON app a `program`, or take the whole of one away, as a JSON app gains no code: a person's evaluation can't either, and `evaluate` says so.

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

As built, in stages 4 and 5 (SCREEN_FORMAT.md, "The loop" and "Hooks and advice"):

- **The tools are the library's, and generic.** A step `evaluate` (text on the person's own press), `formBefore`, `formAround`, `printForm`, `describe-function`, `describe-variable`, `describe-key`, `apropos`. The commands `eval-defun`,
  `eval-last-sexp` and `eval-region` are Calamaio's Lisp, on those.
- **`checkChange` is judged again for the person's own change only:** `VersionControl::commitOwnInput()`, a call in the host's code that only `/program/` paths pass, skips the scan of what reaches a control step and nothing else. A marker in the commit carries that into a history reloaded.
- **`init.lisp` is a change set.** What the person evaluates lands in `/program/init.lisp` when no file has its name; `exportChanges` and `importChanges` carry it. A text that arrives from elsewhere goes through `propose`: a proposal in its own name, with no authority of the person's, even accepted.
- **Decision 5 is enforced by proof, not by marks.** The person's authority goes only to what the person typed: `format::TypedText` records the runs of a bound value that came from their own keystrokes, and anything else `evaluate` is given, a buffer an agent wrote, a paste, an undo, a copy by their own command of what they didn't type, or text loaded from a file, is a proposal in the name `plugin:evaluated`, and doesn't run if it holds anything but definitions. The price: what the person didn't type here has to be retyped to get their authority. A confirmation that shows the whole form could lift that, and it would have to be drawn by the host, as nothing a document draws can be trusted for it; it isn't built.
- **`remove-hook` and `defhook` are definitions too.** Nothing changes at run time: a program's hook points are what its entries say, in that version.

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

**Functions win when logic repeats; small edits lose if a form can only be replaced whole.** That was a real regression for
agents, so stage 2 adds an edit of one keyword inside a form. Measured the same way on the same `kill-line`, the edit alone as
compact JSON without its message:

| Change a command's title | Tokens |
|---|---|
| Today, in JSON: `{"set":{"/actions/kill-line/title":"Kill to End"}}` | 15 |
| Replacing the form: `{"set":{"/program/killring.lisp/kill-line":"(defcommand ..."}}` | 82 |
| **The keyword edit: `{"set":{"/program/killring.lisp/kill-line/:title":"Kill to End"}}`** | **21** |

The difference with JSON left is the longer path. Whole-app numbers come at stage 6, with an edit-accuracy test of agents
writing Lisp against JSON, before Lisp is recommended to agents.

## Performance

- **JSON apps:** no new code on their path; the benchmarks must show no change.
- **Build:** runs on load and on a revision. The budget is the Forge cold start, recorded before and after.
- **Bindings:** the hot path. Simple `~form`s use the existing expression machine at its current speed. Calling
  interpreted functions per frame is the risk: each stage measures it with the `Benchmarks (callgrind)` job, and a
  regression beyond the existing limits blocks the stage.

Measured at stage 3 (`affrescoui_bench`, a Release build, wall clock, on Apple silicon, on a shared laptop, so a few percent either way; base: stage 2):

| Workload | Base | This stage | |
|---|---|---|---|
| `forge/open`, Forge's cold start | 215.7 ms | 216.5 ms | unchanged |
| `load/kanban`, a JSON app | 39.4 ms | 39.8 ms | unchanged |
| `update/400` | 8.75 ms | 8.90 ms | within the noise of repeated runs |
| `frame/400` | 911 us | 864 us | within the noise |
| `revision/kanban/rename-label` | 36.7 ms | 36.2 ms | unchanged |
| `load/habit-tracker` | 42.1 ms | 45.2 ms | Streak has a program of about 100 definitions to build and load now |
| `revision/habit-tracker/rename-label` | 39.9 ms | 42.0 ms | the same: each revision builds it |
| `lisp/event`, an event that calls a function that reads and sets the state | | 1.11 us | |
| `lisp/event-as-steps`, the same event as a step and an expression | | 0.75 us | 1.5 times |
| `lisp/binding`, a binding that calls an interpreted function, per label per frame | | 5.3 us | |
| `lisp/binding-as-expression`, the same as an expression | | 3.5 to 5.3 us | at most 1.5 times |
| `lisp/reload-200`, one definition changed in a program of 200: build, review, new interpreter | | 3.0 ms | |

So an interpreted function is about one and a half times what steps and expressions cost for the same small work; a frame of bound labels is
dominated by what isn't the language. A run leaves nothing behind (`retainedBytes` 0 per event). JSON apps have no new code but one
pointer checked per step, per timer and per unknown action, which no workload can tell from noise.
- **Memory:** definitions keep their source text in the document, and what they build beside it. The history keeps only the
  source. A compiled environment isn't kept: a thread keeps its last four builds (`builtProgram`).

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
- **Build code** is pure and has no step access. A build that exceeds its budget refuses the revision. Its budget is
  tighter than running's, as a revision pays for it before any review: 2,000,000 operations and 16 MiB.
- **Limited readers.** A reader whose reads are limited sees no definition its `read` doesn't name, and can't change a program,
  since a definition run at build time reads any other's value and its result lands in what its author reads. Stage 2 chose
  this over working out what a text reveals: deny rather than leak. It may be loosened when reads of definitions can be traced.
  **As built, the authority weighed is wider than the stack:** every author whose code has run since the outermost call began, those that
  returned too, and whoever scheduled it. See "Open questions", answered. **The attack tests run with the revision's scan off**, so the check at
  run time holds by itself (`tests/unit/lisp_runtime.cpp`), and the scan is a first line on top: a function counts as what it reaches, which is
  sound as the language has no way to find a global from a value (`run` counts as reaching anything).
- **Hooks and advice** can wrap a protected action. They run with their own author's authority, under the least-authority
  rule, and adding one to a protected action needs the same review as changing that action.
  **The agent's denial of service, answered by stage 5's attack tests** (`tests/unit/lisp_hooks.cpp`): under the rule as it was, an agent's `:before` on a person's command, or its hook before the person's next step, would have taken the command's authority away:
  a clear error, and a denial of service an agent could inflict. So a hook, `:before` and `:after`, which give nothing back, run **sealed:** what runs next doesn't count them, and it loses nothing, as nothing of theirs is in it. They can't stop it by failing (reported, naming who) or by spending (they
  share half of what the event has left). What they can leave is data in the state, which already isn't authority (below). `:around` does hold what it wraps, so it is not sealed: going on with the original brings its author into what ran, so the original's control step is refused, naming who, and it
  can skip the original. That is what replacing the function does, and is decided the same way: any advice is always reviewed, and undo removes it. An advice's function that isn't the person's or the file's gets no arguments and no result, and can't change the arguments.
- **Secrets.** Functions reach secrets only as the existing services do; a value printed by `describe-*` or an error
  message goes through the same masking as state. An error's text must not include values read from masked paths.
- **Who wrote it, exactly.** A definition's author is its commit's. A person's restore or revert that brings back someone else's text leaves it
  theirs (found while attacking this: an agent's text restored by a person's undo got the person's authority); accepting a proposal doesn't change it;
  and during a preview what the proposal changes is its author's (found too: a proposal's new text was credited to what it replaced).
- **Timers and callbacks.** A function can only start the timers steps can. Hooks belong to their entry, and removing
  the entry removes them, including when a revision is undone.
- **Found by the last review, with attacks in `tests/unit/lisp_gate.cpp` and `lisp_typed.cpp`:** a form an agent wrote into a buffer ran with the person's authority; who added a hook or advice wasn't counted when it ran from outside any function, so an agent's hook on a person's function ran its control step; a kept `proceed` referred to what its call held; a stranger's advice read the variables around it; and the language's `%` functions read arguments they weren't given.
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

1. **The language, pure.** Reader additions, evaluator, closures, macros, budget, templates. `affrescoui run` (which
   showed screens, and goes on showing them: a program, or `--print`, prints what it builds) and a REPL in the CLI, fuzzing.
   Every existing `lisp/` file gives an identical tree. No live part.
2. **Definitions in the document.** `program`, version control and per-file diffs on definitions, review by the built
   tree, the keyword edit, comments kept. Proved on a test fixture.
3. **The live runtime.** Handlers and bindings call functions, authors tracked, run-time checks. Its own security
   review and attack tests. One small app (Streak) rewritten, since its behaviour needs live functions. As built, a new
   version of the program is a new interpreter, whole; a callback is a definition's name and JSON, never a closure; and a `def` is worked out at
   the build, purely, so nothing lasting comes of an event, which gives back all it made.
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
   fires by itself, as the `patch` step already is. As built: only for what they typed, proved from the keystrokes ("The Emacs loop").

## Open questions

- **Least authority on the stack: answered by stage 3's attack tests.**
  - *Enough for callbacks an agent's code schedules?* Yes, once the callback keeps the authors of what scheduled it, as it does: the run that fires later adds
    them to whoever then runs. The stack alone says nothing there, as the agent's code is gone by then.
  - *Enough at all?* **The stack alone isn't.** A person's command that calls an agent's helper, which returns `call` or the name of a person's function, then
    runs it with only the person on the stack (`((helper) "x")`, tests "a closure, or a primitive, an agent hands..."). So the host weighs every author whose
    code has run since the outermost call began, those that returned too (`CallContext::ran`), which keeps the stack's simple rule and closes that.
  - *Too weak for a person's commands?* It is as weak as it says: a person's command that used an agent's helper can't then run a control step, and is
    refused, naming the author and the step. A person who wants it to has to make the helper theirs, by reviewing and rewriting it, or by a stage-4
    `eval-defun`, or to put the control step in a command that calls no agent code. Ordinary steps aren't touched, so most helpers in most commands
    never notice. Real use may ask for more, and no route needs it now.
  - *What it doesn't cover:* data. A value an earlier event's agent code left in the state reaches a person's `call` as an argument, as with any JSON
    action whose `with` reads the state. A control step's arguments are untrusted input (SECURITY.md, "What `onCall` and `onAction` expose").
- **Least authority and hooks: a sealed run, answered above.** The rule stays what `CallContext::ran` says, for all that gives a value back; only what gives nothing back is left out of it.
- **`@word` tokens:** a stock EDN reader rejects them. Kept unless third-party EDN tooling turns out to matter.
- **Conventional-syntax fixes** from the notation review (`type` as the head, `panel#id`, quoted display text, indentation,
  `fmt` keeping comments) go in their own PR before stage 1, as they apply either way. `fmt` currently deletes comments.
- **Whether JS-style expressions retire in Lisp apps** is left for after stage 6.
