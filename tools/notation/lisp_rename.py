#!/usr/bin/env python3
"""Renames an example's functions, values and views to kebab-case, as Clojure writes them: `cardsOf` as `cards-of`.

    python3 tools/notation/lisp_rename.py <app> [--dry-run] [--cli build/affrescoui]

`<app>` is an example written as a program (`examples/<app>/lisp/`), or `shared` for `examples/shared/`. Only the
definitions made with `defn`, `defn-`, `def` and `defview` whose name has an upper-case letter are renamed; commands,
state, computed values, styles, menus, tokens, parts, components, defaults, hooks and settings keep their names, as do
ids, state keys, parameters, strings, comments, paths (`state.cardsOf`) and keywords. The tool reads the code as forms
(never as text to search) and renames every use of a name as a word: a call, a function passed as a value,
`(handler name ...)`, and in a view's template what runs: a handler option (`:onClick name`, `:run name`,
`:onDone [name ...]`), a key's target (`:keys {:escape name}`) and what a component or a timer is given to run
(`:with {:submit name}`, `:every {:do name}`). A node's type, an id, a component or a style is never renamed, and a name
a template holds anywhere else is left whole, never half renamed. For `shared`, a shared definition is renamed in every
app's code too.

A name is left as it is, and listed with the reason, when the new name would collide with another definition (of the
app, of `examples/shared/`, or a second name made the same), with a word of the language or of the host (what
`affrescoui run --print` reports as defined in an empty program), when a parameter or a local has either name, when a
string of the code names it (a docstring aside), or when a file the tool doesn't rewrite names it as a word: a
scenario, a test, the MCP server, a benchmark (an agent task's request). Each name left gets a mark on the line before
its definition, `; style: keep, <reason>`, which `lisp_idioms.py` counts as kept (its rule `camel-case-name`).

The agent tasks of `benchmarks/agents/tasks/` (`lisp-*.json`, `clj-*.json`) whose screen is the app's get the new
names in their addresses (`/program/<file>/<name>`) and in the Lisp of their references. `--dry-run` prints the table,
old name, new name, files and uses, and the names left, and changes nothing. Only the standard library; the host's
words are asked of the built `affrescoui`.
"""

import argparse
import glob
import json
import os
import re
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lisp_idioms  # noqa: E402  pylint: disable=wrong-import-position  # beside this file

from lisp_idioms import Atom, Form, Prefixed, Text  # noqa: E402  pylint: disable=wrong-import-position

ROOT = lisp_idioms.ROOT
EXAMPLES = lisp_idioms.EXAMPLES
TASKS = os.path.join(ROOT, "benchmarks", "agents", "tasks")
# The definitions renamed; every other kind keeps its name (agents, scenarios and the JSON version name them)
RENAMED = ("defn", "defn-", "def", "defview")
# Forms whose vectors right after the head (or a name and a docstring) are parameters
PARAMETER_HEADS = ("fn", "defn", "defn-", "defview", "defmacro", "defcommand", "defhook", "defcustom", "defedit",
                   "defpart", "defcomponent", "defscreen", "defmenu")
# Forms whose first vector binds names at its even places
BINDING_HEADS = ("let", "loop", "for", "doseq", "when-let", "if-let", "when-some", "if-some", "binding", "dotimes",
                 "letfn", "when-first", "with-open", "repeat")
# In a view's template, the keywords whose value names a function: a handler option, a run target
HANDLER_KEYWORD = re.compile(r":(on[A-Z][A-Za-z]*|then|else|done|run|action|command)$")
# Why a name a template or data holds as a word, where it may be text (an id, a node's kind), stays
HELD_AS_TEXT = "a view or data holds it where it may be text"
# The definitions of data, whose words are text: a word in them names a function only as a handler option or a key's
DATA_HEADS = ("defstate", "defcustom", "deftokens", "defstyle", "defdefaults")
# In a view's template, the keywords whose value is never a function's name: an id, a component, a style, a type...
DATA_KEYWORDS = (":id", ":use", ":style", ":type", ":icon", ":text", ":bind", ":role", ":grammar", ":font", ":cursor",
                 ":kind", ":align", ":dir", ":side", ":placeholder", ":label", ":title", ":tooltip", ":image")
