#!/usr/bin/env python3
"""Counts the old spellings in the examples' Lisp code, the ones `modules/lisp/reference/style.md` writes another way,
and holds each group of files to its count; measures their size.

    python3 tools/notation/lisp_idioms.py --check [--self-test]  # fails unless every app is at its lock (below)
    python3 tools/notation/lisp_idioms.py --count [name...]  # each rule's count, per file of tools/notation/idioms/
    python3 tools/notation/lisp_idioms.py --kept             # the places marked `; style: keep, <reason>`
    python3 tools/notation/lisp_idioms.py --measure [--json] # code files, lines, bytes and tokens per app, and totals
    python3 tools/notation/lisp_idioms.py --write            # writes today's counts into the idioms files
    python3 tools/notation/lisp_idioms.py --self-test        # the counter's own tests

A code file is a .lisp file that `lisp_examples.is_program_file` accepts, outside `locales/` and `seed/`. The counter
reads each one as forms, so comments and strings never count. A place that must stay is marked by a comment on the
line before, `; style: keep, <reason>`, and isn't counted. Rules that hold only in a function don't count inside a
`~form` or a `defcomputed`, which are bindings.

Each `idioms/<name>.json` is `{"folders": [...], "allowed": {"four-nested-calls": n}, "kept": {"<rule>": n}}`: a folder
of examples/ counts with its subfolders, one ending in `/*` its own files only, and every code file belongs to exactly
one name, so a new example's code fails until it has its file. The lock: every spelling rule counts 0 (a place that
must stay is marked, with its reason: a mark with none fails), each rule's kept places are exactly the number `kept`
gives, and a nest of four calls on a line, a shape rather than a spelling, is exactly what `allowed` gives. So an old
spelling, a new kept place and a new nest each fail until the idioms file is changed beside it, under review. Only the standard
library; the tokens are counted with tiktoken's o200k_base, as `tests/perf/tokens.py` does, when it is installed.
"""

import argparse
import glob
import json
import os
import re
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lisp_examples  # noqa: E402  pylint: disable=wrong-import-position  # beside this file

ROOT = lisp_examples.ROOT
EXAMPLES = lisp_examples.EXAMPLES
IDIOMS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "idioms")
DATA_FOLDERS = ("locales", "seed")
KEEP = re.compile(r";+\s*style:\s*keep,\s*(.*\S)")
# A keep mark of any kind, with a reason or without one (which the check refuses)
MARK = re.compile(r";+\s*style:\s*keep\b")
# A shape, not a spelling: style.md leaves a nest where unnesting reads no better, so it is held to its count
SHAPE_RULES = ("four-nested-calls",)
# The rule a keep mark with no reason counts under: always a failure
NO_REASON = "keep-without-reason"
# The host's string functions that the dialect writes as str/... (strings.md)
STRING_FUNCTIONS = ("trim", "upper-case", "lower-case", "split", "starts-with?", "ends-with?", "includes?", "replace")
# Words before a form that make it a binding: a template's ~form and what it splices
BINDING_PREFIXES = ("~", "~@")


class Atom:
    """A word, a number or a keyword, as written."""

    def __init__(self, text, line):
        self.text, self.line, self.last = text, line, line


class Text:
    """A string, a pattern or a character: its content never counts."""

    def __init__(self, line, last):
        self.line, self.last = line, last


class Form:
    """A list, a vector, a map or a set: `opener` is `(`, `[`, `{`, `#{` or `#(`."""

    def __init__(self, opener, line):
        self.opener, self.line, self.last, self.items = opener, line, line, []

    def head(self):
        """The word the form starts with, if it is a call written with one."""
        return self.items[0].text if self.opener == "(" and self.items and isinstance(self.items[0], Atom) else None

    def arguments(self):
        """The forms after the head."""
        return self.items[1:]


class Prefixed:
    """A form after a reader prefix: `'`, `` ` ``, `~`, `~@`, `@`, `#'`, `#_` or `^`."""

    def __init__(self, prefix, form, line):
        self.prefix, self.form, self.line, self.last = prefix, form, line, form.last if form else line


