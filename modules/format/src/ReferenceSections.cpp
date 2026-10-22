/// @file
/// @brief format::sectionKeys(), stepKeys(), definitions(): the reference of the sections, steps and shared values.
///
/// Each entry is a key: the schema of its value and its row in SCREEN_FORMAT.md (Reference.hpp). Which keys exist,
/// and in what order, the registries decide; ReferenceTest checks this table against them.

#include "Reference.hpp"

namespace affresco::format {

namespace {

/// The top-level sections, in the order of the table.
constexpr KeyReference sectionTable[] = {
    {"version", R"json({"const": 1})json", "Required. `1`.", "sections"},
    {"root", R"json({"$ref": "#/$defs/v1/node",
		"description": "The window's root panel: it always fills the window"})json",
	 "Required. The window's root node. It always fills the window, so layout properties aren't allowed on it.",
	 "sections"},
    {"format", R"json({"const": "affrescoui-screen"})json", "`\"affrescoui-screen\"`, for tools.", "sections"},
    {"note", R"json({"type": "string"})json", "A comment.", "sections"},
    {"window", R"json({"type": "object", "additionalProperties": false, "properties": {"title": {"type": "string"},
		"width": {"type": "integer", "minimum": 1}, "height": {"type": "integer", "minimum": 1},
		"minWidth": {"type": "integer", "minimum": 0}, "minHeight": {"type": "integer", "minimum": 0},
		"menuBar": {"type": "array", "description": "The native menu bar: menus of the menus section",
		"items": {"type": "object", "required": ["menu"], "additionalProperties": false,
		"properties": {"menu": {"type": "string"}, "title": {"type": "string"}}}},
		"focusRing": {"description": "The ring around the node the keyboard focused: a colour, or its look, or false",
		"anyOf": [{"type": "string"}, {"const": false}, {"type": "object", "additionalProperties": false,
		"properties": {"color": {"type": "string"}, "width": {"type": "number", "minimum": 0},
		"offset": {"type": "number", "minimum": 0}}}]}}})json",
	 "`title`, `width`, `height`, `minWidth`, `minHeight` (integers); `menuBar`: the native menu bar, a list of "
	 "`{ \"menu\": \"file\", \"title\": \"File\" }` naming menus of the `menus` section (macOS, Windows; elsewhere "
	 "none, and `window.nativeMenuBar` is false so the screen can draw its own); `focusRing`: the ring around the "
	 "node the keyboard focused: a colour, `{ \"color\": \"#2563EB\", \"width\": 2, \"offset\": 2 }` (the "
	 "defaults), or `false` (none).",
	 "sections"},
    {"fonts", R"json({"type": "object",
		"description": "Fonts by name; \"default\" is used by text without a \"font\"",
		"additionalProperties": {"type": "object", "required": ["file", "size"],
		"additionalProperties": false, "properties": {"file": {"type": "string"},
		"size": {"anyOf": [{"type": "number", "exclusiveMinimum": 0}, {"$ref": "#/$defs/v1/tokenName"}]}, "fallback": {"type": "array",
		"description": "Other fonts in \"fonts\", asked in order for characters this one lacks; drawn at this font's size",
		"maxItems": 8, "items": {"type": "string"}}}}})json",
	 "`name → { \"file\", \"size\", \"fallback\" }`, paths relative to the screen file (TrueType or OpenType, a "
	 "collection's first font). `default` is used by text without a `font`. Text is UTF-8, shaped per script "
	 "(kerning, ligatures, Arabic, Indic; right-to-left runs), colour emoji from COLR fonts. `\"fallback\": "
	 "[\"symbols\"]` names other fonts asked in order for a character this one lacks; a character none has shows as "
	 "`?` (macOS menu key labels use ⌘ ⇧ ⌥ ⌃, which JetBrains Mono has).",
	 "sections"},
    {"colors", R"json({"type": "object",
		"description": "Named colors (\"@name\"), or per theme: {\"light\": ..., \"dark\": ...}",
		"additionalProperties": {"anyOf": [{"$ref": "#/$defs/v1/hexColor"}, {"type": "object",
		"minProperties": 1, "additionalProperties": {"$ref": "#/$defs/v1/hexColor"}}]}})json",
	 "`name → \"#rrggbb\"`, or per theme `name → { \"light\": \"#fff\", \"dark\": \"#111827\" }`. Used as `\"@name\"`.",
	 "sections"},
    {"tokens", R"json({"type": "object", "maxProperties": 4096,
		"description": "Design tokens (\"@name\"): numbers and colors, aliases (\"@other\"), or per theme: {\"light\": ..., \"dark\": ...}",
		"propertyNames": {"pattern": "^[A-Za-z_][A-Za-z0-9_.-]*$"},
		"additionalProperties": {"anyOf": [{"$ref": "#/$defs/v1/tokenValue"}, {"type": "object",
		"minProperties": 1, "additionalProperties": {"$ref": "#/$defs/v1/tokenValue"}}]}})json",
	 "`name → value`: a number (spacing, a radius, a type size), a colour (`\"#rrggbb\"`, `\"transparent\"`), an alias "
	 "(`\"@other\"`, of a token or of `colors`), or per theme `{ \"light\": ..., \"dark\": ... }` of those. Used as "
	 "`\"@name\"` wherever a number or a colour fits (`\"padding\": \"@space.m\"`, a font's `\"size\"`). Switched by "
	 "`theme` like `colors`; a name in both is an error. Aliases go at most 16 deep.",
	 "sections"},
    {"theme", R"json({"description": "Which color variant is shown, e.g. \"{{state.theme}}\""})json",
	 "Which variant of the colors and tokens is shown, usually bound: `\"{{state.theme}}\"`. Without a match, the first variant.",
	 "sections"},
    {"styles", R"json({"type": "object", "additionalProperties": {"$ref": "#/$defs/v1/node"}})json",
	 "`name → properties`, applied with `\"style\": \"name\"` or `[\"a\", \"b\"]`. A style may itself have a `style`.",
	 "sections"},
    {"components", R"json({"type": "object", "additionalProperties": {"type": "object", "required": ["node"],
		"additionalProperties": false, "properties": {"params": {"type": "object"},
		"node": {"$ref": "#/$defs/v1/node"}, "note": {"type": "string"}}}})json",
	 "`name → { \"params\": {defaults}, \"node\": node }`: reusable nodes with parameters (see Components).",
	 "sections"},
    {"screens", R"json({"type": "object", "description": "Screens an \"outlet\" shows; {\"navigate\": name} switches",
		"additionalProperties": {"$ref": "#/$defs/v1/node"}})json",
	 "`name → node`: the screens an `outlet` shows; `{\"navigate\": \"name\"}` switches. The first is shown first.",
	 "sections"},
    {"templates", R"json({"type": "object", "additionalProperties": {"$ref": "#/$defs/v1/node"}})json",
	 "`name → node`, built by code on demand (`Screen::instantiate`).", "sections"},
    {"state", R"json({"type": "object"})json", "The app's data, any JSON object.", "sections"},
    {"stateTypes", R"json({"type": "object",
		"description": "A type per state key, written as JSON Schema: type, enum, const, properties, required, additionalProperties, items, minimum, maximum, minLength, maxLength, minItems, maxItems",
		"additionalProperties": {"$ref": "#/$defs/v1/stateType"}})json",
	 "`key → type`: what each state key holds, written as JSON Schema (see Typed state).", "sections"},
    {"computed", R"json({"type": "object", "description": "Values derived from the state, read as computed.name"})json",
	 "`name → expression`: values derived from the state, read as `computed.name` (see Values).", "sections"},
    {"watch", R"json({"type": "object", "additionalProperties": {"$ref": "#/$defs/v1/handler"},
		"description": "\"state.path\" -> handler, run when it changed"})json",
	 "`\"state.path\" → handler`: runs after an update in which that value changed (`event.value`, `event.previous`).",
	 "sections"},
    {"actions", R"json({"type": "object", "additionalProperties": {"anyOf": [{"$ref": "#/$defs/v1/steps"},
		{"$ref": "#/$defs/v1/command"}]}})json",
	 "`name → steps`, run by handlers, `run` steps, shortcuts and channels; or a command `{ \"title\", "
	 "\"category\", \"icon\", \"enablement\", \"description\", \"do\" }`, listed by the `commands` variable "
	 "(see Commands and keybindings).",
	 "sections"},
    {"keys", R"json({"type": "object", "additionalProperties": {"$ref": "#/$defs/v1/keyHandler"},
		"description": "Screen-wide shortcuts: \"mod+k\" -> handler"})json",
	 "Screen-wide shortcuts: `\"mod+k\" → handler` (see Events).", "sections"},
    {"menus", R"json({"type": "object", "description": "menu -> item key -> item, listed by the menus variable",
		"additionalProperties": {"type": "object", "additionalProperties": {"type": "object",
		"additionalProperties": false, "properties": {"run": {"type": "string"}, "submenu": {"type": "string"},
		"title": {"type": "string"}, "icon": {"type": "string"}, "group": {"type": "string"},
		"order": {"type": "number"}, "when": {"type": "string"}, "with": {"type": "object"}}}}})json",
	 "`menu → { key → { \"run\" or \"submenu\", \"group\", \"order\", \"when\", \"title\", \"icon\" } }`, "
	 "listed by the `menus` variable (see Menus and settings).",
	 "sections"},
    {"settings", R"json({"type": "object", "description": "\"group.name\" -> a schema with a required default",
		"additionalProperties": {"type": "object", "required": ["default"]}})json",
	 "`\"editor.fontSize\" → { \"type\", \"default\", \"title\", \"description\", \"minimum\", ... }`: "
	 "the user's settings, stored in `state.settings` (see Menus and settings).",
	 "sections"},
    {"onLoad", "handler", "A handler run once after the first update.", "sections"},
    {"channels", R"json({"type": "object",
		"description": "Programs the screen talks to; messages run \"name:type\" actions. Revisions can't change it.",
		"additionalProperties": {"type": "object", "required": ["socket"],
		"additionalProperties": false, "properties": {"socket": {"type": "string"},
		"note": {"type": "string"}, "reconnect": {"anyOf": [{"type": "boolean"}, {"type": "number"},
		{"type": "object", "properties": {"after": {"type": "number"}}}]}}}})json",
	 "Programs the screen talks to (see Channels). A capability: revisions can't change it.", "sections"},
    {"services", R"json({"type": "object", "maxProperties": 64,
		"description": "Services the screen uses (RFC 0002), each {\"provider\": ...}. Revisions can't change it.",
		"additionalProperties": {"type": "object", "required": ["provider"],
		"properties": {"provider": {"type": "string"}}}})json",
	 "`{\"files\": {\"provider\": \"fs\", \"root\": \"workspace\"}}`: services that `invoke` and `subscribe` "
	 "steps talk to, read by the application's providers (`jsonrpc`, `process`, `fs`); their status is the `services` "
	 "variable. A capability: revisions can't change it.",
	 "sections"},
    {"connections", R"json({"type": "object", "maxProperties": 64,
		"description": "Servers the screen talks to over HTTP or WebSocket (RFC 0003), and what it may ask them. Revisions can't change it.",
		"additionalProperties": {"type": "object", "required": ["url"], "additionalProperties": false,
		"properties": {"url": {"type": "string"}, "auth": {"anyOf": [{"type": "string"},
		{"type": "object", "required": ["tokenUrl", "clientId"], "additionalProperties": false, "properties": {
		"flow": {"enum": ["browser", "device"]}, "signInUrl": {"type": "string"}, "deviceUrl": {"type": "string"},
		"tokenUrl": {"type": "string"}, "clientId": {"type": "string"}, "issuer": {"type": "string"},
		"scopes": {"type": "array", "maxItems": 32, "items": {"type": "string"}}, "note": {"type": "string"}}}]},
		"headers": {"type": "object", "additionalProperties": {"type": "string"}},
		"timeout": {"type": "integer", "minimum": 1, "maximum": 600000},
		"retry": {"type": "object", "additionalProperties": false, "properties": {"times": {"type": "integer",
		"minimum": 0, "maximum": 5}, "after": {"type": "integer", "minimum": 1, "maximum": 60000}}},
		"reconnect": {"anyOf": [{"type": "boolean"}, {"type": "object", "properties": {"after": {"type": "integer"}}}]},
		"operations": {"type": "object", "maxProperties": 256, "additionalProperties": {"type": "object",
		"additionalProperties": false, "properties": {"method": {"enum": ["GET", "HEAD", "POST", "PUT", "PATCH", "DELETE"]},
		"path": {"type": "string"}, "query": {"type": "object"}, "body": {}, "form": {"type": "boolean"},
		"headers": {"type": "object", "additionalProperties": {"type": "string"}},
		"effect": {"enum": ["read", "write"]}, "stream": {"enum": ["events", "lines"]},
		"result": {"type": "string"}, "timeout": {"type": "integer", "minimum": 1, "maximum": 600000},
		"mock": {"type": "object", "properties": {"status": {"type": "integer"}, "body": {}}}}}},
		"messages": {"type": "object", "maxProperties": 256}, "note": {"type": "string"}}}})json",
	 "`{\"api\": {\"url\": \"https://api.example.com/v1\", \"operations\": {\"getProject\": {\"path\": "
	 "\"/projects/{id}\"}}}}`: servers that `request` and `subscribe` steps talk to, each request built from its "
	 "declared operation (method, `path` with `{name}` segments from `with`, `query`, `body`, `headers`, `effect`, "
	 "`stream`, `result`, `timeout`, `mock`); a `ws://`/`wss://` connection has `messages` and runs the actions "
	 "`name:open`, `name:<type>`, `name:error`, `name:closed`. The host allows the origins (`network.allow`). "
	 "`connections.<name>` is `{online, pending, state, error}`. `auth` signs its requests in: an object declares an "
	 "OAuth 2 provider (`flow` `browser` or `device`, `signInUrl` or `deviceUrl`, `tokenUrl`, `clientId`, `scopes`), "
	 "a string names another connection's; the `signIn` step signs in, the host adds the token, and `auth.<name>` is "
	 "`{state, account, expires, error}`, never a token. A capability: revisions use its operations, never add them.",
	 "sections"},
    {"persist", R"json({"type": "array", "items": {"type": "string", "pattern": "^state\\."},
		"description": "State paths kept on disk. Revisions can't change it."})json",
	 "`[\"state.habits\", ...]`: state paths kept on disk across restarts. A capability: revisions can't change it.",
	 "sections"},
    {"include", R"json({"anyOf": [{"type": "string"}, {"type": "array", "items": {"type": "string"}}]})json",
	 "`[\"parts/sidebar.json\", ...]`: files merged in at load, section by section (this file's entries win; `menus` "
	 "menu by menu, so several files add to one menu).",
	 "sections"},
    {"permissions", R"json({"type": "object", "additionalProperties": false,
		"description": "Who may change the running screen (VERSION_CONTROL.md). Revisions can't change it.",
		"properties": {"allow": {"type": "array", "items": {"$ref": "#/$defs/v1/permissionRule"}},
		"ask": {"type": "array", "items": {"$ref": "#/$defs/v1/permissionRule"}},
		"deny": {"type": "array", "items": {"$ref": "#/$defs/v1/permissionRule"}},
		"reviewers": {"type": "array", "items": {"type": "string"}}}})json",
	 "Who may change the running screen (see [Version control](VERSION_CONTROL.md)). Revisions can't change it.",
	 "sections"},
    {"features", R"json({"type": "object", "description": "Installed features, one record each (rfcs/0004-features.md)",
		"additionalProperties": {"type": "object", "required": ["release", "enabled", "provides"]}})json",
	 "`{\"wordCount\": {\"release\": \"1.0.0\", \"enabled\": true, \"order\": 10, \"requires\": {...}, "
	 "\"provides\": {...}}}`: installed features, each kept whole in its record (with `title`, `source`, `digest` and "
	 "`publisher`). The runtime merges the enabled ones in `order`, under their names (`state.wordCount`, "
	 "`wordCount.toggle`); views join the nodes whose `point` they name. Install, enable, disable and remove are "
	 "revisions on `/features/<name>`. A feature can't bring a capability or a control step.",
	 "sections"},
    {"exports", R"json({"type": "object", "description": "Values features read as app.<name>",
		"additionalProperties": {"type": "object", "required": ["type", "value"]}})json",
	 "`{\"editor.text\": {\"type\": \"string\", \"value\": \"{{state.text}}\"}}`: values features read as "
	 "`app.editor.text`.",
	 "sections"},
    {"imports", R"json({"type": "object", "description": "Aliases for installed features",
		"additionalProperties": {"type": "string"}})json",
	 "`{\"ui\": \"workbench@^1.0.0\"}`: `\"use\": \"ui.panel\"` builds the feature's `workbench.panel`, if its "
	 "release is in the range.",
	 "sections"},
    {"$schema", R"json({"type": "string"})json", "", ""},
};