# Where a name in a text is a word: not inside another word, a path, an id, a keyword or a token
NAME_BEFORE = r"(?<![A-Za-z0-9_.#:@$\-])"
NAME_AFTER = r"(?![A-Za-z0-9_?!*\-])"
# The folders whose files the tool doesn't rewrite, read for a name: tests, the MCP server, benchmarks, tools
OUTSIDE = ("tests", "mcp", "benchmarks", "tools")
OUTSIDE_SUFFIXES = (".json", ".py", ".cpp", ".hpp", ".c", ".lisp", ".txt", ".cmake")
C_SUFFIXES = (".cpp", ".hpp", ".c")
SCREENS_OF_CHECK = os.path.join(ROOT, "tests", "render", "screens") + os.sep


def kebab(name):
    """A name in kebab-case: `cardsOf` as `cards-of`, `toJSON` as `to-json`, `parseJSONText` as `parse-json-text`."""
    spaced = re.sub(r"(?<=[A-Z])([A-Z][a-z])", r"-\1", name)
    return re.sub(r"(?<=[a-z0-9])([A-Z])", r"-\1", spaced).lower().replace("_", "-")


def needs_rename(name):
    """Whether a definition's name is not kebab-case: it has an upper-case letter."""
    return any(character.isupper() for character in name)


class Reader(lisp_idioms.Reader):
    """lisp_idioms' reader, which also notes where each word starts and what each string holds."""

    def word(self, character, line):
        """The next word, with its offset in the text (`start`)."""
        begin = self.at
        form = super().word(character, line)
        if isinstance(form, Atom):
            form.start = begin
        return form

    def string(self, line):
        """The next string, with its text as written (`written`) and its offset (`start`)."""
        begin = self.at
        form = super().string(line)
        form.start, form.written = begin, self.text[begin:self.at]
        return form


def read(text, place):
    """A text's top-level forms; raises ValueError naming the place."""
    try:
        return Reader(text).forms()
    except ValueError as error:
        raise ValueError(f"{place}: {error}") from error


def head_of(form):
    """The word a list starts with, or None."""
    return form.head() if isinstance(form, Form) else None


def definition_name(form):
    """A top-level definition's kind and name, `("defn", "cardsOf")`, or None."""
    head = head_of(form)
    if head and head.startswith("def") and len(form.items) > 1 and isinstance(form.items[1], Atom):
        return head, form.items[1].text
    return None


def definitions_in(forms):
    """The definitions of a file's top-level forms (and of a top-level `do`), as [(kind, name)]."""
    return [definition_name(form) for form in top_definitions(forms)]


def words_bound(form, found):
    """Adds the words a binding place binds (a word, a vector or a map that destructures) to a set."""
    if isinstance(form, Atom) and not form.text.startswith(":") and form.text not in ("&", "_"):
        found.add(form.text)
    elif isinstance(form, Form) and form.opener in ("[", "{"):
        for item in form.items:
            words_bound(item, found)


def locals_of(form, found):
    """Adds every parameter and local a form binds, anywhere in it, to a set."""
    if isinstance(form, Prefixed):
        if form.form is not None:
            locals_of(form.form, found)
        return
    if not isinstance(form, Form):
        return
    head = form.head()
    if head in PARAMETER_HEADS:
        for item in form.items[1:]:
            if isinstance(item, Form) and item.opener == "[":
                words_bound(item, found)
                break
            if isinstance(item, Form) and item.opener == "(" and item.items and isinstance(item.items[0], Form):
                words_bound(item.items[0], found)  # one arity of several: ([a] ...) ([a b] ...)
    if head in BINDING_HEADS and len(form.items) > 1 and isinstance(form.items[1], Form) and form.items[1].opener == "[":
        for at, item in enumerate(form.items[1].items):
            if at % 2 == 0:
                words_bound(item, found)
    if head == "catch" and len(form.items) > 2:
        words_bound(form.items[2], found)
    for item in form.items:
        locals_of(item, found)


