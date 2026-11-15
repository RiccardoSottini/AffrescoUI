"""What the person pointed at in the app (pick records, SCREEN_FORMAT.md, "Peers and the live protocol"): cleaned to the
keys of a record, each of its type, so a request can't smuggle anything else to the agent; and told in plain lines."""

import unicodedata

LONGEST_TEXT = 512  # bytes of any text in a record: an id, a pointer, a binding as written, an address
MOST_EACH = 16  # bindings and handlers a record keeps, each, as the engine makes it
MOST_REPEATS = 4  # repeats a record keeps, the innermost ones, as the engine makes it


def text_of(value):
    """The value if it is a text, cut to LONGEST_TEXT bytes of UTF-8 (never inside a character); else None."""
    if not isinstance(value, str):
        return None
    return value.encode("utf-8")[:LONGEST_TEXT].decode("utf-8", "ignore")


def number_of(value):
    """The value if it is a number (a bool is not one); else None."""
    return value if isinstance(value, (int, float)) and not isinstance(value, bool) else None


def whole_of(value):
    """The value if it is a whole number (a bool is not one); else None."""
    return value if isinstance(value, int) and not isinstance(value, bool) else None


def flag_of(value):
    """The value if it is true or false; else None."""
    return value if isinstance(value, bool) else None


def bounds_of(value):
    """The value if it is four numbers (x, y, width, height); else None."""
    if not isinstance(value, list) or len(value) != 4 or any(number_of(n) is None for n in value):
        return None
    return value


def bindings_of(value):
    """At most MOST_EACH of an object's members whose values are texts: the bindings as written, never a value shown."""
    if not isinstance(value, dict):
        return None
    return {text_of(key): text_of(text) for key, text in list(value.items())[:MOST_EACH] if isinstance(text, str)}


def handlers_of(value):
    """At most MOST_EACH texts of a list: the events the element answers."""
    if not isinstance(value, list):
        return None
    return [text_of(name) for name in value[:MOST_EACH] if isinstance(name, str)]


def fields_of(value, kinds):
    """The members of an object that `kinds` names, each kept only if its reader gives a value; None if not an object.

    @param value  the object
    @param kinds  {key: reader}, a reader taking the member and giving its clean value or None
    """
    if not isinstance(value, dict):
        return None
    kept = {key: read(value[key]) for key, read in kinds.items() if key in value}
    return {key: clean for key, clean in kept.items() if clean is not None}


def repeat_of(value):
    """The innermost MOST_REPEATS repeats an element is inside, outermost first: each one's variable, list as written
    and index."""
    if not isinstance(value, list):
        return None
    kinds = {"for": text_of, "in": text_of, "index": whole_of}
    return [fields_of(item, kinds) for item in value[-MOST_REPEATS:] if isinstance(item, dict)]


def component_of(value):
    """The component an element is inside: its name, whether a kit's (built in), and the node that uses it."""
    return fields_of(value, {"name": text_of, "kit": flag_of, "usedAt": text_of})


def source_of(value):
    """Where an element is written: a definition's address and kind, its file, line and column, and whether shared."""
    return fields_of(value, {"address": text_of, "kind": text_of, "file": text_of, "line": whole_of,
                             "column": whole_of, "shared": flag_of})


RECORD = {"id": text_of, "revision": whole_of, "node": text_of, "type": text_of, "pointer": text_of,
          "bounds": bounds_of, "bindings": bindings_of, "handlers": handlers_of, "repeat": repeat_of,
          "component": component_of, "exact": flag_of, "hitType": text_of, "proposal": whole_of, "source": source_of}


def picks_of(value, most=4):
    """The pick records a message carries: at most `most` (4 on a request), each with only the keys of a record, each
    of its type. A record without a `node` text is dropped."""
    if not isinstance(value, list):
        return []
    records = [item for item in value if isinstance(item, dict) and isinstance(item.get("node"), str)]
    return [fields_of(record, RECORD) for record in records[:most]]


def shown(value):
    """A record's text as one line of the agent's text: a control character or a line or paragraph separator becomes a
    space, so an id or a file name a document wrote can't start a line of its own."""
    return "".join(" " if unicodedata.category(c) in ("Cc", "Zl", "Zp") else c for c in str(value))


def pick_line(pick):
    """One line telling what a pick points at, and where it is written. Every text in it is the document's, shown()."""
    source = pick.get("source") or {}
    line = f"Pointed at: {shown(pick.get('type', 'element'))} {shown(pick['node'])}"
    if "line" in source and "address" in source:  # a program: the definition that builds it
        line += f", built by {shown(source['address'])}, line {source['line']}"
    elif "file" in source:  # a JSON document: the file it is in
        line += f", in {shown(source['file'])}"
    if pick.get("repeat"):
        line += " (one of a repeat)"
    component = pick.get("component") or {}
    if component.get("kit"):
        line += f" (inside the built-in component {shown(component.get('name'))}, which can't be changed)"
    if source.get("shared"):
        line += " (a shared add-on)"
    if pick.get("exact") is False:
        line += " (the nearest element with an address)"
    return line


def pick_lines(picks):
    """The lines telling what the person pointed at, one per pick, as the agent reads them after a request's text: data
    about the app's document, never instructions."""
    return [pick_line(pick) for pick in picks]
