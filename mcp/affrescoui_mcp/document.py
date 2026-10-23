"""Reading a screen document the way an agent needs it: compact JSON, the changes since a read, the outline, one
node by address, a search, and JSON Patch applied to a copy."""

import json


def compact(value):
    """Minified JSON: what tool results carry (a reader never sees them, the agent pays per token)."""
    return json.dumps(value, separators=(",", ":"), ensure_ascii=False)


def diff(before, after, path=""):
    """A small JSON Patch from before to after: objects key by key, lists item by item when only their tail differs
    in length, anything else replaced whole."""
    if before == after:
        return []
    if isinstance(before, dict) and isinstance(after, dict):
        return diff_objects(before, after, path)
    if isinstance(before, list) and isinstance(after, list):
        return diff_lists(before, after, path)
    return [{"op": "replace", "path": path, "value": after}]


def member(path, key):
    """The JSON Pointer to an object's member: its key escaped (RFC 6901), "~" as "~0" and "/" as "~1"."""
    return path + "/" + str(key).replace("~", "~0").replace("/", "~1")


def diff_objects(before, after, path):
    """diff() of two objects: members removed, changed (recursively), then added."""
    ops = []
    for key in before:
        if key not in after:
            ops.append({"op": "remove", "path": member(path, key)})
        else:
            ops.extend(diff(before[key], after[key], member(path, key)))
    ops.extend({"op": "add", "path": member(path, key), "value": after[key]} for key in after if key not in before)
    return ops


def diff_lists(before, after, path):
    """diff() of two lists: the common items changed in place, the tail removed from the end or appended; the list
    replaced whole when that is shorter."""
    common = min(len(before), len(after))
    ops = []
    for i in range(common):
        ops.extend(diff(before[i], after[i], f"{path}/{i}"))
    ops.extend({"op": "remove", "path": f"{path}/{i}"} for i in range(len(before) - 1, common - 1, -1))
    ops.extend({"op": "add", "path": f"{path}/-", "value": after[i]} for i in range(common, len(after)))
    if len(compact(ops)) > len(compact(after)):
        return [{"op": "replace", "path": path, "value": after}]
    return ops


def changes_since(seen, document):
    """What get_screen returns after a first read: a JSON Patch from what the agent saw to the document now, its
    paths written from node ids, as edits are."""
    changes = diff(seen, document)
    for op in changes:
        if op["path"].startswith(("/root", "/screens", "/templates", "/components")):
            op["path"] = by_id(document if op["op"] != "remove" else seen, op["path"])
    return changes


def by_id(document, pointer):
    """A JSON Pointer written from the nearest node with an id: "/root/children/2/children/1/text" is "#title/text"."""
    parts = pointer.split("/")[1:]
    node, best = document, None
    for i, part in enumerate(parts):
        try:
            node = node[int(part)] if isinstance(node, list) else node[part.replace("~1", "/").replace("~0", "~")]
        except (KeyError, IndexError, ValueError, TypeError):
            break
        if isinstance(node, dict) and isinstance(node.get("id"), str):
            found = find_node(document, node["id"])
            if found and found[0] == "/" + "/".join(parts[:i + 1]):  # the id finds this very node
                best = (node["id"], parts[i + 1:])
    if best is None:
        return pointer
    return "#" + best[0] + ("/" + "/".join(best[1]) if best[1] else "")


def find_node(document, node_id):
    """The JSON Pointer and node with an id (in root, screens, templates, components), like the host's #id paths."""
    def search(node, pointer):
        if not isinstance(node, dict):
            return None
        if node.get("id") == node_id:
            return pointer, node
        for i, child in enumerate(node.get("children") or []):
            found = search(child, f"{pointer}/children/{i}")
            if found:
                return found
        return None
    found = search(document.get("root"), "/root")
    for section in ("screens", "templates", "overlays"):
        for name, node in (document.get(section) or {}).items():
            found = found or search(node, f"/{section}/{name}")
    for name, component in (document.get("components") or {}).items():
        found = found or search((component or {}).get("node"), f"/components/{name}/node")
    return found


def trees(document):
    """Where a document's nodes start, as (JSON Pointer, node): root, screens, templates, overlays, then components'
    nodes."""
    yield "/root", document.get("root")
    for section in ("screens", "templates", "overlays"):
        for name, node in (document.get(section) or {}).items():
            yield f"/{section}/{name}", node
    for name, component in (document.get("components") or {}).items():
        yield f"/components/{name}/node", (component or {}).get("node")


def walk_nodes(node, address, depth=0, parent=None):
    """Every node of a tree, parents first, as (node, address, depth, parent's address). The address is "#id", or a
    path from the nearest node with one."""
    if not isinstance(node, dict) or node.get("slot") is True:
        return
    if isinstance(node.get("id"), str) and node["id"]:
        address = "#" + node["id"]
    yield node, address, depth, parent
    for i, child in enumerate(node.get("children") or []):
        yield from walk_nodes(child, f"{address}/children/{i}", depth + 1, address)