class Uses:
    """Walks a file's forms and notes every use of a name to rename, as a word at an offset; and the words of a
    template that are names to rename but stay, as data (`seen`)."""

    def __init__(self, names):
        self.names, self.found, self.seen, self.strict = names, [], [], False

    def note(self, atom):
        """Notes a word if it is a name to rename."""
        if isinstance(atom, Atom) and atom.text in self.names:
            self.found.append(atom)

    def top(self, form):
        """Walks a top-level form. A definition of data (state, a setting, tokens, a style) is data throughout: only a
        handler option or a key's target in it names a function (`strict`)."""
        if head_of(form) in DATA_HEADS:
            self.strict = True
            for item in form.items[2:]:
                self.template(item, False)
            self.strict = False
            return
        self.code(form, in_macro=head_of(form) == "defmacro")

    def code(self, form, in_macro):
        """Walks a form that runs: every word naming a definition is a use."""
        if isinstance(form, Atom):
            self.note(form)
        elif isinstance(form, Prefixed):
            if form.form is None:
                return
            if form.prefix == "`" and not in_macro:
                self.template(form.form, in_macro)
            else:
                self.code(form.form, in_macro)
        elif isinstance(form, Form):
            items = form.items
            if form.head() == "case":  # a clause's constant is not evaluated
                items = [item for at, item in enumerate(items) if at < 2 or at % 2 == 1 or at == len(items) - 1]
            for item in items:
                self.code(item, in_macro)

    def template(self, form, in_macro):
        """Walks a view's template: data, but for what a `~` runs, `(handler name ...)` and a handler option."""
        if isinstance(form, Prefixed):
            if form.form is None:
                return
            if form.prefix in ("~", "~@", "'"):
                self.code(form.form, in_macro)
            else:
                self.template(form.form, in_macro)
            return
        if isinstance(form, Atom):
            if form.text in self.names:
                self.seen.append(form)
            return
        if not isinstance(form, Form):
            return
        items = form.items
        if form.head() == "handler" and len(items) > 1:
            self.note(items[1])
            items = items[2:]
        for at, item in enumerate(items):
            before = items[at - 1] if at else None
            if isinstance(before, Atom) and before.text == ":keys" and isinstance(item, Form) and item.opener == "{":
                for place, target in enumerate(item.items):  # a key's value runs: {"mod+enter" save :escape cancel}
                    if place % 2 and isinstance(target, Atom):
                        self.note(target)
                    else:
                        self.template(target, in_macro)
                continue
            if isinstance(before, Atom) and HANDLER_KEYWORD.match(before.text):
                if isinstance(item, Atom):
                    self.note(item)
                    continue
                if isinstance(item, Form) and item.opener == "[" and item.items and isinstance(item.items[0], Atom):
                    self.note(item.items[0])
                    for rest in item.items[1:]:
                        self.template(rest, in_macro)
                    continue
            if (not self.strict and isinstance(before, Atom) and before.text.startswith(":")
                    and before.text not in DATA_KEYWORDS
                    and isinstance(item, Atom) and item.text in self.names):
                self.note(item)  # what a component or a timer runs: `:with {:submit save}`, `:every {:do tick}`
                continue
            self.template(item, in_macro)


def strings_of(form, found, top=True):
    """Adds the strings of a form to a list, but a definition's docstring (a sentence about it, not a use)."""
    if isinstance(form, Text):
        found.append(form)
    elif isinstance(form, Prefixed) and form.form is not None:
        strings_of(form.form, found, False)
    elif isinstance(form, Form):
        for at, item in enumerate(form.items):
            if top and at == 2 and isinstance(item, Text) and definition_name(form):
                continue
            strings_of(item, found, False)


def strings_in(form):
    """The strings of a top-level form, its docstring left out."""
    found = []
    strings_of(form, found)
    return found


def named_in(text, name):
    """Whether a text names a name as a word."""
    return re.search(NAME_BEFORE + re.escape(name) + NAME_AFTER, text) is not None


class Source:
    """A code file: its path from examples/, its text and its forms."""

    def __init__(self, path):
        self.path = path
        with open(os.path.join(EXAMPLES, path), encoding="utf-8") as file:
            self.text = file.read()
        self.forms = read(self.text, path)


def code_paths(app):
    """The code files an app's renames may rewrite, and those whose definitions it renames: for an app, its lisp/
    folder; for `shared`, examples/shared/ and every app's (which use the shared names)."""
    shared = lisp_idioms.code_files("shared")
    if app != "shared":
        own = lisp_idioms.code_files(app + "/lisp")
        return own, own
    return sorted(set(lisp_idioms.every_code_file())), shared


