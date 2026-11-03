#!/usr/bin/env python3
"""Writes a first draft of a data file of an app as a Lisp program (SCREEN_FORMAT.md, "Writing an app as a program").

A data file of an example's lisp/ folder (a screen or a part it includes: one map, the JSON tree in parentheses) becomes
definitions: each section's members their own forms (`defstate`, `defcomputed`, `defstyle`, `defcomponent`, `defmenu`,
`deftokens`, `defkey`, `defcustom`), the root a `defscreen main` that calls small views (`defview`), each repeat a
`repeat`, and every `{{...}}` text the Lisp form it means (`affrescoui lisp-binding`). What it doesn't know how to
write as a definition (actions as steps, watch, channels...) goes in a `defpart`, for a person to make functions of.

It is a draft: the views are cut where the tree is long, named after their nodes' ids, and their docstrings say only
that; a person names them, writes what they are for, turns step maps into functions and puts the text in the locales.
The draft builds to the same tree as the data file, so the app draws the same (`ctest -R lisp_parity`).

    python3 tools/notation/lisp_program.py <file.lisp> [--entry] [--lines 15] [-o out.lisp] [--cli build/affrescoui]

--entry writes the screen's own sections (window, fonts, strings, persist, permissions, theme) as defscreen main's
options, and its includes as an include form; without it, they go in a defpart. A text it can't translate stays as
written, and is listed on standard error. Only the standard library, and the built affrescoui program.
"""

import argparse
import json
import os
import re
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# The screen's own sections, defscreen main's options in the entry
SCREEN_OPTIONS = ("window", "strings", "fonts", "persist", "permissions", "theme")
# The sections written one definition per member, and the form that defines each
MEMBER_FORMS = {"state": "defstate", "computed": "defcomputed", "styles": "defstyle", "components": "defcomponent",
                "menus": "defmenu"}
# What the language has: a view isn't named so
TAKEN = set()


class Draft:
    """What is being written: the forms, the texts to translate, and the views cut out."""

    def __init__(self, cli, lines, avoid):
        self.cli = cli
        self.lines = lines
        self.names = set(avoid)
        self.views = []
        self.untranslated = []
        self.counter = 0

    def marker(self):
        """A word the printer writes as it is, replaced afterwards."""
        self.counter += 1
        return f"@@m{self.counter}@@"


def run(command, given=None):
    """Runs the affrescoui program; its output."""
    done = subprocess.run(command, input=given, capture_output=True, text=True)
    if done.returncode != 0:
        sys.exit(f"lisp_program: {' '.join(command[:2])}: {done.stderr.strip()}")
    return done.stdout


def printed(draft, value):
    """A value in the Lisp notation, as the canonical printer writes it."""
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False, encoding="utf-8") as file:
        json.dump(value, file, ensure_ascii=False)
    try:
        return run([draft.cli, "convert", file.name, "--to", "lisp"]).rstrip("\n")
    finally:
        os.remove(file.name)


def translations(draft, texts):
    """Each text's Lisp form, or None where it can't be translated."""
    if not texts:
        return {}
    answers = json.loads(run([draft.cli, "lisp-binding", "--batch"], json.dumps(texts)))
    return {text: answer.get("form") for text, answer in zip(texts, answers)}


def texts_in(value, found):
    """Every text with `{{` in a value, notes left out."""
    if isinstance(value, str):
        if "{{" in value:
            found.add(value)
    elif isinstance(value, list):
        for item in value:
            texts_in(item, found)
    elif isinstance(value, dict):
        for key, item in value.items():
            if key != "note":
                texts_in(item, found)


def kebab(name):
    """`topBar` as `top-bar`."""
    return re.sub(r"(?<=[a-z0-9])([A-Z])", r"-\1", name).lower().replace("_", "-")


def view_name(draft, node, parent):
    """A name for a view of a node, which nothing else has."""
    base = kebab(node.get("id") or f"{parent}-{node.get('type', 'node')}")
    name = base if base not in draft.names and base not in TAKEN else base + "-view"
    number = 2
    while name in draft.names or name in TAKEN:
        name, number = f"{base}-{number}", number + 1
    draft.names.add(name)
    return name


def uses(form_texts, name):
    """Whether one of some forms reads a name: `card`, `card.title`."""
    pattern = re.compile(r"(?<![\w.\-@$])" + re.escape(name) + r"(?=[.\s)\]}]|$)")
    return any(pattern.search(text) for text in form_texts)