class Reader:
    """Reads a Lisp text into forms, and the lines its `; style: keep` comments mark."""

    CLOSERS = {"(": ")", "[": "]", "{": "}", "#{": "}", "#(": ")"}
    DELIMITERS = set("()[]{}\"; \t\r\n,")

    def __init__(self, text):
        self.text, self.at, self.line, self.kept, self.unreasoned = text, 0, 1, {}, []

    def forms(self):
        """The text's top-level forms."""
        found = []
        while True:
            form = self.next_form(None)
            if form is None:
                return found
            found.append(form)

    def skip(self):
        """Moves past white space, commas and comments, noting the keep marks."""
        while self.at < len(self.text):
            character = self.text[self.at]
            if character == "\n":
                self.line += 1
            elif character == ";":
                end = self.text.find("\n", self.at)
                end = len(self.text) if end < 0 else end
                marked = KEEP.match(self.text[self.at:end])
                if marked:
                    self.kept[self.line + 1] = marked.group(1)
                elif MARK.match(self.text[self.at:end]):
                    self.unreasoned.append(self.line)
                self.at = end
                continue
            elif character not in " \t\r,":
                return
            self.at += 1

    def next_form(self, closer):
        """The next form, or None at the end of the text or at the closer given (which it reads)."""
        self.skip()
        if self.at >= len(self.text):
            return None
        character, line = self.text[self.at], self.line
        if closer is not None and character == closer:
            self.at += 1
            return None
        if character in ")]}":
            raise ValueError(f"line {line}: an unexpected {character}")
        two = self.text[self.at:self.at + 2]
        if two in ("#{", "#("):
            self.at += 2
            return self.items_of(Form(two, line))
        if character in "([{":
            self.at += 1
            return self.items_of(Form(character, line))
        if character == '"' or two == '#"':
            return self.string(line)
        return self.word(character, line)

    def word(self, character, line):
        """The next form that is no collection nor string: a prefixed form, a character or an atom."""
        prefix = next((word for word in ("~@", "#'", "#_", "~", "'", "`", "@", "^") if self.text.startswith(word,
                                                                                                            self.at)),
                      None)
        if prefix:
            self.at += len(prefix)
            return Prefixed(prefix, self.next_form(None), line)
        if character == "\\":  # a character: \a, \newline, \( ...
            self.at += 2
            while self.at < len(self.text) and self.text[self.at] not in self.DELIMITERS:
                self.at += 1
            return Text(line, line)
        start = self.at
        while self.at < len(self.text) and self.text[self.at] not in self.DELIMITERS:
            self.at += 1
        return Atom(self.text[start:self.at], line)

    def items_of(self, form):
        """Reads a form's items up to its closer."""
        closer = self.CLOSERS[form.opener]
        while True:
            item = self.next_form(closer)
            if item is None:
                if self.text[self.at - 1:self.at] != closer:
                    raise ValueError(f"line {form.line}: {form.opener} is never closed")
                form.last = self.line
                return form
            form.items.append(item)

    def string(self, line):
        """Reads a string or a pattern."""
        self.at += 2 if self.text[self.at] == "#" else 1
        while self.at < len(self.text) and self.text[self.at] != '"':
            if self.text[self.at] == "\\":
                self.at += 1
            elif self.text[self.at] == "\n":
                self.line += 1
            self.at += 1
        self.at += 1
        return Text(line, self.line)


def is_call(form, *heads):
    """Whether a form is a call of one of the words given."""
    return isinstance(form, Form) and form.head() in heads


def written(form):
    """A form's words in order, to compare two forms (strings are all one word)."""
    if isinstance(form, Atom):
        return form.text
    if isinstance(form, Text):
        return '""'
    if isinstance(form, Prefixed):
        return form.prefix + written(form.form)
    return form.opener + " ".join(written(item) for item in form.items)


def compared_value(test):
    """The forms an `(= a b)` compares, as texts; None if the test is not one."""
    if is_call(test, "=") and len(test.arguments()) == 2:
        return {written(item) for item in test.arguments()}
    return None


def one_value(tests):
    """Whether two or more `(= a b)` tests all compare one value."""
    values = [compared_value(test) for test in tests]
    if len(values) < 2 or None in values:
        return False
    return bool(set.intersection(*values))


def cond_tests(form):
    """A `cond`'s tests, its default (`:else` or `true`) left out."""
    tests = form.arguments()[0::2]
    if tests and isinstance(tests[-1], Atom) and (tests[-1].text.startswith(":") or tests[-1].text == "true"):
        tests = tests[:-1]
    return tests