def host_taken(cli, names):
    """The names the language or the host already means (what an empty program doesn't report as undefined)."""
    with tempfile.TemporaryDirectory() as folder:
        probe = os.path.join(folder, "probe.lisp")
        with open(probe, "w", encoding="utf-8") as file:
            file.write(";;; probe.lisp --- which words are taken\n\n(defn probe [] "
                       + " ".join(f"({name})" for name in sorted(names)) + ")\n")
        run = subprocess.run([cli, "run", "--print", probe], capture_output=True, text=True, check=False,
                             env=dict(os.environ, AFFRESCOUI_HIDDEN="1"))
    if run.returncode != 0:
        sys.exit(f"lisp_rename: {cli} run --print: {(run.stderr or run.stdout).strip()[:400]}")
    undefined = set(re.findall(r'"([^"]+)" is not defined', run.stderr))
    return {name for name in names if name not in undefined}


def outside_files(app):
    """The files the tool reads but never rewrites, for a name used as a word: the app's scenarios and other JSON,
    tests, the MCP server, benchmarks and tools (the agent tasks it rewrites are read for their requests only). A
    C++ file counts only if it loads the app's program (it names the app and a lisp/ folder): the others hold their
    own documents. The rendering check's own screens are left out for the same reason."""
    places = [os.path.join(EXAMPLES, app if app == "shared" else os.path.join(app, "lisp"))]
    if app == "shared":
        places += glob.glob(os.path.join(EXAMPLES, "*", "lisp"))
    found = {}
    for place in places:
        for path in glob.glob(os.path.join(place, "**", "*.json"), recursive=True):
            if os.sep + "seed" + os.sep not in path:
                found[path] = read_text(path)
    for folder in OUTSIDE:
        for path in glob.glob(os.path.join(ROOT, folder, "**", "*"), recursive=True):
            if (not path.endswith(OUTSIDE_SUFFIXES) or not os.path.isfile(path) or is_task(path)
                    or os.path.dirname(path) == os.path.dirname(os.path.abspath(__file__))
                    or path.startswith(SCREENS_OF_CHECK)):
                continue
            text = read_text(path)
            if path.endswith(C_SUFFIXES) and not loads_program(text, app):
                continue
            found[path] = text
    return found


def read_text(path):
    """A file's text."""
    with open(path, encoding="utf-8", errors="replace") as file:
        return file.read()


def loads_program(text, app):
    """Whether a C++ file may load an app's program: it names the app (`"flow"`, `flow/`) and a lisp/ folder."""
    return ("lisp" in text) and (f'"{app}"' in text or f"{app}/" in text)


def is_task(path):
    """Whether a file is an agent task this tool rewrites (`lisp-*.json`, `clj-*.json`)."""
    name = os.path.basename(path)
    return os.path.dirname(path) == TASKS and name.endswith(".json") and name.startswith(("lisp-", "clj-"))


def tasks_of(app):
    """The agent tasks on an app's program (every one for `shared`), by path, with their content."""
    found = {}
    for path in sorted(glob.glob(os.path.join(TASKS, "*.json"))):
        if not is_task(path):
            continue
        with open(path, encoding="utf-8") as file:
            task = json.load(file)
        if app == "shared" or task.get("screen", "").startswith(f"examples/{app}/lisp/"):
            found[path] = task
    return found