class Writer:
    """Writes values as templates: markers for what is code, filled in once printed."""

    def __init__(self, draft, forms):
        self.draft = draft
        self.forms = forms
        self.fills = {}

    def value(self, value, key=None):
        """A value with each text that has `{{` replaced by a marker for its form."""
        if isinstance(value, str):
            if key == "note" or "{{" not in value:
                return value
            form = self.forms.get(value)
            if form is None:
                self.draft.untranslated.append(value)
                return value
            mark = self.draft.marker()
            self.fills[mark] = "~" + form
            return mark
        if isinstance(value, list):
            return [self.value(item) for item in value]
        if isinstance(value, dict):
            return {name: self.value(item, name) for name, item in value.items()}
        return value

    def node(self, node, scope, parent, depth):
        """A node as a template: its children cut into views where they are long, its repeats `repeat` forms."""
        out = {}
        for name, item in node.items():
            if name == "children" and isinstance(item, list):
                out[name] = [self.child(child, scope, node.get("id") or parent, depth) for child in item]
            else:
                out[name] = self.value(item, name)
        return out

    def child(self, child, scope, parent, depth):
        """A child: as it is, or a marker for a repeat or a view."""
        if not isinstance(child, dict):  # a child is a node, with a type or without
            return self.value(child)
        repeat = child.get("repeat")
        if isinstance(repeat, dict) and "for" in repeat and "in" in repeat:
            return self.repeated(child, repeat, scope, parent, depth)
        return self.maybe_view(child, scope, parent, depth)

    def repeated(self, child, repeat, scope, parent, depth):
        """`~(repeat [item list :index i :key k] body)`."""
        body = {name: item for name, item in child.items() if name != "repeat"}
        item, index = repeat["for"], repeat.get("index")
        inner = scope + [item] + ([index] if index else [])
        text = self.text_of(self.node(body, inner, parent, depth + 1))
        source = repeat["in"]
        if isinstance(source, str) and "{{" not in source and not re.fullmatch(r"[A-Za-z_][\w.]*", source):
            source = "{{" + source + "}}"  # an expression without its braces
        source_form = self.forms.get(source) if isinstance(source, str) and "{{" in source else None
        if isinstance(source, str) and "{{" in source and source_form is None:
            self.draft.untranslated.append(source)
        source_text = source_form or (source if isinstance(source, str) else json.dumps(source))
        options = f" :index {index}" if index else ""
        if "key" in repeat:
            key = repeat["key"]
            translated = self.forms.get(key) if isinstance(key, str) and "{{" in key else None
            options += " :key " + (translated or json.dumps(key))
        used = [name for name in inner if uses([text], name)]
        call = self.view_call(body, text, used, parent) if len(text.split("\n")) > self.draft.lines else None
        mark = self.draft.marker()
        self.fills[mark] = f"~(repeat [{item} {source_text}{options}] " + (call or "`" + text) + ")"
        return mark

    def maybe_view(self, child, scope, parent, depth):
        """A child as it is, or, if it is long, a call of a view cut out of it."""
        text = self.text_of(self.node(child, scope, parent, depth + 1))
        if len(text.split("\n")) <= self.draft.lines:
            return self.reparse(text)
        used = [name for name in scope if uses([text], name)]
        mark = self.draft.marker()
        self.fills[mark] = "~" + self.view_call(child, text, used, parent)
        return mark

    def view_call(self, node, text, params, parent):
        """Cuts a view out: its definition is kept, and its call returned."""
        name = view_name(self.draft, node, parent)
        doc = node.get("note") if isinstance(node.get("note"), str) else f"#{node.get('id', name)}."
        body = "\n".join(("  `" if at == 0 else "   ") + line for at, line in enumerate(text.split("\n")))
        self.draft.views.append(f"(defview {name} {json.dumps(doc, ensure_ascii=False)} [{' '.join(params)}]\n"
                                f"{body})")
        return f"({name}{''.join(' ' + param for param in params)})"

    def text_of(self, value):
        """A template's text, its markers filled."""
        return fill(printed(self.draft, value), self.fills)

    def reparse(self, text):
        """A child already written as text: a marker for it."""
        mark = self.draft.marker()
        self.fills[mark] = text
        return mark