def tests_with(form, head, inner):
    """Whether a form is a call of `head` whose first argument is a call of `inner`: `(when (not ...` and the like."""
    return is_call(form, head) and len(form.items) > 1 and is_call(form.items[1], inner)


def is_one_line_fn(form):
    """A one-parameter `fn` written on one line."""
    if not is_call(form, "fn") or form.line != form.last or len(form.items) < 3:
        return False
    parameters = form.items[1]
    return (isinstance(parameters, Form) and parameters.opener == "[" and len(parameters.items) == 1
            and isinstance(parameters.items[0], Atom))


def is_by_one(form, head):
    """`(- x 1)` or `(+ x 1)` (or `(+ 1 x)`)."""
    if not is_call(form, head) or len(form.arguments()) != 2:
        return False
    ones = [isinstance(item, Atom) and item.text == "1" for item in form.arguments()]
    return ones[1] or (head == "+" and ones[0])


def is_count_zero(form):
    """`(= (count x) 0)`, either way round."""
    if not is_call(form, "=") or len(form.arguments()) != 2:
        return False
    first, second = form.arguments()
    return ((is_call(first, "count") and isinstance(second, Atom) and second.text == "0")
            or (is_call(second, "count") and isinstance(first, Atom) and first.text == "0"))


def is_camel_case_definition(form):
    """A function, a value or a view whose name has an upper-case letter: Clojure writes `cards-of`, not `cardsOf`."""
    return (is_call(form, "defn", "defn-", "def", "defview") and len(form.items) > 1 and isinstance(form.items[1], Atom)
            and any(character.isupper() for character in form.items[1].text))


def call_depth(form):
    """How deep calls nest in a form written on one line (a call is 1); 0 for anything else."""
    if not (isinstance(form, Form) and form.opener == "(" and form.line == form.last):
        return 0
    return 1 + max((call_depth(item) for item in form.items), default=0)


# Each rule: its name, what it finds, and whether it holds only in a function (not in a binding)
RULES = {
    "fn-one-parameter": (is_one_line_fn, False),
    "not-equal": (lambda form: tests_with(form, "not", "="), False),
    "when-not": (lambda form: tests_with(form, "when", "not"), False),
    "if-not": (lambda form: tests_with(form, "if", "not"), False),
    "if-nil": (lambda form: tests_with(form, "if", "nil?"), False),
    "when-some": (lambda form: tests_with(form, "when", "some?"), False),
    "cond-one-value": (lambda form: is_call(form, "cond") and one_value(cond_tests(form)), False),
    "or-equals": (lambda form: is_call(form, "or") and one_value(form.arguments()), True),
    "contains-vector": (lambda form: is_call(form, "contains") and len(form.items) > 1
                        and isinstance(form.items[1], Form) and form.items[1].opener == "[", True),
    "minus-one": (lambda form: is_by_one(form, "-"), False),
    "plus-one": (lambda form: is_by_one(form, "+"), False),
    "count-zero": (is_count_zero, False),
    "string-function": (lambda form: is_call(form, *STRING_FUNCTIONS), True),
    "filter-some-map": (lambda form: is_call(form, "filter") and len(form.items) > 2 and isinstance(form.items[1], Atom)
                        and form.items[1].text == "some?" and is_call(form.items[2], "map"), True),
    "four-nested-calls": (lambda form: call_depth(form) >= 4, False),
    "camel-case-name": (is_camel_case_definition, False),
    "entry-include": (lambda form: False, False),  # top level only: counted by count_file
}


def walk(form, binding, top, visit):
    """Visits each form under one, with whether it is in a binding and whether its parent counts four nested calls."""
    if isinstance(form, Prefixed):
        if form.form is not None:
            walk(form.form, binding or form.prefix in BINDING_PREFIXES, top, visit)
        return
    if not isinstance(form, Form):
        return
    visit(form, binding, top)
    inner = binding or form.head() == "defcomputed"
    nested = call_depth(form) >= 4
    for item in form.items:
        walk(item, inner, not nested, visit)