/// The keys of a step: the steps, then the keys they read.
constexpr KeyReference stepTable[] = {
    {"set", "", R"(`{ "set": "state.current", "to": "{{index}}" }`)", "steps"},
    {"push", "", R"(`{ "push": "state.columns[c].cards", "value": "{{state.draft}}" }`)", "steps"},
    {"remove", "", R"(`{ "remove": "state.items", "at": 0 }`, or without `at` to delete a key of an object)",
	 "steps"},
    {"toggle", "", R"(`{ "toggle": "state.dark" }`)", "steps"},
    {"increment", "", R"(`{ "increment": "state.clicks", "by": 1 }`)", "steps"},
    {"if", "", R"(`{ "if": "{{len(state.draft) == 0}}", "then": [...], "else": [...] }`)", "steps"},
    {"forEach", "", R"(`{ "forEach": "state.items", "as": "item", "index": "i", "do": [...] }`)", "steps"},
    {"while", "", R"(`{ "while": "{{state.n < 10}}", "do": [...] }` (at most 10,000 iterations))", "steps"},
    {"run", "",
	 "`{ \"run\": \"moveCard\", \"with\": { \"from\": \"{{c}}\", \"to\": \"{{c + 1}}\" } }`: an action with variables; "
	 "the name may be computed, and `with` too (`\"{{item.with}}\"`, an object or null)",
	 "steps"},
    {"call", "",
	 "`{ \"call\": \"pickFile\", \"with\": {...}, \"into\": \"state.file\", \"onError\": [...] }`: a native function "
	 "(`Screen::onCall`); `error` in `onError`, `result` in `then`. A slow one (`Screen::onAsyncCall`) runs on a worker thread: "
	 "`\"loading\": \"state.busy\"` is true while it runs, `\"progress\": \"state.done\"` goes from 0 to 1, partial "
	 "results stream into `into`, `\"timeout\": 5000` ends it with the error `timeout`, `then` runs with `result`",
	 "steps"},
    {"cancel", "", R"(`{ "cancel": "search" }`: stop the running slow calls of a function; they change nothing more)",
	 "steps"},
    {"invoke", "",
	 "`{ \"invoke\": \"files.read\", \"with\": {\"path\": \"a.txt\"}, \"into\": \"state.text\" }`: a method of "
	 "a declared service, answered later; `loading`, `progress`, `timeout` (ms, default 30000), `id` (a new call with "
	 "the same id cancels it), `onDone` with `result`, `onError` with `error` (`{code, message, data}`); a stream's "
	 "events go to `append` (the last `keep`) or run `onEvent` with `events`",
	 "steps"},
    {"request", "",
	 "`{ \"request\": \"api.getProject\", \"with\": {\"id\": \"{{state.id}}\"}, \"into\": \"state.project\", "
	 "\"loading\": \"state.busy\" }`: an operation (or a WebSocket message) of a declared connection, built from its "
	 "declaration; the keys of `invoke`, with `error` as `{code, message, status, body}`. In a preview only a read "
	 "without a `mock` reaches the server: the others answer with their mock, or fail with `preview`",
	 "steps"},
    {"subscribe", "",
	 "`{ \"subscribe\": \"build.output\", \"append\": \"state.log\", \"keep\": 500, \"id\": \"build\" }`: a "
	 "stream of a declared service or connection until `cancel` or its node goes; `do` runs with `events`",
	 "steps"},
    {"respond", "",
	 "`{ \"respond\": \"{{state.answer}}\" }` or `{ \"respond\": null, \"error\": \"no\" }`: answers the "
	 "service request that ran the action `service:method` (its `request`); a control step",
	 "steps"},
    {"signIn", "",
	 "`{ \"signIn\": \"api\", \"onDone\": [...], \"onError\": [...] }`: signs in to the provider a connection's "
	 "`auth` declares (OAuth 2: the browser with PKCE, or a device code), then its requests carry the token. The "
	 "token never reaches the document or the state: the `auth` variable shows `{state, account, expires}`, and "
	 "`error` in `onError` is `{code, message}`. A control step",
	 "steps"},
    {"signOut", "", R"(`{ "signOut": "api" }`: forgets the sign-in and its kept token; a control step)", "steps"},
    {"emit", "", R"(`{ "emit": "saved", "data": {...} }`: an event for every connected peer)", "steps"},
    {"send", "", R"(`{ "send": "host", "data": {...} }`: a message on a declared channel)", "steps"},
    {"connect", "", R"(`{ "connect": "host" }`: connect a declared channel now)", "steps"},
    {"patch", "", R"(`{ "patch": [JSON Patch ops], "message": "..." }`: the app revises its own document)", "steps"},
    {"navigate", "", R"(`{ "navigate": "settings" }` or `"back"`)", "steps"},
    {"focus", "", R"(`{ "focus": "#input" }`, `{ "scrollTo": "#row7" }` (after the update))", "steps"},
    {"scrollTo", "", "", "steps"},
    {"dismiss", R"json({"const": true})json",
	 "`{ \"dismiss\": true }`: closes the open overlays, e.g. a menu once an item is chosen", "steps"},
    {"after", "", R"(`{ "after": 2000, "do": [...] }`: later)", "steps"},
    {"debounce", "", R"(`{ "debounce": 300, "do": [...] }`: later, unless run again in the meantime (search boxes))",
	 "steps"},
    {"animate", "",
	 R"(`{ "animate": "state.progress", "to": 100, "ms": 300, "easing": "out" }` (`linear`, `in`, `out`, `inOut`))",
	 "steps"},
    {"copy", "", R"(`{ "copy": "{{state.link}}" }`: to the clipboard)", "steps"},
    {"log", "", R"(`{ "log": "n is {{state.n}}" }`, a comment, `{ "stop": true }` (end the action))", "steps"},
    {"note", "", "", "steps"},
    {"stop", "", "", "steps"},
    {"preview", "",
	 "Review and history, e.g. `{ \"accept\": \"{{proposals[0].id}}\", \"head\": \"{{proposals[0].head}}\" }` "
	 "(`accept` names the head the user reviewed; refused if the proposal moved since)",
	 "steps"},
    {"endPreview", R"json({"const": true})json", "", "steps"},
    {"accept", "", "", "steps"},
    {"reject", "", "", "steps"},
    {"restore", "", "", "steps"},
    {"revert", "", "", "steps"},
    {"undo", R"json({"const": true})json", "", "steps"},
    {"to", "", "", ""},
    {"value", "", "", ""},
    {"at", "", "", ""},
    {"by", "", "", ""},
    {"as", "", "", ""},
    {"index", "", "", ""},
    {"with", "", "", ""},
    {"into", "", "", ""},
    {"loading", "", "", ""},
    {"progress", "", "", ""},
    {"timeout", "", "", ""},
    {"append", "", "", ""},
    {"keep", R"json({"anyOf": [{"type": "integer", "minimum": 1, "maximum": 10000}, {"type": "string"}]})json", "",
	 ""},
    {"id", "", "", ""},
    {"error", "", "", ""},
    {"data", "", "", ""},
    {"message", "", "", ""},
    {"ms", "", "", ""},
    {"easing", R"json({"enum": ["linear", "in", "out", "inOut"]})json", "", ""},
    {"head", "", "", ""},
    {"reason", "", "", ""},
    {"then", "steps", "", ""},
    {"else", "steps", "", ""},
    {"do", "steps", "", ""},
    {"onError", "steps", "", ""},
    {"onEvent", "steps", "", ""},
    {"onDone", "steps", "", ""},
};

/// The shared values: "$defs/v1" entries the other schemas refer to.
constexpr KeyReference definitionTable[] = {
    {"permissionRule", R"json({"type": "string", "pattern": "^[^() ]+(\\((#|/)[^ ]*\\))?$",
		"description": "\"author\" or \"author(pattern)\", e.g. \"agent(#board)\""})json",
	 "", ""},
    {"hexColor", R"json({"type": "string", "pattern": "^#([0-9a-fA-F]{3}|[0-9a-fA-F]{6}|[0-9a-fA-F]{8})$"})json", "",
	 ""},
    {"stateType",
	 R"json({"description": "A type, written as JSON Schema (the subset the runtime reads), or true (anything) or false (nothing)",
		"type": ["object", "boolean"], "additionalProperties": false,
		"properties": {"type": {"oneOf": [{"$ref": "#/$defs/v1/stateTypeName"}, {"type": "array",
		"minItems": 1, "items": {"$ref": "#/$defs/v1/stateTypeName"}}]}, "enum": {"type": "array",
		"minItems": 1}, "const": {}, "properties": {"type": "object",
		"additionalProperties": {"$ref": "#/$defs/v1/stateType"}}, "required": {"type": "array",
		"items": {"type": "string"}}, "additionalProperties": {"$ref": "#/$defs/v1/stateType"},
		"items": {"$ref": "#/$defs/v1/stateType"}, "minimum": {"type": "number"},
		"maximum": {"type": "number"}, "minLength": {"type": "integer", "minimum": 0},
		"maxLength": {"type": "integer", "minimum": 0}, "minItems": {"type": "integer", "minimum": 0},
		"maxItems": {"type": "integer", "minimum": 0}, "description": {"type": "string"},
		"title": {"type": "string"}, "note": {"type": "string"}, "default": {},
		"examples": {"type": "array"}}})json",
	 "", ""},
    {"stateTypeName", R"json({"enum": ["string", "number", "integer", "boolean", "object", "array", "null"]})json", "",
	 ""},
    {"color", R"json({"description": "\"#rgb\", \"#rrggbb\", \"#rrggbbaa\", \"@name\" or \"transparent\"",
		"anyOf": [{"type": "string",
		"pattern": "^(#([0-9a-fA-F]{3}|[0-9a-fA-F]{6}|[0-9a-fA-F]{8})|@[A-Za-z_][A-Za-z0-9_.-]*|transparent)$"},
		{"$ref": "#/$defs/v1/dynamic"}]})json",
	 "", ""},
    {"length", R"json({"description": "Pixels (120 or \"120px\") or a percentage of the parent (\"50%\")",
		"anyOf": [{"type": "number"}, {"type": "string", "pattern": "^-?[0-9]+(\\.[0-9]+)?(px|%)?$"},
		{"$ref": "#/$defs/v1/dynamic"}]})json",
	 "", ""},
    {"edge", R"json({"description": "A length from the parent's edge, or another node's edge: \"#header.bottom + 8\"",
		"anyOf": [{"$ref": "#/$defs/v1/length"}, {"type": "string",
		"pattern": "^#[A-Za-z0-9_-]+\\.(top|bottom|left|right)( *[-+] *[0-9]+(\\.[0-9]+)?)?$"}]})json",
	 "", ""},
    {"align", R"json({"anyOf": [{"enum": ["start", "center", "end"]}, {"$ref": "#/$defs/v1/dynamic"}]})json", "", ""},
    {"crossAlign",
	 R"json({"anyOf": [{"enum": ["start", "center", "end", "stretch"]}, {"$ref": "#/$defs/v1/dynamic"}]})json", "", ""},
    {"track",
	 R"json({"description": "A grid column or row: pixels (120), a percentage of the grid (\"25%\"), a fraction of the room left (\"1fr\"), \"auto\" (its largest child) or \"minmax(minimum, maximum)\"",
		"anyOf": [{"type": "number", "minimum": 0}, {"type": "string",
		"pattern": "^(auto|[0-9]+(\\.[0-9]+)?(px|%|fr)?|minmax\\( *(auto|[0-9]+(\\.[0-9]+)?(px|%|fr)?) *, *(auto|[0-9]+(\\.[0-9]+)?(px|%|fr)?) *\\))$"},
		{"$ref": "#/$defs/v1/dynamic"}]})json",
	 "", ""},
    {"stateBlock", R"json({"type": "object",
		"description": "Properties shown in that state (any visual property, or a style)"})json",
	 "", ""},
    {"dynamic", R"json({"description": "A {{binding or expression}}, an operator ({\"$clamp\": [...]}), or a \"@token\"",
		"anyOf": [{"type": "string", "pattern": "\\{\\{.*\\}\\}"}, {"$ref": "#/$defs/v1/expression"},
		{"$ref": "#/$defs/v1/tokenName"}]})json",
	 "", ""},
    {"tokenName", R"json({"description": "\"@name\" of a token of \"tokens\"", "type": "string",
		"pattern": "^@[A-Za-z_][A-Za-z0-9_.-]*$"})json",
	 "", ""},
    {"tokenValue", R"json({"anyOf": [{"type": "number"}, {"$ref": "#/$defs/v1/hexColor"}, {"const": "transparent"},
		{"$ref": "#/$defs/v1/tokenName"}]})json",
	 "", ""},
    {"pixels", R"json({"description": "A number of pixels, 0 or more", "anyOf": [{"type": "number", "minimum": 0},
		{"$ref": "#/$defs/v1/dynamic"}]})json",
	 "", ""},
    {"flag", R"json({"anyOf": [{"type": "boolean"}, {"$ref": "#/$defs/v1/dynamic"}]})json", "", ""},
    {"handler", R"json({"description": "An action name, a step, or steps", "anyOf": [{"type": "string"},
		{"$ref": "#/$defs/v1/steps"}]})json",
	 "", ""},
    {"command", R"json({"type": "object", "additionalProperties": false,
		"description": "An action that describes itself; \"enablement\" is checked for keys, menus and palettes",
		"properties": {"title": {"type": "string"}, "category": {"type": "string"}, "icon": {"type": "string"},
		"enablement": {}, "description": {"type": "string"}, "do": {"$ref": "#/$defs/v1/handler"}}})json",
	 "", ""},
    {"binding", R"json({"type": "object", "required": ["do"], "additionalProperties": false,
		"description": "A key binding: runs \"do\" (an action name or steps) when \"when\" is true",
		"properties": {"when": {}, "do": {"$ref": "#/$defs/v1/handler"}, "with": {"type": "object"}}})json",
	 "", ""},
    {"keyHandler", R"json({"description": "A handler, a binding, or bindings (the first that applies runs)",
		"anyOf": [{"$ref": "#/$defs/v1/handler"}, {"$ref": "#/$defs/v1/binding"},
		{"type": "array", "minItems": 1, "items": {"$ref": "#/$defs/v1/binding"}}]})json",
	 "", ""},
    {"steps", R"json({"anyOf": [{"$ref": "#/$defs/v1/step"}, {"type": "array",
		"items": {"$ref": "#/$defs/v1/step"}}]})json",
	 "", ""},
};

} // namespace

std::span<const KeyReference> sectionKeys() { return sectionTable; }

std::span<const KeyReference> stepKeys() { return stepTable; }

std::span<const KeyReference> definitions() { return definitionTable; }

} // namespace affresco::format
