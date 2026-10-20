#!/usr/bin/env python3
"""Feature packages, composed: RFC 0004's install checks, its install revision, and the merged screen.

    python3 compose.py check   APP FEATURE...            # what keeps each feature from installing, with its place
    python3 compose.py install APP FEATURE... [-o FILE]  # the revision that installs them (one record per feature)
    python3 compose.py expand  APP FEATURE... -o FILE    # the screen the runtime would build, loadable today

The app is a screen in the RFC's syntax: nodes with "point" are contribution points, and "exports" names values
features may read as `app.<name>`. A feature is an `affrescoui-feature` manifest: what it requires and what it
provides. Inside a manifest, declarations are local ("toggle") and references are full ("wordCount.toggle",
"state.wordCount.characters", "computed.wordCount.words"); node ids are written in full ("wordCount-label").

`expand` writes what the runtime would merge at build time (RFC 0004, "How the runtime merges features"), in today's
format: the RFC's own keys (point, exports, features, menus, services, command objects) are left out or lowered, and
`app.<name>` is replaced by the export's expression. Then `affrescoui check` and `affrescoui render` prove and show
it. Only the standard library.
"""

import argparse
import copy
import json
import os
import re
import sys

NAME = re.compile(r"^[a-z][A-Za-z0-9]*$")
CONTROL_STEPS = {"call", "send", "connect", "patch", "respond", "accept", "reject", "preview", "endPreview",
                 "restore", "revert", "undo"}
CAPABILITIES = {"services", "connections", "channels", "persist", "permissions"}
LOCAL_SECTIONS = ("state", "computed", "actions", "components", "styles", "menus", "settings", "keys", "views")


def load(path):
    """A JSON file."""
    with open(path, encoding="utf-8") as file:
        return json.load(file)


def version_of(text):
    """'1.2.3' as a tuple of three integers."""
    parts = text.split(".")
    if len(parts) != 3 or not all(part.isdigit() for part in parts):
        raise ValueError(f"'{text}' isn't a version like 1.2.3")
    return tuple(int(part) for part in parts)


def satisfies(release, wanted):
    """Whether a release meets a range: '1.2.3', '^1.2.0', '~1.2.0' or '>=1.0.0' (semver's meanings)."""
    have = version_of(release)
    if wanted.startswith(">="):
        return have >= version_of(wanted[2:])
    if wanted.startswith("^"):
        low = version_of(wanted[1:])
        high = (low[0] + 1, 0, 0) if low[0] > 0 else (0, low[1] + 1, 0)
        return low <= have < high
    if wanted.startswith("~"):
        low = version_of(wanted[1:])
        return low <= have < (low[0], low[1] + 1, 0)
    return have == version_of(wanted)


def points_of(node, found=None):
    """Every contribution point of a node tree: point name -> node."""
    found = {} if found is None else found
    if isinstance(node, dict):
        if "point" in node:
            found[node["point"]] = node
        for child in node.get("children", []):
            points_of(child, found)
    return found


def ids_of(node, found=None):
    """Every node id in a tree."""
    found = set() if found is None else found
    if isinstance(node, dict):
        if "id" in node:
            found.add(node["id"])
        for value in node.values():
            ids_of(value, found)
    elif isinstance(node, list):
        for item in node:
            ids_of(item, found)
    return found


def steps_named(value, names, place, found):
    """The places of every step in a value whose first key is one of `names`."""
    if isinstance(value, dict):
        if value and next(iter(value)) in names:
            found.append(f"{place}: '{next(iter(value))}'")
        for key, item in value.items():
            steps_named(item, names, f"{place}.{key}", found)
    elif isinstance(value, list):
        for index, item in enumerate(value):
            steps_named(item, names, f"{place}[{index}]", found)
    return found


class Composition:
    """An app and the features installed into it so far, in order."""

    def __init__(self, app):
        self.app = app
        self.points = points_of(app["root"])
        self.installed = {}  # name -> manifest

    def problems(self, feature):
        """What keeps a feature from installing, each naming its place in the manifest. Empty: it may install."""
        name, found = feature.get("name", ""), []
        if not NAME.match(name):
            return [f"name: '{name}' must be a word in camelCase"]
        if name in self.installed:
            found.append(f"name: a feature '{name}' is installed already")
        found += self.missing_requirements(feature)
        found += self.capability_problems(feature)
        found += self.namespace_problems(feature)
        found += self.key_conflicts(feature)
        return found

    def missing_requirements(self, feature):
        """Requirements the app or the installed features don't meet."""
        requires, found = feature.get("requires", {}), []
        if requires.get("format", 1) != self.app.get("version"):
            found.append(f"requires.format: needs format {requires.get('format')}")
        for point in requires.get("points", []):
            if point not in self.points:
                offered = ", ".join(sorted(self.points)) or "none"
                found.append(f"requires.points: the app has no contribution point '{point}' (it has: {offered})")
        exports = self.app.get("exports", {})
        for export, wanted in requires.get("exports", {}).items():
            if export not in exports:
                found.append(f"requires.exports.{export}: the app doesn't export it")
            elif wanted.get("type") != exports[export].get("type"):
                found.append(f"requires.exports.{export}: needs a {wanted.get('type')}, the app exports a "
                             f"{exports[export].get('type')}")
        for service, methods in requires.get("services", {}).items():
            if service not in self.app.get("services", {}):
                found.append(f"requires.services.{service}: the app declares no service '{service}' (a host must "
                             f"grant it: a revision can't)")
        for other, wanted in requires.get("features", {}).items():
            if other not in self.installed:
                found.append(f"requires.features.{other}: not installed")
            elif not satisfies(self.installed[other]["release"], wanted):
                found.append(f"requires.features.{other}: needs {wanted}, {self.installed[other]['release']} is "
                             f"installed")
        return found

    def capability_problems(self, feature):
        """What only the host may install: capability sections and control steps."""
        provides, found = feature.get("provides", {}), []
        for section in CAPABILITIES & set(provides):
            found.append(f"provides.{section}: a capability: only the host can grant it, never a revision")
        for place in steps_named(provides, CONTROL_STEPS, "provides", []):
            found.append(f"{place} is a control step: a feature that uses one is installed by the host, not by a "
                         f"revision")
        return found

    def namespace_problems(self, feature):
        """Node ids outside the feature's namespace, and ids the app already uses."""
        name, found = feature["name"], []
        taken = ids_of(self.app["root"])
        for point, nodes in feature.get("provides", {}).get("views", {}).items():
            for node_id, node in nodes.items():
                for each in {node_id} | ids_of(node):
                    if not each.startswith(name + "-"):
                        found.append(f"provides.views.{point}.{node_id}: id '{each}' must start with '{name}-'")
                    elif each in taken:
                        found.append(f"provides.views.{point}.{node_id}: id '{each}' is taken")
        return found

    def key_conflicts(self, feature):
        """Shortcuts the app or an installed feature already binds (without a `when` to tell them apart)."""
        bound = dict.fromkeys(self.app.get("keys", {}), "the app")
        for other, manifest in self.installed.items():
            bound.update(dict.fromkeys(manifest.get("provides", {}).get("keys", {}), f"feature '{other}'"))
        return [f"provides.keys.{key}: {bound[key]} binds it already (give one a \"when\")"
                for key in feature.get("provides", {}).get("keys", {}) if key in bound]

    def install(self, feature):
        """Records a feature as installed (after its problems were checked)."""
        self.installed[feature["name"]] = feature