def count_text(text, place):
    """The places of each rule in a Lisp text, and the kept ones: ([(rule, line)], [(rule, line, reason)])."""
    reader = Reader(text)
    try:
        forms = reader.forms()
    except ValueError as error:
        raise ValueError(f"{place}: {error}") from error
    found, kept = [], []

    def noted(rule, line):
        """Counts a place, or lists it as kept when the line before marks it."""
        if line in reader.kept:
            kept.append((rule, line, reader.kept[line]))
        else:
            found.append((rule, line))

    def visit(form, binding, top):
        """Notes each rule a form matches."""
        for rule, (finds, function_only) in RULES.items():
            if function_only and binding:
                continue
            if rule == "four-nested-calls" and not top:
                continue  # a nest is counted once, at its outermost call
            if finds(form):
                noted(rule, form.line)

    for form in forms:
        if is_call(form, "include"):
            noted("entry-include", form.line)
        walk(form, False, True, visit)
    found += [(NO_REASON, line) for line in reader.unreasoned]
    return found, kept


def code_files(folder):
    """The code files under a folder of examples/ (`x/*`: its own files only), as paths from examples/."""
    own = folder.endswith("/*")
    base = os.path.join(EXAMPLES, folder[:-2] if own else folder)
    found = []
    for place, folders, names in os.walk(base):
        folders[:] = [] if own else sorted(name for name in folders if name not in DATA_FOLDERS)
        for name in names:
            path = os.path.relpath(os.path.join(place, name), EXAMPLES)
            if name.endswith(".lisp") and lisp_examples.is_program_file(path):
                found.append(path)
    return sorted(found)


def every_code_file():
    """Every code file of the examples written as programs and of examples/shared/."""
    apps = [name for name in os.listdir(EXAMPLES) if os.path.isdir(os.path.join(EXAMPLES, name, "lisp"))]
    return sorted({path for app in apps for path in code_files(app + "/lisp")} | set(code_files("shared")))


def groups():
    """The idioms files, by name: {"folders": [...], "allowed": {...}}."""
    found = {}
    for path in sorted(glob.glob(os.path.join(IDIOMS, "*.json"))):
        with open(path, encoding="utf-8") as file:
            found[os.path.basename(path)[:-len(".json")]] = json.load(file)
    return found


def files_of(group):
    """A group's code files."""
    return sorted({path for folder in group["folders"] for path in code_files(folder)})


def counted(paths):
    """Each rule's count over files, and the kept places, as ({rule: n}, [(path, line, rule, reason)])."""
    totals, kept = dict.fromkeys([*RULES, NO_REASON], 0), []
    for path in paths:
        with open(os.path.join(EXAMPLES, path), encoding="utf-8") as file:
            found, marked = count_text(file.read(), path)
        for rule, _ in found:
            totals[rule] += 1
        kept += [(path, line, rule, reason) for rule, line, reason in marked]
    return totals, kept


def kept_counts(kept):
    """Each rule's count of kept places, from counted()'s list: {rule: n}, the rules with none left out."""
    counts = {}
    for _, _, rule, _ in kept:
        counts[rule] = counts.get(rule, 0) + 1
    return dict(sorted(counts.items()))


def count_failures(name, totals, allowed):
    """A group's rules whose count is off what its idioms file allows (a spelling is allowed none): the failures."""
    failures = []
    for rule, count in totals.items():
        expected = allowed.get(rule, 0) if rule in SHAPE_RULES else 0
        if count != expected:
            failures.append(f"{name}: {rule} counts {count}, {expected} expected "
                            f"({'a mark needs its reason' if rule == NO_REASON else 'style.md writes it another way'}"
                            f"{', or lower the count in its idioms file' if count < expected else ''})")
    return failures


def group_failures(name, group):
    """What breaks a group's lock: a spelling found unmarked, a shape off its count, kept places off theirs."""
    failures = [f"{name}: {folder} is no folder of examples/" for folder in group["folders"]
                if not os.path.isdir(os.path.join(EXAMPLES, folder.removesuffix("/*")))]
    totals, kept = counted(files_of(group))
    allowed, marked = group.get("allowed", {}), kept_counts(kept)
    for rule in (rule for rule in allowed if rule not in SHAPE_RULES):
        failures.append(f"{name}: {rule} is a spelling: it may not be allowed, only kept with a reason")
    failures += count_failures(name, totals, allowed)
    for rule in sorted(set(marked) | set(group.get("kept", {}))):
        if marked.get(rule, 0) != group.get("kept", {}).get(rule, 0):
            failures.append(f"{name}: {rule} has {marked.get(rule, 0)} kept, the idioms file says "
                            f"{group.get('kept', {}).get(rule, 0)} (a kept place is reviewed with its file)")
    return failures