def node_type(node, styles=None):
    """A node's type as the outline writes it: its "type", else its style's (styles may name styles), else "panel";
    "use <component>" for a use."""
    if "use" in node:
        return "use " + str(node["use"])
    for _ in range(16):  # styles naming styles, as deep as the runtime follows them
        if isinstance(node.get("type"), str):
            return node["type"]
        style = node.get("style")
        if not isinstance(style, str) or not isinstance((styles or {}).get(style), dict):
            break
        node = styles[style]
    return "panel"


def short(value, length=24):
    """A value in a few characters: text quoted, cut with "..."."""
    text = value if isinstance(value, str) else compact(value)
    return compact(text[:length] + "..." if len(text) > length else text)


def summary(node, address, styles=None):
    """What an agent needs to know of a node to find and change it, without its styling: its type (unless its id
    starts with it, as given ids do: "#label-blocked"), its text, and what it binds, repeats, shows when, looks like
    (style) and runs on a click."""
    kind = node_type(node, styles)
    bits = [] if address.startswith(f"#{kind}-") else [kind]
    for key in ("text", "placeholder", "tooltip"):
        if isinstance(node.get(key), str):
            bits.append(short(node[key]) if key == "text" else f"{key}={short(node[key])}")
    for key in ("bind", "onClick"):
        if isinstance(node.get(key), str):
            bits.append(f"{key}={node[key]}")
    if isinstance(node.get("repeat"), dict):
        bits.append(f"repeat {node['repeat'].get('for', 'item')} in {node['repeat'].get('in')}")
    if "when" in node:
        bits.append("when")
    return " ".join(bits)


TYPE_WORDS = {"string": "text", "number": "number", "integer": "whole number", "boolean": "boolean",
              "object": "object", "array": "list", "null": "null"}


def describe_type(kind, depth=0):
    """A state type in a few words, as the runtime says it (StateTypes.cpp, describeType): "number", "'all' | 'bug'",
    "list of {title: text}"."""
    if isinstance(kind, bool):
        return "anything" if kind else "nothing"
    if not isinstance(kind, dict):
        return "anything"
    if isinstance(kind.get("enum"), list):
        return describe_enum(kind["enum"])
    if "const" in kind:
        return compact(kind["const"])
    names = kind.get("type")
    said = [describe_type_name(kind, name, depth) for name in (names if isinstance(names, list) else [names])]
    return " | ".join(word for word in said if word) or "anything"


def describe_enum(values):
    """An enum's values as the runtime says them: text quoted, the rest as JSON, the first 8 and then "..."."""
    said = ["'" + v + "'" if isinstance(v, str) else compact(v) for v in values[:8]]
    return " | ".join(said) + (" | ..." if len(values) > 8 else "")


def describe_type_name(kind, name, depth):
    """One of a state type's JSON types in words, its items or properties three levels deep; "" for no type."""
    if name == "array" and "items" in kind:
        return "list of ..." if depth >= 3 else "list of " + describe_type(kind["items"], depth + 1)
    if name == "object" and isinstance(kind.get("properties"), dict):
        return "{...}" if depth >= 3 else "{" + ", ".join(
            f"{key}: {describe_type(inside, depth + 1)}" for key, inside in kind["properties"].items()) + "}"
    return TYPE_WORDS.get(name, "")


def state_types(document):
    """The state's declared types ("stateTypes") on one line, for the outline: "state: filter: 'all' | 'bug';
    volume: number"; "" if it declares none."""
    types = document.get("stateTypes")
    if not isinstance(types, dict) or not types:
        return ""
    return "state: " + "; ".join(f"{key}: {describe_type(kind)}" for key, kind in types.items())


def outline(document, at=None, depth=None):
    """The node tree, one line per node: its address, then summary(). `at` (a node's address or pointer, found with
    locate) starts it there; `depth` stops it that many levels down, and a node with more inside says how many
    ("+12"). Enough to choose where to edit, without the styling. The whole outline starts with the state's types,
    when the screen declares them."""
    starts = [locate(document, at)] if at else trees(document)
    styles = document.get("styles")
    lines = [[state_types(document), 0]] if not at and state_types(document) else []  # [text, nodes not shown]
    for pointer, start in starts:
        cut = None  # the last line at the deepest level shown: what is below it isn't
        for node, address, level, _ in walk_nodes(start, pointer):
            if depth is not None and level > depth:
                cut[1] += 1
                continue
            lines.append([" " * level + f"{address} {summary(node, address, styles)}".rstrip(), 0])
            if depth is not None and level == depth:
                cut = lines[-1]
    return "\n".join(text + (f" +{inside}" if inside else "") for text, inside in lines)