def fill(text, fills):
    """Puts each marker's text where it is, indenting its later lines under the marker's column."""
    pattern = re.compile(r"@@m\d+@@")
    while True:
        found = pattern.search(text)
        if not found:
            return text
        filling = fills.get(found.group(0), found.group(0).strip("@"))
        column = found.start() - (text.rfind("\n", 0, found.start()) + 1)
        filling = filling.replace("\n", "\n" + " " * column)
        text = text[:found.start()] + filling + text[found.end():]


def options_text(writer, options):
    """`:key value` pairs, one a line when they don't fit on one."""
    pairs = []
    for key, value in options.items():
        name = key if re.fullmatch(r"[A-Za-z_$@][\w.$@-]*", key) else json.dumps(key)
        text = writer.text_of(writer.value(value, key))
        pairs.append(f":{name} " + text.replace("\n", "\n" + " " * (len(name) + 2)))
    one = " ".join(pairs)
    return one if "\n" not in one and len(one) < 100 else "\n".join(pairs)


def literal_code(draft, value):
    """A value as code that gives it: text quoted, a list or a map quoted as data."""
    if isinstance(value, (list, dict)):
        return "'" + printed(draft, value)
    return json.dumps(value, ensure_ascii=False)


def definition(head, name, rest, doc=None):
    """`(head name "doc" rest)`, its later lines indented two spaces."""
    lines = rest.split("\n")
    first = f"({head} {name}" + (f" {json.dumps(doc, ensure_ascii=False)}" if doc else "")
    if len(lines) == 1 and len(first) + len(rest) < 118:
        return f"{first} {rest})"
    return first + "\n" + "\n".join("  " + line for line in lines) + ")"


def member_forms(draft, writer, document):
    """The definitions of the sections written member by member, keys and settings."""
    out = []
    types = document.get("stateTypes", {})
    for section, head in MEMBER_FORMS.items():
        for name, value in document.get(section, {}).items():
            if section == "state":
                text = writer.text_of(writer.value(value))
                if name in types:
                    text += " :type " + printed(draft, types[name])
                out.append(definition(head, name, text))
            elif section == "computed":
                form = writer.forms.get(value) if isinstance(value, str) and "{{" in value else None
                if form is None and isinstance(value, str) and "{{" in value:
                    draft.untranslated.append(value)
                out.append(definition(head, name, form or writer.text_of(writer.value(value))))
            elif section == "components" and isinstance(value, dict) and set(value) <= {"note", "params", "node"}:
                params = writer.text_of(writer.value(value.get("params", {})))
                node = writer.text_of(writer.node(value.get("node", {}), list(value.get("params", {})), name, 1))
                body = params + "\n`" + node.replace("\n", "\n ")
                out.append(definition(head, name, body, value.get("note") or f"The {name} component."))
            elif section == "components":
                out.append(definition("defpart", name, ":components " + writer.text_of(writer.value({name: value}))))
            else:
                out.append(definition(head, name, writer.text_of(writer.value(value))))
    if document.get("tokens"):
        out.append(definition("deftokens", "tokens", writer.text_of(writer.value(document["tokens"])), "Tokens."))
    for chord, what in document.get("keys", {}).items():
        out.append(definition("defkey", json.dumps(chord), writer.text_of(writer.value(what))))
    for name, setting in document.get("settings", {}).items():
        options = {key: item for key, item in setting.items() if key not in ("default", "description")}
        text = literal_code(draft, setting.get("default"))
        if setting.get("description"):
            text += " " + json.dumps(setting["description"], ensure_ascii=False)
        if options:
            text += ("\n" if len(text) > 60 else " ") + options_text(writer, options)
        out.append(definition("defcustom", name, text))
    return out