def check():
    """Fails unless every group is at its lock (group_failures), and every code file belongs to exactly one group,
    so a new example's files fail until it has an idioms file; the exit code."""
    failures, owners = [], {}
    for name, group in groups().items():
        for path in files_of(group):
            owners.setdefault(path, []).append(name)
        failures += group_failures(name, group)
    for path in every_code_file():
        if len(owners.get(path, [])) != 1:
            failures.append(f"{path}: counted by {owners.get(path) or 'no idioms file'}, exactly one expected")
    for failure in failures:
        print(failure)
    print(f"lisp_idioms: {len(groups())} files of rules, {len(owners)} code files, {len(failures)} failures")
    return 1 if failures else 0


def write():
    """Writes today's counts into the idioms files; the exit code."""
    for name, group in groups().items():
        totals, kept = counted(files_of(group))
        group["allowed"] = {rule: count for rule, count in totals.items() if count and rule in SHAPE_RULES}
        group["kept"] = kept_counts(kept)
        with open(os.path.join(IDIOMS, name + ".json"), "w", encoding="utf-8") as file:
            file.write(json.dumps(group, indent="\t") + "\n")
        print(f"{name}: {sum(totals.values())} places")
    return 0


def print_counts(names):
    """Prints each rule's count per group, beside what it allows; the exit code."""
    for name, group in groups().items():
        if names and name not in names:
            continue
        totals, _ = counted(files_of(group))
        allowed = group.get("allowed", {})
        print(f"{name}: " + ", ".join(f"{rule} {count}/{allowed.get(rule, 0)}"
                                      for rule, count in totals.items() if count or allowed.get(rule)))
    return 0


def print_kept():
    """Prints the places marked to stay; the exit code."""
    _, kept = counted(every_code_file())
    for path, line, rule, reason in kept:
        print(f"examples/{path}:{line}: {rule}: {reason}")
    print(f"{len(kept)} places kept")
    return 0


def token_counter():
    """A function counting a text's tokens as tests/perf/tokens.py does, or None if tiktoken isn't installed."""
    try:
        import tiktoken  # pylint: disable=import-outside-toplevel  # optional: only for the tokens
    except ImportError:
        return None
    encoding = tiktoken.get_encoding("o200k_base")
    return lambda text: len(encoding.encode(text, disallowed_special=()))


def measures():
    """Code files, lines, bytes and tokens (None if not counted) per app, the apps named after their folder."""
    tokens = token_counter()
    apps = {}
    for path in every_code_file():
        app = path.split(os.sep)[0]
        with open(os.path.join(EXAMPLES, path), "rb") as file:
            content = file.read()
        sizes = apps.setdefault(app, {"files": 0, "lines": 0, "bytes": 0, "tokens": 0 if tokens else None})
        sizes["files"] += 1
        sizes["lines"] += content.count(b"\n")
        sizes["bytes"] += len(content)
        if tokens:
            sizes["tokens"] += tokens(content.decode("utf-8"))
    total = {key: (None if tokens is None and key == "tokens" else sum(sizes[key] for sizes in apps.values()))
             for key in ("files", "lines", "bytes", "tokens")}
    return apps, total


def measure(as_json):
    """Prints the measures; the exit code."""
    apps, total = measures()
    if as_json:
        print(json.dumps({"apps": apps, "total": total}, indent=1))
        return 0
    print(f"{'app':12} {'files':>6} {'lines':>7} {'bytes':>9} {'tokens':>8}")
    for name, sizes in [*apps.items(), ("total", total)]:
        counted_tokens = "not counted" if sizes["tokens"] is None else str(sizes["tokens"])
        print(f"{name:12} {sizes['files']:6} {sizes['lines']:7} {sizes['bytes']:9} {counted_tokens:>8}")
    if total["tokens"] is None:
        print("tokens: not counted (tiktoken is not installed: pip install -r tests/perf/requirements.txt)")
    return 0


