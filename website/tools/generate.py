#!/usr/bin/env python3
"""Stages the documentation site's sources into website/build/docs: the hand-written pages of website/docs, the
recipes (for pymdownx.snippets), and the references generated from the repository, so nothing is copied by hand.

Usage: generate.py --cli PATH [--api DIR]
  --cli  the affrescoui command-line tool, for `affrescoui schema`
  --api  Doxygen's HTML output, published under api/ (reference/cpp-api.md links to it)
"""

import argparse
import json
import posixpath
import re
import shutil
import subprocess
import sys
from pathlib import Path
from urllib.parse import urljoin, urlsplit

WEBSITE = Path(__file__).resolve().parent.parent
REPO = WEBSITE.parent
BUILD = WEBSITE / "build"
DOCS = BUILD / "docs"
SNIPPETS = BUILD / "snippets"
GITHUB = "https://github.com/RiccardoSottini/AffrescoUI/"
BLOB = GITHUB + "blob/master/"

# Repository documents the site includes whole, and the page each becomes. Links to them become links to the page.
INCLUDED = {
    "SCREEN_FORMAT.md": "reference/screen-format.md",
    "VERSION_CONTROL.md": "reference/version-control.md",
    "SECURITY.md": "security.md",
    "tools/affrescoui/README.md": "reference/cli.md",
    "schema/affrescoui-screen.schema.json": "reference/schemas.md",
    "schema/affrescoui-scenarios.schema.json": "reference/schemas.md",
}

FENCE = re.compile(r"^(```|~~~)")
LINK = re.compile(r"(!?\[[^\]]*\])\(([^)\s]+)\)")
CODE_SPAN = re.compile(r"(`+)(?:.|\n)*?\1")


def rewrite_link(target, source, page):
    """Returns where a link of a repository document points on the site.

    @param target  the link as written in the document
    @param source  the document's path in the repository, e.g. "tools/affrescoui/README.md"
    @param page    the page it becomes on the site, e.g. "reference/cli.md"
    @return a page of the site (relative to page) when the target is included, else a GitHub URL
    """
    if urlsplit(target).scheme or target.startswith("#") or target.startswith("mailto:"):
        return target
    url = urljoin(BLOB + source, target)
    if not url.startswith(BLOB):
        return url
    path, _, anchor = url[len(BLOB):].partition("#")
    if path in INCLUDED:
        relative = posixpath.relpath(INCLUDED[path], posixpath.dirname(page) or ".")
        return relative + ("#" + anchor if anchor else "")
    return url


def rewrite_text(text, source, page):
    """Rewrites the links of Markdown text outside code blocks and code spans.

    @param text    the document's Markdown
    @param source  the document's path in the repository
    @param page    the page it becomes on the site
    @return the Markdown with every link pointing at the site or at GitHub
    """
    out, prose, fenced = [], [], False

    def flush():
        chunk = "".join(prose)
        pieces, last = [], 0
        for code in CODE_SPAN.finditer(chunk):
            pieces.append(LINK.sub(lambda m: m.group(1) + "(" + rewrite_link(m.group(2), source, page) + ")",
                                   chunk[last:code.start()]))
            pieces.append(code.group(0))
            last = code.end()
        pieces.append(LINK.sub(lambda m: m.group(1) + "(" + rewrite_link(m.group(2), source, page) + ")",
                               chunk[last:]))
        out.append("".join(pieces))
        prose.clear()

    for line in text.splitlines(keepends=True):
        if FENCE.match(line.lstrip()):
            if not fenced:
                flush()
            fenced = not fenced
            out.append(line)
        elif fenced:
            out.append(line)
        else:
            prose.append(line)
    flush()
    return "".join(out)


def include_document(source, page, intro=""):
    """Writes a repository document as a site page, with its links rewritten.

    @param source  the document's path in the repository
    @param page    the page it becomes
    @param intro   Markdown put under the document's first heading
    """
    text = (REPO / source).read_text(encoding="utf-8")
    text = rewrite_text(text, source, page)
    if intro:
        head, _, rest = text.partition("\n")
        text = head + "\n\n" + intro + "\n" + rest
    note = f"\n\n---\n\n*This page is [`{source}`]({BLOB}{source}), included when the site is built.*\n"
    write(page, text.rstrip() + note)


def write(page, text):
    """Writes a page of the site.

    @param page  its path under the docs folder
    @param text  its Markdown
    """
    path = DOCS / page
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def cell(text):
    """Escapes text for a Markdown table cell.

    @param text  any text
    @return the text on one line, with its pipes escaped
    """
    return " ".join(str(text).split()).replace("|", "\\|")


def describe(schema, defs):
    """Returns a short description of a schema: its own, else the definition it refers to.

    @param schema  a JSON Schema
    @param defs    the version's definitions, for $ref
    @return the description, or ""
    """
    if not isinstance(schema, dict):
        return ""
    if schema.get("description"):
        return schema["description"]
    ref = schema.get("$ref", "")
    if ref.startswith("#/$defs/v1/"):
        name = ref.rsplit("/", 1)[1]
        text = describe(defs.get(name, {}), defs)
        return text or f"A {name}"
    for key in ("anyOf", "oneOf"):
        for option in schema.get(key, []):
            text = describe(option, defs)
            if text:
                return text
    return ""