class Plan:
    """What renaming an app does: each name's new name, files and uses, and the names left with their reason."""

    def __init__(self, app, cli):
        self.app = app
        paths, defining = code_paths(app)
        self.sources = [Source(path) for path in paths]
        shared = [Source(path) for path in lisp_idioms.code_files("shared")] if app != "shared" else []
        defined_here = [(kind, name, source.path) for source in self.sources if source.path in defining
                        for kind, name in definitions_in(source.forms)]
        self.defining = [source for source in self.sources if source.path in defining]
        every_name = {name for source in self.sources + shared for _, name in definitions_in(source.forms)}
        locals_found = set()
        for source in self.sources + shared:
            for form in source.forms:
                locals_of(form, locals_found)
        self.left, self.renames = {}, {}
        wanted = {}
        for kind, name, path in defined_here:
            if kind in RENAMED and needs_rename(name):
                wanted.setdefault(name, set()).add(path)
        taken = host_taken(cli, {kebab(name) for name in wanted})
        made = {}
        for name in wanted:
            made.setdefault(kebab(name), []).append(name)
        for name in sorted(wanted):
            new = kebab(name)
            reason = self.refusal(name, new, every_name, locals_found, taken, made)
            if reason:
                self.left[name] = reason
            else:
                self.renames[name] = new
        self.leave_named_elsewhere(shared)
        self.uses = self.find_uses()
        if self.seen:  # a name a template holds where the tool can't tell it runs: left whole, never half renamed
            for path, atom in self.seen:
                self.left.setdefault(atom.text, f"{HELD_AS_TEXT}: examples/{path}:{atom.line}")
                self.renames.pop(atom.text, None)
            self.uses = self.find_uses()

    @staticmethod
    def refusal(name, new, every_name, locals_found, taken, made):
        """Why a name can't be renamed, or None."""
        if not re.fullmatch(r"[a-z][a-z0-9-]*[?!*]?", new):
            return f"{new} is no plain kebab-case word"
        if new in every_name:
            return f"{new} is already a definition"
        if len(made[new]) > 1:
            return f"{new} would be the name of {' and '.join(sorted(made[new]))}"
        if new in taken:
            return f"{new} is a word of the language or of the host"
        if name in locals_found or new in locals_found:
            return "a parameter or a local has the same name"
        return None

    def leave_named_elsewhere(self, shared):
        """Leaves a name a string of the code names (a docstring aside), or one used by a shared file the app doesn't
        rename (it would call the old name), or a file the tool doesn't rewrite names as a word."""
        texts = [(source.path, text) for source in self.sources for form in source.forms
                 for text in strings_in(form)]
        shared_words = set()
        for source in shared:
            for form in source.forms:
                walker = Uses(set(self.renames))
                walker.top(form)
                shared_words.update(atom.text for atom in walker.found)
        outside = outside_files(self.app)
        for path, task in tasks_of(self.app).items():
            outside[path] = task.get("request", "")
        for name in sorted(self.renames):
            naming = next(((path, text) for path, text in texts if named_in(text.written, name)), None)
            if naming:
                self.left[name] = f"a string names it: examples/{naming[0]}:{naming[1].line} {naming[1].written[:60]}"
            elif name in shared_words:
                self.left[name] = "a shared file uses it"
            else:
                files = [os.path.relpath(path, ROOT) for path, text in outside.items() if named_in(text, name)]
                if files:
                    self.left[name] = "named in " + ", ".join(files[:3]) + (" ..." if len(files) > 3 else "")
            if name in self.left:
                del self.renames[name]

    def find_uses(self):
        """Every use of each name to rename, by file: {path: [atom]}; and the template words left (self.seen)."""
        uses, self.seen = {}, []
        for source in self.sources:
            walker = Uses(set(self.renames))
            for form in source.forms:
                walker.top(form)
            if walker.found:
                uses[source.path] = walker.found
            self.seen += [(source.path, atom) for atom in walker.seen]
        return uses

    def table(self):
        """The renames as rows: (old, new, files, uses)."""
        rows = []
        for name, new in sorted(self.renames.items()):
            files = sorted(path for path, atoms in self.uses.items() if any(atom.text == name for atom in atoms))
            count = sum(1 for atoms in self.uses.values() for atom in atoms if atom.text == name)
            rows.append((name, new, files, count))
        return rows

    def write(self):
        """Rewrites the code files (each use renamed, each name left marked to stay, with its reason) and the agent
        tasks; returns how many files changed."""
        changed = 0
        for source in self.sources:
            edits = [(atom.start, len(atom.text), self.renames[atom.text]) for atom in self.uses.get(source.path, [])]
            if source in self.defining:
                edits += self.marks(source)
            if not edits:
                continue
            text = source.text
            for start, length, written in sorted(edits, key=lambda edit: edit[0], reverse=True):
                text = text[:start] + written + text[start + length:]
            with open(os.path.join(EXAMPLES, source.path), "w", encoding="utf-8", newline="\n") as file:
                file.write(text)
            changed += 1
        for path, task in tasks_of(self.app).items():
            if rename_task(task, self.renames):
                with open(path, "w", encoding="utf-8", newline="\n") as file:
                    file.write(json.dumps(task, indent="\t", ensure_ascii=False) + "\n")
                changed += 1
        return changed

    def marks(self, source):
        """The keep marks a file's definitions left in camelCase get, as edits: `; style: keep, <reason>` on the line
        before, unless one is there (lisp_idioms.py counts such a definition as kept)."""
        lines = source.text.split("\n")
        starts = [0]
        for line in lines:
            starts.append(starts[-1] + len(line) + 1)
        edits = []
        for form in top_definitions(source.forms):
            kind, name = definition_name(form)
            if kind not in RENAMED or name not in self.left or lisp_idioms.MARK.search(lines[form.line - 2] if
                                                                                     form.line > 1 else ""):
                continue
            indent = re.match(r"\s*", lines[form.line - 1]).group(0)
            edits.append((starts[form.line - 1], 0, f"{indent}; style: keep, {short_reason(self.left[name])}\n"))
        return edits