# The self-test: each rule on a snippet that has it and one that doesn't
SNIPPETS = {
    "fn-one-parameter": ("(defn f [xs] (filter (fn [it] (= it.tag 1)) xs))",
                         "(defn f [xs] (map-indexed (fn [i it] i) xs))"),
    "not-equal": ("(defn f [a b] (not (= a b)))", "(defn f [a b] (not= a b))"),
    "when-not": ("(defn f [a] (when (not a) 1))", "(defn f [a] (when-not a 1))"),
    "if-not": ("(defn f [a] (if (not a) 1 2))", "(defn f [a] (if-not a 1 2))"),
    "if-nil": ("(defn f [a] (if (nil? a) 1 2))", "(defn f [a] (if-some [b a] 2 1))"),
    "when-some": ("(defn f [a] (when (some? a) 1))", "(defn f [a] (when-some [b a] 1))"),
    "cond-one-value": ("(defn f [a]\n  (cond (= a :x) 1\n        (= :y a) 2\n        :else 3))",
                       "(defn f [a b]\n  (cond (= a :x) 1\n        (= b :y) 2\n        :else 3))"),
    "or-equals": ("(defn f [a] (or (= a :x) (= a :y)))", "(defn f [a] (#{:x :y} a))"),
    "contains-vector": ("(defn f [a] (contains [1 2] a))", "(defn f [a] (contains? #{1 2} a))"),
    "minus-one": ("(defn f [n] (- n 1))", "(defn f [n] (dec n))"),
    "plus-one": ("(defn f [n] (+ 1 n))", "(defn f [n] (+ n 2))"),
    "count-zero": ("(defn f [xs] (= (count xs) 0))", "(defn f [xs] (empty? xs))"),
    "string-function": ("(defn f [s] (trim s))", "(defn f [s] (str/trim s))"),
    "filter-some-map": ("(defn f [xs] (filter some? (map g xs)))", "(defn f [xs] (keep g xs))"),
    "four-nested-calls": ("(defn f [x] (a (b (c (d x)))))", "(defn f [x]\n  (a (b (c\n           (d x)))))"),
    "camel-case-name": ("(defn cardsOf [c] c)", "(defn cards-of [c] c)"),
    "entry-include": ('(include "a.lisp")', '(ns app.index (:require [app.a]))'),
}


def counts_of(text, rule):
    """How many places of a rule a text has."""
    return sum(1 for found, _ in count_text(text, "snippet")[0] if found == rule)


def snippet_failures():
    """Each rule's snippets counted where the rule is written, and not where it isn't, nor in a string or a comment,
    nor (for a rule of functions) in a binding, nor where it is marked to stay: the failures."""
    failures = []
    for rule, (has, has_not) in SNIPPETS.items():
        cases = [(has, 1), (has_not, 0), ('(def s "' + has.replace('"', '\\"') + '")', 0),
                 (";; " + has.replace("\n", "\n;; "), 0), (marked_to_stay(has, rule), 0)]
        if RULES[rule][1]:
            cases.append(("(defcomputed c " + has + ")", 0))
            cases.append(("(defview v [] `(node :text ~" + has + "))", 0))
        for text, expected in cases:
            if counts_of(text, rule) != expected:
                failures.append(f"{rule}: {counts_of(text, rule)} found, {expected} expected, in: {text!r}")
    return failures


def self_test():
    """The counter's own tests: each rule found where it is written, not where it isn't, nor in a string or a comment,
    nor (for a rule of functions) in a binding, nor where it is marked to stay; the exit code."""
    failures = snippet_failures()
    if counts_of("(defn f [x] (a (b (c (d (e x))))))", "four-nested-calls") != 1:
        failures.append("four-nested-calls: a deeper nest is counted more than once")
    if counts_of("(defn f [c] (= c \\( ))", "count-zero") != 0 or counts_of('#"(not (= a b))"', "not-equal"):
        failures.append("a character or a pattern is read as code")
    if counted_kept("; style: keep, the parameter is read as text\n(defn f [n] (- n 1))") != [
            ("minus-one", 2, "the parameter is read as text")]:
        failures.append("a kept place is not listed with its reason")
    for mark in ("; style: keep", ";; style: keep,", "; style: keep ,  "):
        text = mark + "\n(defn f [n] (- n 1))"
        if counts_of(text, NO_REASON) != 1 or counts_of(text, "minus-one") != 1:
            failures.append(f"a mark with no reason is not refused, or keeps its place: {mark!r}")
    failures += lock_failures()
    for failure in failures:
        print(failure)
    print(f"lisp_idioms --self-test: {len(SNIPPETS)} rules, {len(failures)} failures")
    return 1 if failures else 0