def draft_program(path, entry, cli, lines):
    """The program text for a data file."""
    document = json.loads(run([cli, "convert", path, "--to", "json"]))
    folder = os.path.dirname(os.path.abspath(path))
    avoid = set()
    for name in os.listdir(folder):
        if name.endswith(".lisp"):
            with open(os.path.join(folder, name), encoding="utf-8") as file:
                avoid.update(re.findall(r"^\((?:def\w*) ([^\s()]+)", file.read(), re.M))
    draft = Draft(cli, lines, avoid)
    texts = set()
    texts_in({key: value for key, value in document.items() if key != "note"}, texts)
    for value in document.get("root", {}), *(document.get("components", {}).values()):
        found = []
        collect_repeats(value, found)
        texts.update(source if "{{" in source else "{{" + source + "}}" for source in found)
    forms = translations(draft, sorted(texts))
    for form in forms.values():  # a view isn't named as a name a binding reads: it would stand for it
        draft.names.update(word.split(".")[0] for word in re.findall(r"[A-Za-z_$][\w.$?-]*", form or ""))
    writer = Writer(draft, forms)
    out = member_forms(draft, writer, document)
    handled = set(MEMBER_FORMS) | {"stateTypes", "tokens", "keys", "settings", "note", "root", "screens", "$schema",
                                   "format", "version", "include"}
    screen_options = SCREEN_OPTIONS if entry else ()
    rest = {key: value for key, value in document.items() if key not in handled and key not in screen_options}
    stem = os.path.basename(path).split(".")[0]
    if rest:
        out.append(definition("defpart", stem, options_text(writer, rest), f"What {stem} adds."))
    screens = []
    if "root" in document:
        options = {key: document[key] for key in screen_options if key in document}
        root = writer.text_of(writer.node(document["root"], [], "root", 0))
        option_text = options_text(writer, options) if options else ""
        screens.append(definition("defscreen", "main", (option_text + "\n" if option_text else "") + root))
    for name, screen in document.get("screens", {}).items():
        screens.append(definition("defscreen", name, writer.text_of(writer.node(screen, [], name, 0))))
    note = document.get("note", "")
    header = [f";;; {os.path.basename(path)} --- {note.split('. ')[0] if note else 'a part of the app'}"]
    if ". " in note:
        header += [";;;"] + [";;; " + line for line in wrap(note.split(". ", 1)[1], 114)]
    includes = document.get("include", [])
    includes = [includes] if isinstance(includes, str) else includes
    parts = ["\n".join(header)]
    if includes:
        parts.append("(include " + "\n         ".join(json.dumps(name) for name in includes) + ")")
    parts += [";;; Definitions\n\n" + "\n\n".join(out)] if out else []
    parts += [";;; Views\n\n" + "\n\n".join(draft.views)] if draft.views else []
    parts += [";;; The screen\n\n" + "\n\n".join(screens)] if screens else []
    return "\n\n".join(parts) + "\n", draft.untranslated


def collect_repeats(value, found):
    """Every repeat's list, to translate."""
    if isinstance(value, dict):
        repeat = value.get("repeat")
        if isinstance(repeat, dict) and isinstance(repeat.get("in"), str):
            found.append(repeat["in"])
        if isinstance(repeat, dict) and isinstance(repeat.get("key"), str) and "{{" in repeat["key"]:
            found.append(repeat["key"])
        for item in value.values():
            collect_repeats(item, found)
    elif isinstance(value, list):
        for item in value:
            collect_repeats(item, found)


def wrap(text, width):
    """Lines of at most a width."""
    lines, line = [], ""
    for word in text.split():
        if line and len(line) + 1 + len(word) > width:
            lines.append(line)
            line = word
        else:
            line = f"{line} {word}" if line else word
    return lines + ([line] if line else [])


def main():
    """Writes the draft; returns the exit code."""
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("file")
    parser.add_argument("--entry", action="store_true", help="the app's entry screen")
    parser.add_argument("--lines", type=int, default=15, help="a child longer than this becomes a view")
    parser.add_argument("-o", dest="out", help="write here, else to standard output")
    parser.add_argument("--cli", default=os.path.join(ROOT, "build", "affrescoui"))
    arguments = parser.parse_args()
    with open(os.path.join(ROOT, "modules", "notation", "include", "affresco", "notation", "Words.hpp"),
              encoding="utf-8") as words:
        block = words.read().split("languageWords")[1].split("});")[0]
    TAKEN.update(re.findall(r'"([^"]+)"', block))
    TAKEN.update(("main", "list", "nil?", "some?", "empty?", "every?", "str", "t", "state", "computed", "event", "it", "index"))
    text, untranslated = draft_program(arguments.file, arguments.entry, arguments.cli, arguments.lines)
    if arguments.out:
        with open(arguments.out, "w", encoding="utf-8", newline="\n") as file:
            file.write(text)
    else:
        sys.stdout.write(text)
    for value in untranslated:
        print(f"left as written: {value}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