def top_definitions(forms):
    """A file's top-level definition forms (and those of a top-level `do`)."""
    found = []
    for form in forms:
        if head_of(form) == "do":
            found += top_definitions(form.items[1:])
        elif definition_name(form):
            found.append(form)
    return found


def short_reason(reason):
    """A left name's reason as a keep mark gives it: without the place a string is at, which moves."""
    if reason.startswith("a string names it"):
        return "a string of the code names it, and a rename never changes a string"
    if reason.startswith(HELD_AS_TEXT):
        return HELD_AS_TEXT
    return reason.replace(" ...", " and more")


def rename_text(text, renames):
    """A Lisp text (a definition an agent task adds) with its uses of the names renamed, as the tool renames a file."""
    try:
        forms = read(text, "a task's reference")
    except ValueError:
        return text
    walker = Uses(set(renames))
    for form in forms:
        walker.top(form)
    for atom in sorted(walker.found, key=lambda atom: atom.start, reverse=True):
        text = text[:atom.start] + renames[atom.text] + text[atom.start + len(atom.text):]
    return text


def rename_address(address, renames):
    """A program's address, `/program/<file>/<name>[/...]`, with its name renamed."""
    parts = address.split("/")
    if len(parts) > 3 and parts[1] == "program" and parts[3] in renames:
        parts[3] = renames[parts[3]]
    return "/".join(parts)


def rename_task(task, renames):
    """Renames, in an agent task, the addresses it names (`unchanged`, `present`, and its references' keys) and the
    Lisp its references add or set; whether it changed."""
    before = json.dumps(task, sort_keys=True)
    for key in ("unchanged", "present"):
        if key in task:
            task[key] = [rename_address(address, renames) for address in task[key]]
    for revision in task.get("reference", []):
        for operation in ("add", "set", "remove"):
            value = revision.get(operation)
            if isinstance(value, dict):
                revision[operation] = {rename_address(address, renames):
                                       (rename_text(text, renames) if isinstance(text, str)
                                        and address.startswith("/program/") and address.count("/") == 3 else text)
                                       for address, text in value.items()}
            elif isinstance(value, list):
                revision[operation] = [rename_address(address, renames) if isinstance(address, str) else address
                                       for address in value]
    return json.dumps(task, sort_keys=True) != before


def report(plan):
    """Prints the table of renames and the names left."""
    rows = plan.table()
    width = max((len(row[0]) for row in rows), default=8)
    print(f"{'old name':{width}}  {'new name':{width}}  uses  files")
    for name, new, files, count in rows:
        print(f"{name:{width}}  {new:{width}}  {count:4}  {', '.join(files)}")
    print(f"{plan.app}: {len(rows)} names renamed, {sum(row[3] for row in rows)} uses, {len(plan.left)} left")
    for name, reason in sorted(plan.left.items()):
        print(f"  left: {name}: {reason}")


def main():
    """Renames an app's names, or prints what it would; returns the exit code."""
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("app", help="an example written as a program (examples/<app>/lisp), or shared")
    parser.add_argument("--dry-run", action="store_true", help="print the table and change nothing")
    parser.add_argument("--cli", default=os.path.join(ROOT, "build", "affrescoui"), help="the built affrescoui")
    arguments = parser.parse_args()
    known = lisp_tree_apps()
    if arguments.app not in known:
        parser.error(f"not an example written as a program: {arguments.app}")
    try:
        plan = Plan(arguments.app, arguments.cli)
    except ValueError as error:
        print(f"lisp_rename: {error}")
        return 2
    report(plan)
    if not arguments.dry_run:
        print(f"{plan.write()} files rewritten")
    return 0


def lisp_tree_apps():
    """The examples written as programs, and `shared`."""
    return sorted(name for name in os.listdir(EXAMPLES) if os.path.isdir(os.path.join(EXAMPLES, name, "lisp"))) + [
        "shared"]


if __name__ == "__main__":
    sys.exit(main())
