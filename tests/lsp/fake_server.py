#!/usr/bin/env python3
"""A fake language server for the `lsp` service: JSON-RPC over stdio with Content-Length framing, its answers fixed.

It speaks enough of the protocol for every request the service sends (initialize, sync, hover, completion, signature
help, definition, references, rename, formatting, symbols, semantic tokens) and publishes a diagnostic for each line of
an open document that contains "FIXME". Once initialized it asks for its settings (`workspace/configuration`, section
"fake") and publishes them as a diagnostic of "settings.json". What it answers is answer(method, params, text) below:

    python3 tests/lsp/fake_server.py --print hover      # the result it gives for a hover, as JSON

tests/unit/lsp_service.cpp drives it. Flags that make it misbehave, for the client's limits: --crash-after N (exit
after N requests), --outside (locations and edits outside the folder), --flood (a diagnostic per line and 10,000
completions).
"""

import json
import sys

LEGEND = {"tokenTypes": ["namespace", "type", "function", "variable", "macro", "keyword", "comment"],
          "tokenModifiers": ["declaration"]}


def location(uri, line, character, length=4):
    """A location of `length` characters at a line and character (from 0)."""
    return {"uri": uri, "range": {"start": {"line": line, "character": character},
                                  "end": {"line": line, "character": character + length}}}


def diagnostics(uri, text, flood=False):
    """The diagnostics of a document: an error at each "FIXME" (with --flood, a warning on every line too)."""
    items = []
    for number, line in enumerate(text.split("\n")):
        at = line.find("FIXME")
        if at >= 0:
            items.append({"range": {"start": {"line": number, "character": at},
                                    "end": {"line": number, "character": at + 5}},
                          "severity": 1, "source": "fake", "message": "FIXME left in the code"})
        elif flood:
            items.append({"range": {"start": {"line": number, "character": 0}, "end": {"line": number, "character": 1}},
                          "severity": 2, "source": "fake", "message": "line " + str(number + 1)})
    return {"uri": uri, "diagnostics": items}


def answer(method, params, root, outside=False, flood=False):
    """The result of a request, from its method and params; root is the folder's URI (no trailing slash)."""
    uri = params.get("textDocument", {}).get("uri", root + "/src/main.cpp")
    elsewhere = "file:///elsewhere/secret.cpp" if outside else uri
    if method == "initialize":
        return {"capabilities": {
            "positionEncoding": "utf-32",
            "textDocumentSync": {"openClose": True, "change": 2},
            "hoverProvider": True, "completionProvider": {"triggerCharacters": ["."]},
            "signatureHelpProvider": {"triggerCharacters": ["(", ","]},
            "definitionProvider": True, "referencesProvider": True, "renameProvider": True,
            "documentFormattingProvider": True, "documentSymbolProvider": True, "workspaceSymbolProvider": True,
            "semanticTokensProvider": {"legend": LEGEND, "full": True}},
            "serverInfo": {"name": "fake", "version": "1"}}
    if method == "shutdown":
        return None
    if method == "textDocument/hover":
        return {"contents": {"kind": "markdown", "value": "```cpp\nint main()\n```\nThe program starts here."}}
    if method == "textDocument/completion":
        items = [{"label": "printf", "kind": 3, "detail": "int printf(const char*, ...)", "insertText": "printf"},
                 {"label": "puts", "kind": 3, "detail": "int puts(const char*)",
                  "insertTextFormat": 2, "insertText": "puts(${1:text})"},
                 {"label": "putchar", "kind": 3, "detail": "int putchar(int)"}]
        if flood:
            items = [{"label": "item" + str(n)} for n in range(10000)]
        return {"isIncomplete": False, "items": items}
    if method == "textDocument/signatureHelp":
        return {"signatures": [{"label": "int puts(const char* text)", "documentation": "Writes a line.",
                                "parameters": [{"label": [9, 25]}]}], "activeSignature": 0, "activeParameter": 0}
    if method == "textDocument/definition":
        return location(elsewhere, 0, 4)
    if method == "textDocument/references":
        return [location(uri, 0, 4), location(elsewhere, 2, 8)]
    if method == "textDocument/rename":
        name = params.get("newName", "renamed")
        return {"changes": {uri: [{"range": location(uri, 0, 4)["range"], "newText": name}],
                            elsewhere: [{"range": location(elsewhere, 2, 8)["range"], "newText": name}]}}
    if method == "textDocument/formatting":
        return [{"range": {"start": {"line": 0, "character": 0}, "end": {"line": 0, "character": 0}},
                 "newText": "// formatted\n"}]
    if method == "textDocument/documentSymbol":
        return [{"name": "main", "detail": "int ()", "kind": 12, "range": location(uri, 0, 0, 10)["range"],
                 "selectionRange": location(uri, 0, 4)["range"],
                 "children": [{"name": "count", "kind": 13, "range": location(uri, 1, 1, 9)["range"],
                               "selectionRange": location(uri, 1, 5, 5)["range"]}]}]
    if method == "workspace/symbol":
        return [{"name": "main", "kind": 12, "location": location(root + "/src/main.cpp", 0, 4)}]
    if method == "textDocument/semanticTokens/full":
        return {"data": [0, 0, 3, 1, 0, 0, 4, 4, 2, 1]}  # "int" a type, "main" a function's declaration
    raise KeyError(method)