def kind(schema):
    """Returns the JSON type a schema accepts, for the catalog's tables.

    @param schema  a JSON Schema
    @return "string", "number", "object"... or "" when it accepts several
    """
    if not isinstance(schema, dict):
        return ""
    if "type" in schema:
        return schema["type"] if isinstance(schema["type"], str) else " or ".join(schema["type"])
    if "const" in schema:
        return json.dumps(schema["const"])
    if "$ref" in schema:
        return schema["$ref"].rsplit("/", 1)[1]
    return ""


def table(properties, defs):
    """Returns a Markdown table of a schema's properties.

    @param properties  the "properties" of a JSON Schema
    @param defs        the version's definitions
    @return the table
    """
    rows = ["| Key | Accepts | Meaning |", "|---|---|---|"]
    for name, schema in properties.items():
        rows.append(f"| `{name}` | {cell(kind(schema))} | {cell(describe(schema, defs))} |")
    return "\n".join(rows)


def catalog(schema):
    """Writes reference/catalog.md: the built-in catalog, from the JSON Schema the tool generates.

    @param schema  the output of `affrescoui schema`
    """
    defs = schema["$defs"]["v1"]
    screen = defs["screen"]["properties"]
    node = defs["node"]["properties"]
    parts = [
        "# Catalog",
        "",
        "What the format offers out of the box: the sections of a screen, the properties of a node, the steps and "
        "the value types. This page is generated from `affrescoui schema` when the site is built, so it matches the "
        "library it was built from. A running host's catalog (`getCatalog`, MCP tool `get_catalog`) also lists the "
        "custom types, native actions and native functions its application registered.",
        "",
        "## Node types",
        "",
        cell(describe(node["type"], defs)) + ".",
        "",
        "## Sections of a screen",
        "",
        table(screen, defs),
        "",
        "## Node properties",
        "",
        "Every property any built-in node type accepts. [The screen format](screen-format.md) says which type takes "
        "which.",
        "",
        table(node, defs),
        "",
        "## Steps",
        "",
        "A step is one object; its first key names it, and the keys in brackets are its arguments.",
        "",
        cell(describe(defs["step"], defs)),
        "",
        "## Value types",
        "",
        "| Name | Meaning |",
        "|---|---|",
    ]
    for name, value in defs.items():
        if name not in ("screen", "node", "step") and describe(value, defs) and value.get("description"):
            parts.append(f"| `{name}` | {cell(value['description'])} |")
    write("reference/catalog.md", "\n".join(parts) + "\n")


def mcp_tools():
    """Writes reference/mcp.md from the MCP server's tool list (TOOLS in mcp/affrescoui_mcp.py)."""
    sys.path.insert(0, str(REPO / "mcp"))
    import affrescoui_mcp  # noqa: E402 (the server's module; importing it starts nothing)

    parts = [
        "# MCP tools",
        "",
        "The tools [`mcp/affrescoui_mcp.py`](" + BLOB + "mcp/affrescoui_mcp.py) offers an agent, generated from "
        "its tool list when the site is built. Setup: [`mcp/README.md`](" + BLOB + "mcp/README.md).",
        "",
    ]
    for tool in affrescoui_mcp.TOOLS:
        parts += [f"## `{tool['name']}`", "", tool["description"], ""]
        properties = tool.get("inputSchema", {}).get("properties", {})
        required = set(tool.get("inputSchema", {}).get("required", []))
        if properties:
            parts += ["| Argument | Type | Required | Meaning |", "|---|---|---|---|"]
            for name, schema in properties.items():
                parts.append(f"| `{name}` | {cell(kind(schema))} | {'yes' if name in required else ''} | "
                             f"{cell(schema.get('description', ''))} |")
            parts.append("")
    write("reference/mcp.md", "\n".join(parts))


def schemas():
    """Writes reference/schemas.md and publishes the JSON Schemas of schema/ next to it."""
    parts = [
        "# JSON Schemas",
        "",
        "Editors validate and complete screen files with these. A screen names its schema with `\"$schema\"`; the "
        "[VS Code extension](" + BLOB + "editors/vscode/README.md) associates them by file name.",
        "",
        "| File | For | Download |",
        "|---|---|---|",
    ]
    for path in sorted((REPO / "schema").glob("*.json")):
        data = json.loads(path.read_text(encoding="utf-8"))
        shutil.copyfile(path, DOCS / "reference" / path.name)
        parts.append(f"| `schema/{path.name}` | {cell(data.get('description', data.get('title', '')))} | "
                     f"[{path.name}]({path.name}) |")
    parts += ["", "`affrescoui schema` generates the screen schema from the library; `affrescoui schema --check` "
              "says whether the file in the repository is out of date.", ""]
    write("reference/schemas.md", "\n".join(parts))


def main():
    """Stages the site into website/build/docs."""
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--cli", required=True)
    parser.add_argument("--api")
    args = parser.parse_args()

    shutil.rmtree(DOCS, ignore_errors=True)
    shutil.rmtree(SNIPPETS, ignore_errors=True)
    shutil.copytree(WEBSITE / "docs", DOCS)
    shutil.copytree(WEBSITE / "recipes", SNIPPETS / "recipes")

    include_document("SCREEN_FORMAT.md", "reference/screen-format.md")
    include_document("VERSION_CONTROL.md", "reference/version-control.md")
    include_document("SECURITY.md", "security.md")
    include_document("tools/affrescoui/README.md", "reference/cli.md")
    schema = json.loads(subprocess.run([args.cli, "schema"], check=True, capture_output=True, text=True).stdout)
    catalog(schema)
    schemas()
    mcp_tools()
    if args.api:
        shutil.copytree(args.api, DOCS / "api")


if __name__ == "__main__":
    main()