def revision_for(features, sources):
    """The install revision: one record per feature under /features, as compact edits."""
    edits = {}
    for order, feature in enumerate(features):
        record = {"release": feature["release"], "source": sources[feature["name"]], "enabled": True,
                  "order": (order + 1) * 10, "requires": feature.get("requires", {}),
                  "provides": feature.get("provides", {})}
        edits[f"/features/{feature['name']}"] = record
    names = ", ".join(f"{f['title']} {f['release']}" for f in features)
    return {"type": "revision", "message": f"Install {names}", "set": edits}


def lowered_steps(handler):
    """A command object ({title, do}) as today's steps."""
    if isinstance(handler, dict) and "do" in handler and not any(k in CONTROL_STEPS for k in handler):
        return handler["do"]
    return handler


def replace_exports(value, exports):
    """`app.<name>` replaced by the export's expression, in every string of a value."""
    if isinstance(value, dict):
        return {key: replace_exports(item, exports) for key, item in value.items()}
    if isinstance(value, list):
        return [replace_exports(item, exports) for item in value]
    if not isinstance(value, str) or "app." not in value:
        return value
    for name, export in sorted(exports.items(), key=lambda item: -len(item[0])):
        expression = export["value"].strip()[2:-2].strip()
        value = value.replace("app." + name, f"({expression})" if "{{" in value else expression)
    return value


def expand(app, features):
    """The merged screen, in today's format."""
    screen = copy.deepcopy(app)
    exports = screen.pop("exports", {})
    for key in ("services", "menus", "settings"):
        screen.pop(key, None)
    points = points_of(screen["root"])
    for feature in features:
        name = feature["name"]
        provides = replace_exports(copy.deepcopy(feature.get("provides", {})), exports)
        if "state" in provides:
            screen.setdefault("state", {})[name] = provides["state"]
        if "computed" in provides:
            fields = ", ".join(f"{key}: ({value.strip()[2:-2]})" for key, value in provides["computed"].items())
            screen.setdefault("computed", {})[name] = "{{ {" + fields + "} }}"
        for section in ("actions", "components", "styles"):
            for key, value in provides.get(section, {}).items():
                screen.setdefault(section, {})[f"{name}.{key}"] = lowered_steps(value) if section == "actions" \
                    else value
        screen.setdefault("keys", {}).update(provides.get("keys", {}))
        for point, nodes in provides.get("views", {}).items():
            for node_id, node in nodes.items():
                points[point]["children"].append({"id": node_id, **node})
    for node in points.values():
        node.pop("point")
    screen["note"] = "Expanded by compose.py: " + ", ".join(f"{f['name']} {f['release']}" for f in features)
    return screen


def main():
    """The three commands."""
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("command", choices=["check", "install", "expand"])
    parser.add_argument("app")
    parser.add_argument("features", nargs="+")
    parser.add_argument("-o", "--output")
    arguments = parser.parse_args()

    app = load(arguments.app)
    composition, features, sources, failed = Composition(app), [], {}, False
    for path in arguments.features:
        feature = load(path)
        problems = composition.problems(feature)
        if arguments.command == "check" or problems:
            print(f"{os.path.basename(path)}: " + ("can be installed" if not problems else
                                                   f"{len(problems)} problems"))
            for problem in problems:
                print(f"  {feature.get('name', '?')}.{problem}")
        failed |= bool(problems)
        if not problems:
            composition.install(feature)
            features.append(feature)
            sources[feature["name"]] = "file:" + os.path.basename(path)
    if arguments.command == "check":
        return 1 if failed else 0
    if failed:
        print("nothing was written: fix the problems above", file=sys.stderr)
        return 1
    result = revision_for(features, sources) if arguments.command == "install" else expand(app, features)
    text = json.dumps(result, indent="\t", ensure_ascii=False) + "\n"
    if arguments.output:
        with open(arguments.output, "w", encoding="utf-8") as file:
            file.write(text)
        print(f"wrote {arguments.output}")
    else:
        sys.stdout.write(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