def read(stream):
    """One message, or None at the end of the input."""
    length = None
    while True:
        line = stream.readline()
        if not line:
            return None
        line = line.strip()
        if not line:
            break
        name, _, value = line.decode("ascii").partition(":")
        if name.lower() == "content-length":
            length = int(value)
    return json.loads(stream.read(length)) if length is not None else None


def write(stream, message):
    """Sends one message."""
    body = json.dumps(message).encode("utf-8")
    stream.write(b"Content-Length: " + str(len(body)).encode("ascii") + b"\r\n\r\n" + body)
    stream.flush()


def main(arguments):
    """Serves stdin and stdout until exit, or prints one answer with --print METHOD."""
    outside = "--outside" in arguments
    flood = "--flood" in arguments
    crash = int(arguments[arguments.index("--crash-after") + 1]) if "--crash-after" in arguments else -1
    if "--print" in arguments:
        method = arguments[arguments.index("--print") + 1]
        method = method if "/" in method or method in ("initialize", "shutdown") else "textDocument/" + method
        print(json.dumps(answer(method, {"newName": "renamed"}, "file:///folder", outside, flood), indent=1))
        return 0
    source, sink = sys.stdin.buffer, sys.stdout.buffer
    root, texts, requests = "file:///", {}, 0
    while True:
        message = read(source)
        if message is None:
            return 0
        method, params = message.get("method", ""), message.get("params") or {}
        if method == "exit":
            return 0
        if not method:  # the answer to the settings asked for below
            if message.get("id") == "settings":
                write(sink, {"jsonrpc": "2.0", "method": "textDocument/publishDiagnostics", "params": {
                    "uri": root + "/settings.json", "diagnostics": [
                        {"range": {"start": {"line": 0, "character": 0}, "end": {"line": 0, "character": 1}},
                         "severity": 3, "message": json.dumps(message.get("result"))}]}})
            continue
        if method == "initialized":
            write(sink, {"jsonrpc": "2.0", "id": "settings", "method": "workspace/configuration",
                         "params": {"items": [{"section": "fake"}]}})
        if method == "initialize":
            root = (params.get("rootUri") or "file://").rstrip("/")
        if method in ("textDocument/didOpen", "textDocument/didChange"):
            document = params["textDocument"]
            text = document.get("text", texts.get(document["uri"], ""))
            for change in params.get("contentChanges", []):
                if "range" not in change:
                    text = change["text"]
                    continue
                lines = text.split("\n")
                start, end = change["range"]["start"], change["range"]["end"]

                def offset(at):
                    return sum(len(line) + 1 for line in lines[:at["line"]]) + at["character"]

                text = text[:offset(start)] + change["text"] + text[offset(end):]
            texts[document["uri"]] = text
            write(sink, {"jsonrpc": "2.0", "method": "textDocument/publishDiagnostics",
                         "params": diagnostics(document["uri"], text, flood)})
        if "id" not in message:
            continue
        requests += 1
        if requests == crash:
            return 1
        try:
            write(sink, {"jsonrpc": "2.0", "id": message["id"], "result": answer(method, params, root, outside, flood)})
        except KeyError:
            write(sink, {"jsonrpc": "2.0", "id": message["id"], "error": {"code": -32601, "message": "no " + method}})


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