# What the lock says of one app's file, against its idioms file: the code, the group, and how many failures
LOCK_CASES = [
    ("(defn f [n] (dec n))", {"allowed": {}, "kept": {}}, 0),
    ("(defn f [n] (- n 1))", {"allowed": {}, "kept": {}}, 1),  # an old spelling
    ("(defn f [n] (- n 1))", {"allowed": {"minus-one": 1}, "kept": {}}, 2),  # a spelling is never allowed
    ("; style: keep, a reason\n(defn f [n] (- n 1))", {"allowed": {}, "kept": {"minus-one": 1}}, 0),
    ("; style: keep, a reason\n(defn f [n] (- n 1))", {"allowed": {}, "kept": {}}, 1),  # a kept place unseen
    ("(defn f [n] (dec n))", {"allowed": {}, "kept": {"minus-one": 1}}, 1),  # a kept place gone
    ("(defn f [x] (a (b (c (d x)))))", {"allowed": {"four-nested-calls": 1}, "kept": {}}, 0),
    ("(defn f [x] (a (b (c (d x)))))", {"allowed": {}, "kept": {}}, 1),  # a new nest
    ("(defn f [x] x)", {"allowed": {"four-nested-calls": 1}, "kept": {}}, 1),  # a nest gone: lower the count
]


def lock_failures():
    """The lock's own tests, on an examples/ folder of one file made for each (group_failures above)."""
    global EXAMPLES  # pylint: disable=global-statement  # pointed at the made folder, then restored
    failures, saved = [], (EXAMPLES, lisp_examples.EXAMPLES)
    try:
        for text, group, expected in LOCK_CASES:
            with tempfile.TemporaryDirectory() as folder:
                os.makedirs(os.path.join(folder, "app", "lisp"))
                with open(os.path.join(folder, "app", "lisp", "a.lisp"), "w", encoding="utf-8") as file:
                    file.write(";;; a.lisp --- a test\n\n" + text + "\n")
                EXAMPLES = lisp_examples.EXAMPLES = folder
                found = group_failures("app", {"folders": ["app/lisp"], **group})
                if len(found) != expected:
                    failures.append(f"the lock: {len(found)} failures, {expected} expected, for {text!r} and {group}")
    finally:
        EXAMPLES, lisp_examples.EXAMPLES = saved
    if not group_failures("app", {"folders": ["no-such-folder/lisp"], "allowed": {}}):
        failures.append("the lock: a folder that isn't there passes")
    return failures


def marked_to_stay(text, rule):
    """A text with `; style: keep` on the line before the place of a rule."""
    lines = text.split("\n")
    line = next(found for name, found in count_text(text, "snippet")[0] if name == rule)
    return "\n".join([*lines[:line - 1], "; style: keep, a reason", *lines[line - 1:]])


def counted_kept(text):
    """The kept places of a text."""
    return count_text(text, "snippet")[1]


ACTIONS = ("check", "count", "kept", "measure", "write")


def parse_arguments():
    """The arguments, checked: one action, or --self-test alone or with --check."""
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    action = parser.add_mutually_exclusive_group()
    for flag, words in (("--check", "fail if a rule counts more than allowed"), ("--count", "print each rule's count"),
                        ("--kept", "print the places marked to stay"), ("--measure", "print the size of each app"),
                        ("--write", "write today's counts as what is allowed")):
        action.add_argument(flag, action="store_true", help=words)
    parser.add_argument("--self-test", action="store_true", help="test the counter (first, with --check)")
    parser.add_argument("names", nargs="*", help="--count: the idioms files (none: every one)")
    parser.add_argument("--json", action="store_true", help="--measure: print one JSON object")
    arguments = parser.parse_args()
    chosen = {name for name in ACTIONS if getattr(arguments, name)}
    if not arguments.self_test and not chosen:
        parser.error("one of --check, --count, --kept, --measure, --write or --self-test is needed")
    if arguments.self_test and chosen - {"check"}:
        parser.error("--self-test goes alone or with --check")
    return arguments


def run(arguments):
    """Runs the action the arguments ask; returns the exit code."""
    if arguments.self_test:
        failed = self_test()
        if failed or not arguments.check:
            return failed
    actions = {"check": check, "count": lambda: print_counts(arguments.names), "kept": print_kept, "write": write}
    name = next((name for name in actions if getattr(arguments, name)), None)
    return actions[name]() if name else measure(arguments.json)


def main():
    """Runs what the arguments ask; returns the exit code."""
    arguments = parse_arguments()
    try:
        return run(arguments)
    except ValueError as error:
        print(f"lisp_idioms: {error}")
        return 2


if __name__ == "__main__":
    sys.exit(main())