def locate(document, at):
    """The JSON Pointer and value at "#id" (a node), "#id/rest", or a JSON Pointer; a ValueError says why there is
    none."""
    pointer = str(at)
    if pointer.startswith("#"):
        node_id, _, rest = pointer[1:].partition("/")
        found = find_node(document, node_id)
        if not found:
            raise ValueError(f"no node with id '{node_id}'")
        pointer = found[0] + ("/" + rest if rest else "")
    value = document
    try:
        for part in [p for p in pointer.split("/") if p]:
            value = value[int(part)] if isinstance(value, list) else value[part.replace("~1", "/").replace("~0", "~")]
    except (KeyError, IndexError, ValueError, TypeError):
        raise ValueError(f"nothing at {at}")
    return pointer, value


def pruned(node, address, depth):
    """A node with what is deeper than `depth` levels below it replaced by its children's addresses:
    "children": ["#label-a", "#row-b"]. Read one of them with at= to go further."""
    if not isinstance(node, dict) or not isinstance(node.get("children"), list):
        return node
    children = []
    for i, child in enumerate(node["children"]):
        named = isinstance(child, dict) and isinstance(child.get("id"), str) and child["id"]
        child_address = "#" + child["id"] if named else f"{address}/children/{i}"
        children.append(child_address if depth <= 0 else pruned(child, child_address, depth - 1))
    return {**node, "children": children}


def find_nodes(document, kind=None, text=None, prop=None, value=None, limit=20):
    """The nodes that match every criterion given: their type (`kind`, or the component of a use), text (in their
    text, placeholder or tooltip, any case), and a property they have (`prop`), with a value (`value`, compared as
    written). One line each, as in the outline, with the parent's address."""
    needle = text.lower() if isinstance(text, str) else None
    styles = document.get("styles")
    found = []
    for pointer, start in trees(document):
        for node, address, _, parent in walk_nodes(start, pointer):
            if not (matches_kind(node, kind, styles) and matches_text(node, needle)
                    and matches_property(node, prop, value)):
                continue
            found.append(f"{address} {summary(node, address, styles)}".rstrip() + (f" in {parent}" if parent else ""))
    shown = found[:max(1, int(limit))]
    more = f"\n... and {len(found) - len(shown)} more: narrow the search or raise limit" if len(found) > len(shown) \
        else ""
    return f"{len(found)} found" + ("\n" + "\n".join(shown) if shown else "") + more


def matches_kind(node, kind, styles):
    """Whether a node is of type `kind` or uses the component `kind`; any node when no kind is given."""
    return not kind or kind in (node_type(node, styles), node.get("use"))


def matches_text(node, needle):
    """Whether a node's text, placeholder or tooltip holds `needle` (lower case); any node when it is None."""
    return needle is None or any(isinstance(node.get(k), str) and needle in node[k].lower()
                                 for k in ("text", "placeholder", "tooltip"))


def matches_property(node, prop, value):
    """Whether a node has `prop`, set to `value` (as written, or as its compact JSON) when one is given; any node
    when no property is."""
    if not prop:
        return True
    return prop in node and (value is None or node[prop] == value or compact(node[prop]) == str(value))


def pointer_parts(pointer):
    """A JSON Pointer's segments, unescaped (RFC 6901): "/a~1b/0" -> ["a/b", "0"]."""
    if pointer == "":
        return []
    if not pointer.startswith("/"):
        raise ValueError(f"not a JSON Pointer: {pointer!r}")
    return [part.replace("~1", "/").replace("~0", "~") for part in pointer[1:].split("/")]


def apply_patch(document, patch):
    """A copy of document with an RFC 6902 JSON Patch applied (what get_diff returns for a proposal)."""
    root = {"": json.loads(json.dumps(document))}  # a holder, so the whole document can be replaced too

    def parent(pointer):
        parts = [""] + pointer_parts(pointer)
        at = root
        for part in parts[:-1]:
            at = at[int(part)] if isinstance(at, list) else at[part]
        return at, parts[-1]

    def take(pointer, remove):
        holder, key = parent(pointer)
        index = int(key) if isinstance(holder, list) else key
        return holder.pop(index) if remove else holder[index]

    def put(pointer, value, replace):
        holder, key = parent(pointer)
        if isinstance(holder, list):
            index = len(holder) if key == "-" else int(key)
            holder[index:index + (1 if replace else 0)] = [value]
        else:
            holder[key] = value

    for operation in patch:
        kind, path = operation["op"], operation["path"]
        if kind in ("add", "replace"):
            put(path, operation["value"], kind == "replace")
        elif kind == "remove":
            take(path, True)
        elif kind in ("move", "copy"):
            put(path, take(operation["from"], kind == "move"), False)
        elif kind == "test" and take(path, False) != operation["value"]:
            raise ValueError(f"test failed at {path}")
    return root[""]
