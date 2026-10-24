/// @file
/// @brief format::sectionKeys(), stepKeys(), definitions(): the reference of the sections, steps and shared values.
///
/// Each entry is a key: the schema of its value and its row in SCREEN_FORMAT.md (Reference.hpp). Which keys exist,
/// and in what order, the registries decide; ReferenceTest checks this table against them.

#include "Reference.hpp"

#include <array>

namespace affresco::format {

namespace {

/// The top-level sections, in the order of the table.
constexpr auto sectionTable = std::to_array<KeyReference>({
    {"version", R"json({"const": 2})json",
	 "Required. `2` (a file of version 1 is read by migrating it: \"Migrating from version 1\").", "sections"},
    {"root", R"json({"$ref": "#/$defs/v2/node",
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
		"size": {"anyOf": [{"type": "number", "exclusiveMinimum": 0}, {"$ref": "#/$defs/v2/tokenName"}]}, "fallback": {"type": "array",
		"description": "Other fonts in \"fonts\", asked in order for characters this one lacks; drawn at this font's size",
		"maxItems": 8, "items": {"type": "string"}}, "bold": {"type": "boolean",
		"description": "Thickened outlines, for a file that has only a regular weight"}, "italic": {"type": "boolean",
		"description": "Slanted outlines, for a file that has only an upright style"}}}})json",
	 "`name → { \"file\", \"size\", \"fallback\", \"bold\", \"italic\" }`, paths relative to the screen file (TrueType "
	 "or OpenType, a collection's first font). `default` is used by text without a `font`. Text is UTF-8, shaped per "
	 "script "
	 "(kerning, ligatures, Arabic, Indic; right-to-left runs), colour emoji from COLR fonts. `\"fallback\": "
	 "[\"symbols\"]` names other fonts asked in order for a character this one lacks; a character none has shows as "
	 "`?` (macOS menu key labels use ⌘ ⇧ ⌥ ⌃, which JetBrains Mono has). `\"bold\": true` and `\"italic\": true` "
	 "thicken or slant the outlines, for headings from a file with one weight: `\"heading\": { \"file\": "
	 "\"Roboto-Regular.ttf\", \"size\": 20, \"bold\": true }`; a label's `runs` keep its font's style unless they "
	 "set their own.",
	 "sections"},
    {"languages", R"json({"type": "object", "maxProperties": 64,
		"description": "Code languages an editor's \"language\" names: patterns whose scopes the @syntax.* tokens colour, and editing rules",
		"additionalProperties": {"type": "object", "additionalProperties": false, "properties": {
		"title": {"type": "string"}, "aliases": {"type": "array", "maxItems": 64, "items": {"type": "string"}},
		"files": {"type": "array", "maxItems": 64, "items": {"type": "string"}},
		"comments": {"type": "object", "additionalProperties": false, "properties": {"line": {"type": "string"},
		"block": {"type": "array", "minItems": 2, "maxItems": 2, "items": {"type": "string"}}}},
		"brackets": {"$ref": "#/$defs/v2/characterPairs"}, "pairs": {"$ref": "#/$defs/v2/characterPairs"},
		"indent": {"type": "object", "additionalProperties": false, "properties": {"increase": {"type": "string"},
		"decrease": {"type": "string"}}}, "wordPattern": {"type": "string"},
		"patterns": {"type": "array", "items": {"type": "object"}},
		"repository": {"type": "object", "additionalProperties": {"type": "object"}}}}})json",
	 "`name → language`: the code languages an editor's `language` names (by name, alias or file name). Each has "
	 "`title`, `aliases`, `files` (`[\"*.py\", \"CMakeLists.txt\"]`), `comments` (`{ \"line\": \"//\", \"block\": "
	 "[\"/*\", \"*/\"] }`), `brackets` and `pairs` (`[[\"(\", \")\"], ...]`: Enter between brackets indents, typing "
	 "an opener of a pair adds its closer), `indent` (`{ \"increase\": regex, \"decrease\": regex }`), "
	 "`wordPattern` (regex), and TextMate-style `patterns` with a `repository`: `{ \"match\": regex, \"scope\": "
	 "\"keyword\" }`, `{ \"begin\": regex, \"end\": regex, \"scope\", \"contentScope\", \"patterns\" }` (an `end` may "
	 "use the begin's groups, `\\1`), `captures` / `beginCaptures` / `endCaptures` (`{ \"1\": { \"scope\": "
	 "\"type\" } }`) and `{ \"include\": \"#name\" }` or `\"$self\"`. A scope is coloured by `@syntax.<scope>`, or "
	 "its parent's (`keyword.control` by `@syntax.keyword`). Regular expressions are PCRE2, each find limited in "
	 "steps and each line in time; a language has at most 2,048 rules. Ready-made languages are data files to "
	 "`include` (`examples/shared/languages/`).",
	 "sections"},
    {"colors", R"json({"type": "object",
		"description": "Named colors (\"@name\"), or per theme: {\"light\": ..., \"dark\": ...}",
		"additionalProperties": {"anyOf": [{"$ref": "#/$defs/v2/hexColor"}, {"type": "object",
		"minProperties": 1, "additionalProperties": {"$ref": "#/$defs/v2/hexColor"}}]}})json",
	 "`name → \"#rrggbb\"`, or per theme `name → { \"light\": \"#fff\", \"dark\": \"#111827\" }`. Used as `\"@name\"`.",
	 "sections"},
    {"tokens", R"json({"type": "object", "maxProperties": 4096,
		"description": "Design tokens (\"@name\"): numbers and colors, aliases (\"@other\"), or per theme: {\"light\": ..., \"dark\": ...}",
		"propertyNames": {"pattern": "^[A-Za-z_][A-Za-z0-9_.-]*$"},
		"additionalProperties": {"anyOf": [{"$ref": "#/$defs/v2/tokenValue"}, {"type": "object",
		"minProperties": 1, "additionalProperties": {"$ref": "#/$defs/v2/tokenValue"}}]}})json",
	 "`name → value`: a number (spacing, a radius, a type size), a colour (`\"#rrggbb\"`, `\"transparent\"`), an alias "
	 "(`\"@other\"`, of a token or of `colors`), or per theme `{ \"light\": ..., \"dark\": ... }` of those. Used as "
	 "`\"@name\"` wherever a number or a colour fits (`\"padding\": \"@space.m\"`, a font's `\"size\"`). Switched by "
	 "`theme` like `colors`; a name in both is an error. Aliases go at most 16 deep.",
	 "sections"},
    {"theme", R"json({"description": "Which color variant is shown, e.g. \"{{state.theme}}\""})json",
	 "Which variant of the colors and tokens is shown, usually bound: `\"{{state.theme}}\"`. Without a match, the "
	 "first variant.",
	 "sections"},
    {"strings", R"json({"type": "object", "maxProperties": 66,
		"description": "Translations per locale for t(): {\"default\": \"en\", \"locale\": \"{{state.locale}}\", \"en\": {key: message}}",
		"properties": {"default": {"type": "string"}, "locale": {"type": "string"}},
		"additionalProperties": {"type": "object", "maxProperties": 10000, "additionalProperties": {"type": "string",
		"maxLength": 4096}}})json",
	 "`locale → { key → message }`, with `\"default\"` (the locale a key falls back to) and `\"locale\"` (the locale "
	 "shown, usually bound: `\"{{state.locale}}\"`; else the host's, from the system). `t(\"key\", {params})` "
	 "looks a key up in the locale, then its parents (`fr-CA`, then `fr`), then the default; a key found nowhere shows "
	 "as itself. See Strings and locales.",
	 "sections"},
    {"styles", R"json({"type": "object", "additionalProperties": {"$ref": "#/$defs/v2/node"}})json",
	 "`name → properties`, applied with `\"style\": \"name\"` or `[\"a\", \"b\"]`. A style may itself have a `style`.",
	 "sections"},
    {"components", R"json({"type": "object", "additionalProperties": {"type": "object", "required": ["node"],
		"additionalProperties": false, "properties": {"params": {"type": "object"},
		"node": {"$ref": "#/$defs/v2/node"}, "note": {"type": "string"}}}})json",
	 "`name → { \"params\": {defaults}, \"node\": node }`: reusable nodes with parameters (see Components).",
	 "sections"},
    {"screens", R"json({"type": "object", "description": "Screens an \"outlet\" shows; {\"navigate\": name} switches",
		"additionalProperties": {"$ref": "#/$defs/v2/node"}})json",
	 "`name → node`: the screens an `outlet` shows; `{\"navigate\": \"name\"}` switches. The first is shown first.",
	 "sections"},
    {"templates", R"json({"type": "object", "additionalProperties": {"$ref": "#/$defs/v2/node"}})json",
	 "`name → node`, built by code on demand (`Screen::instantiate`).", "sections"},
    {"overlays", R"json({"type": "object", "additionalProperties": {"$ref": "#/$defs/v2/node"}})json",
	 "`name → node`: laid over the window, above the root and its own overlays, e.g. from an `include`d add-on. Each "
	 "is an `overlay` placed in the window by its own anchors unless it says otherwise; it may `use` a component, and "
	 "isn't built while its `when` doesn't hold.",
	 "sections"},
    {"state", R"json({"type": "object"})json", "The app's data, any JSON object.", "sections"},
    {"assets", R"json({"type": "object", "maxProperties": 1000,
		"description": "The files revisions brought, kept with the history, read as asset:<name>",
		"additionalProperties": {"type": "object", "required": ["type", "size", "sha256"], "additionalProperties": false,
		"properties": {"type": {"type": "string"}, "size": {"type": "integer", "minimum": 0},
		"sha256": {"type": "string", "pattern": "^[0-9a-f]{64}$"}}}})json",
	 "`name → {type, size, sha256}`: files revisions brought, kept with the history, read as `\"asset:<name>\"` "
	 "(see Assets).",
	 "sections"},
    {"stateTypes", R"json({"type": "object",
		"description": "A type per state key, written as JSON Schema: type, enum, const, properties, required, additionalProperties, items, minimum, maximum, minLength, maxLength, minItems, maxItems",
		"additionalProperties": {"$ref": "#/$defs/v2/stateType"}})json",
	 "`key → type`: what each state key holds, written as JSON Schema (see Typed state).", "sections"},
    {"computed", R"json({"type": "object", "description": "Values derived from the state, read as computed.name"})json",
	 "`name → expression`: values derived from the state, read as `computed.name` (see Values).", "sections"},
    {"watch", R"json({"type": "object", "additionalProperties": {"$ref": "#/$defs/v2/handler"},
		"description": "\"state.path\" -> handler, run when it changed"})json",
	 "`\"state.path\" → handler`: runs after an update in which that value changed (`event.value`, `event.previous`).",
	 "sections"},
    {"actions", R"json({"type": "object", "additionalProperties": {"anyOf": [{"$ref": "#/$defs/v2/steps"},
		{"$ref": "#/$defs/v2/command"}]}})json",
	 "`name → steps`, run by handlers, `run` steps, shortcuts and channels; or a command `{ \"title\", "
	 "\"category\", \"icon\", \"enablement\", \"description\", \"do\" }`, listed by the `commands` variable "
	 "(see Commands and keybindings).",
	 "sections"},
    {"keys", R"json({"type": "object", "additionalProperties": {"$ref": "#/$defs/v2/keyHandler"},
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
	 "Programs the screen talks to (see Channels). A capability: revisions can't change it.", "capabilities"},
    {"services", R"json({"type": "object", "maxProperties": 64,
		"description": "Services the screen uses (RFC 0002), each {\"provider\": ...}. Revisions can't change it.",
		"additionalProperties": {"type": "object", "required": ["provider"],
		"properties": {"provider": {"type": "string"}}}})json",
	 "`{\"files\": {\"provider\": \"fs\", \"root\": \"workspace\"}}`: services that `invoke` and `subscribe` "
	 "steps talk to, read by the application's providers (`jsonrpc`, `process`, `fs`); their status is the `services` "
	 "variable. A capability: revisions can't change it.",
	 "capabilities"},
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
	 "capabilities"},
    {"persist", R"json({"type": "array", "items": {"type": "string", "pattern": "^state\\."},
		"description": "State paths kept on disk. Revisions can't change it."})json",
	 "`[\"state.habits\", ...]`: state paths kept on disk across restarts. A capability: revisions can't change it.",
	 "capabilities"},
    {"include", R"json({"anyOf": [{"type": "string"}, {"type": "array", "items": {"type": "string"}}]})json",
	 "`[\"parts/sidebar.json\", ...]`: files merged in at load, section by section (this file's entries win; `menus` "
	 "menu by menu, so several files add to one menu; `persist` lists join).",
	 "sections"},
    {"permissions", R"json({"type": "object", "additionalProperties": false,
		"description": "Who may change the running screen (VERSION_CONTROL.md). Revisions can't change it.",
		"properties": {"allow": {"type": "array", "items": {"$ref": "#/$defs/v2/permissionRule"}},
		"ask": {"type": "array", "items": {"$ref": "#/$defs/v2/permissionRule"}},
		"deny": {"type": "array", "items": {"$ref": "#/$defs/v2/permissionRule"}},
		"reviewers": {"type": "array", "items": {"type": "string"}},
		"review": {"type": "object", "additionalProperties": false,
		"description": "What a proposal needs before it merges.",
		"properties": {"approvals": {"type": "integer", "minimum": 1, "maximum": 20},
		"behaviourApprovals": {"type": "integer", "minimum": 1, "maximum": 20},
		"owners": {"type": "object", "additionalProperties": {"type": "array", "minItems": 1, "items": {"type": "string"}}},
		"grantsLast": {"type": "string", "pattern": "^[1-9][0-9]{0,5}[mhd]$"}}},
		"budgets": {"type": "array", "maxItems": 64, "description": "The most nodes an author changes in one revision.",
		"items": {"type": "object", "additionalProperties": false, "required": ["author"],
		"properties": {"author": {"type": "string"}, "added": {"type": "integer", "minimum": 0, "maximum": 100000},
		"removed": {"type": "integer", "minimum": 0, "maximum": 100000},
		"changed": {"type": "integer", "minimum": 0, "maximum": 100000}, "over": {"enum": ["ask", "deny"]}}}},
		"data": {"type": "array", "maxItems": 64, "description": "The paths an author may read, write or never touch.",
		"items": {"type": "object", "additionalProperties": false, "required": ["author"],
		"properties": {"author": {"type": "string"},
		"read": {"type": "array", "maxItems": 64, "items": {"type": "string", "pattern": "^/"}},
		"write": {"type": "array", "maxItems": 64, "items": {"type": "string", "pattern": "^/"}},
		"never": {"type": "array", "maxItems": 64, "items": {"type": "string", "pattern": "^/"}}}}}}})json",
	 "Who may change the running screen (see [Version control](VERSION_CONTROL.md)). Revisions can't change it.",
	 "capabilities"},
    {"features", R"json({"type": "object", "description": "Installed features, one record each (rfcs/0004-features.md)",
		"additionalProperties": {"type": "object", "required": ["release", "enabled", "provides"]}})json",
	 "`{\"wordCount\": {\"release\": \"1.0.0\", \"enabled\": true, \"order\": 10, \"requires\": {...}, "
	 "\"provides\": {...}}}`: installed features, each kept whole in its record (with `title`, `source`, `digest` and "
	 "`publisher`). The runtime merges the enabled ones in `order`, under their names (`state.wordCount`, "
	 "`wordCount.toggle`); views join the nodes whose `point` they name. Install, enable, disable and remove are "
	 "revisions on `/features/<name>`. A feature can't bring a capability or a control step.",
	 "capabilities"},
    {"exports", R"json({"type": "object", "description": "Values features read as app.<name>",
		"additionalProperties": {"type": "object", "required": ["type", "value"]}})json",
	 "`{\"editor.text\": {\"type\": \"string\", \"value\": \"{{state.text}}\"}}`: values features read as "
	 "`app.editor.text`.",
	 "capabilities"},
    {"imports", R"json({"type": "object", "description": "Aliases for installed features",
		"additionalProperties": {"type": "string"}})json",
	 "`{\"ui\": \"workbench@^1.0.0\"}`: `\"use\": \"ui.panel\"` builds the feature's `workbench.panel`, if its "
	 "release is in the range.",
	 "capabilities"},
    {"images", R"json({"type": "object", "maxProperties": 16,
		"description": "Raster images the image step edits, with their tools and lowered limits",
		"additionalProperties": {"type": "object", "required": ["width", "height"],
		"properties": {"width": {"type": "integer", "minimum": 1}, "height": {"type": "integer", "minimum": 1},
		"format": {"enum": ["rgba8", "rgba16f"]}, "background": {"type": "string"},
		"into": {"type": "string", "pattern": "^state\\."}, "tool": {"type": "string"},
		"tools": {"type": "object", "maxProperties": 64, "additionalProperties": {"type": "object",
		"required": ["gesture"], "properties": {"gesture": {"type": "string"}}}},
		"limits": {"type": "object", "additionalProperties": {"type": "integer", "minimum": 1}}}}})json",
	 "`{\"canvas\": {\"width\": 800, \"height\": 600, \"into\": \"state.canvas\", \"tools\": {\"brush\": "
	 "{\"gesture\": \"paint\", \"size\": 12}}}}`: the images the `image` step edits; `tools` names the "
	 "library's gestures with their settings, and `limits` only lowers the library's (see Images). Revisions can't "
	 "change it.",
	 "capabilities"},
    {"audio", R"json({"type": "object", "maxProperties": 32,
		"description": "Sounds the audio step plays, each from one source. Only the person accepts a change to it.",
		"additionalProperties": {"type": "object", "additionalProperties": false,
		"properties": {"file": {"type": "string"}, "url": {"type": "string", "pattern": "^https?://"},
		"source": {"type": "string"}, "tone": {"type": "object", "required": ["frequency", "seconds"],
		"additionalProperties": false, "properties": {"frequency": {"type": "number", "minimum": 1, "maximum": 20000},
		"seconds": {"type": "number", "minimum": 0.001, "maximum": 86400},
		"amplitude": {"type": "number", "minimum": 0, "maximum": 1}}},
		"into": {"type": "string", "pattern": "^state\\."}, "volume": {"type": "number", "minimum": 0, "maximum": 4},
		"pan": {"type": "number", "minimum": -1, "maximum": 1}, "rate": {"type": "number", "minimum": 0.25, "maximum": 4},
		"loop": {"type": "boolean"}, "updates": {"type": "number", "minimum": 50, "maximum": 5000},
		"maxBytes": {"type": "integer", "minimum": 1024, "maximum": 67108864}, "note": {"type": "string"}}}})json",
	 "`{\"song\": {\"file\": \"sounds/song.mp3\", \"into\": \"state.song\", \"loop\": true}}`: the sounds the `audio` "
	 "step plays, each from one source: a `file` (WAV, FLAC or MP3) in the screen's folder or one the host allows, a "
	 "`url` the host's network allows (streamed), a `source` the host names (`Screen::setAudioSource`), or a `tone` "
	 "(`frequency`, `seconds`, `amplitude`); with `volume`, `pan`, `rate`, `loop`, `updates` (ms between position "
	 "updates) and a URL's `maxBytes`. `into` gets `{status, playing, position, duration, progress, volume, pan, rate, "
	 "loop, level, underruns, error}`; the actions `name:started`, `name:ended`, `name:underrun` and `name:error` run "
	 "(see Audio). A `file` may be `asset:<name>`, also a sound description. A revision may give a sound a `file` or a "
	 "`tone` (never a `url` or a `source`), and only the person accepts it (Assets).",
	 "capabilities"},
    {"points", R"json({"type": "object", "description": "Contribution points another app's features may fill",
		"additionalProperties": {"type": "object", "required": ["slot"],
		"properties": {"slot": {"enum": ["dock", "panel", "menu", "toolbar"]},
		"receives": {"type": "array", "maxItems": 32, "items": {"type": "string"}},
		"description": {"type": "string"}, "note": {"type": "string"}}}})json",
	 "`{\"panels.dock\": {\"slot\": \"dock\", \"receives\": [\"doc.layers\", \"layers.move\"]}}`: where a feature "
	 "from another app may put its views (the node whose `point` has the name) and the exports it receives; the "
	 "action exports a point receives are the host actions such features may run. Revisions can't change it.",
	 "capabilities"},
    {"$schema", R"json({"type": "string"})json", "", ""},
});

/// The keys of a step: the steps, then the keys they read.
constexpr auto stepTable = std::to_array<KeyReference>({
    {"set", "", R"(`{ "set": "state.current", "to": "{{index}}" }`)", "steps"},
    {"push", "", R"(`{ "push": "state.columns[c].cards", "value": "{{state.draft}}" }`)", "steps"},
    {"remove", "", R"(`{ "remove": "state.items", "at": 0 }`, or without `at` to delete a key of an object)", "steps"},
    {"toggle", "", R"(`{ "toggle": "state.dark" }`)", "steps"},
    {"increment", "", R"(`{ "increment": "state.clicks", "by": 1 }`)", "steps"},
    {"if", "", R"(`{ "if": "{{length(state.draft) == 0}}", "then": [...], "else": [...] }`)", "steps"},
    {"forEach", "", R"(`{ "forEach": "state.items", "as": "item", "index": "i", "do": [...] }`)", "steps"},
    {"while", "", R"(`{ "while": "{{state.n < 10}}", "do": [...] }` (at most 10,000 iterations))", "steps"},
    {"run", "",
	 "`{ \"run\": \"moveCard\", \"with\": { \"from\": \"{{c}}\", \"to\": \"{{c + 1}}\" } }`: an action with variables; "
	 "the name may be computed, and `with` too (`\"{{item.with}}\"`, an object or null)",
	 "steps"},
    {"call", "",
	 "`{ \"call\": \"pickFile\", \"with\": {...}, \"into\": \"state.file\", \"onError\": [...] }`: a native function "
	 "(`Screen::onCall`); `error` in `onError`, `result` in `then`. A slow one (`Screen::onAsyncCall`) runs on a "
	 "worker thread: "
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
    {"chooseFolder", "",
	 "`{ \"chooseFolder\": \"workspace\", \"onDone\": [...] }`: after the event, "
	 "the person chooses the folder a root names in the OS's dialog (if the host lets them); its services start again "
	 "on it. `result` is `{name}` (never where it is), `error` `{code, message}`, a cancel runs nothing. A control "
	 "step",
	 "steps"},
    {"emit", "", R"(`{ "emit": "saved", "data": {...} }`: an event for every connected peer)", "steps"},
    {"send", "", R"(`{ "send": "host", "data": {...} }`: a message on a declared channel)", "steps"},
    {"connect", "", R"(`{ "connect": "host" }`: connect a declared channel now)", "steps"},
    {"ask", "",
	 "`{ \"ask\": \"{{state.draft}}\", \"into\": \"state.asked\" }`: asks the agents in plain words, with "
	 "`options` (an object) passed on to them; replies run `ask:reply`, never as the person's decision (Asking the "
	 "agents). A control step",
	 "steps"},
    {"attach", "",
	 "`{ \"attach\": { \"accept\": [\"audio/*\"] }, \"into\": \"state.files\" }`: the person's files as "
	 "attachments (Assets). A control step",
	 "steps"},
    {"patch", "", R"(`{ "patch": [JSON Patch ops], "message": "..." }`: the app revises its own document)", "steps"},
    {"navigate", "", R"(`{ "navigate": "settings" }` or `"back"`)", "steps"},
    {"focus", "",
	 R"(`{ "focus": "#input" }` (a container gives it to its first focusable node), `{ "scrollTo": "#row7" }` )"
	 R"((after the update, waiting up to two frames for a node that is still being laid out))",
	 "steps"},
    {"scrollTo", "", "", "steps"},
    {"edit", "", "`{ \"edit\": \"sortLines\", \"in\": \"#code\" }`: a code editor's command (Editor steps)", "steps"},
    {"in", "", "", ""},
    {"line", "", "", ""},
    {"column", "", "", ""},
    {"toLine", "", "", ""},
    {"toColumn", "", "", ""},
    {"prefix", "", "", ""},
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
    {"attachments", "", "", ""},
    {"loading", "", "", ""},
    {"progress", "", "", ""},
    {"timeout", "", "", ""},
    {"options", R"json({"anyOf": [{"type": "object"}, {"$ref": "#/$defs/v2/dynamic"}]})json", "", ""},
    {"image", "", R"(`{ "image": "undo", "of": "canvas" }`: runs an image operation (see Images))", "steps"},
    {"of", "", "", ""},
    {"audio", R"json({"anyOf": [{"enum": ["play", "pause", "toggle", "stop", "seek", "volume", "pan", "rate", "loop",
		"master"]}, {"$ref": "#/$defs/v2/dynamic"}]})json",
	 R"(`{ "audio": "play", "of": "song" }`: see Audio)", "steps"},
    {"from", "", "", ""},
    {"camera", "",
	 R"(`{ "camera": "#viewport", "view": "front" }`: moves a scene's camera (`view`, `frame`, `projection`, `orbit`, )"
	 R"(`zoom`: see `scene`))",
	 "steps"},
    {"view", R"json({"anyOf": [{"enum": ["front", "back", "left", "right", "top", "bottom"]},
		{"$ref": "#/$defs/v2/dynamic"}]})json",
	 "", ""},
    {"frame", "", "", ""},
    {"projection", R"json({"anyOf": [{"enum": ["perspective", "orthographic", "toggle"]},
		{"$ref": "#/$defs/v2/dynamic"}]})json",
	 "", ""},
    {"orbit", "", "", ""},
    {"zoom", "", "", ""},
    {"transform", "",
	 R"(`{ "transform": "#viewport", "mode": "translate" }`: a scene's modal move of its selection, after the )"
	 R"(pointer until a click; `axis` locks it, `value` types its amount (units, degrees or a factor; null follows )"
	 R"(the pointer again), `end` (confirm, cancel) ends it)",
	 "steps"},
    {"mode", R"json({"anyOf": [{"enum": ["translate", "rotate", "scale"]}, {"$ref": "#/$defs/v2/dynamic"}]})json", "",
	 ""},
    {"axis",
	 R"json({"anyOf": [{"enum": ["x", "y", "z", "xy", "yz", "zx", "view"]}, {"$ref": "#/$defs/v2/dynamic"}]})json", "",
	 ""},
    {"end", R"json({"anyOf": [{"enum": ["confirm", "cancel"]}, {"$ref": "#/$defs/v2/dynamic"}]})json", "", ""},
    {"append", "", "", ""},
    {"keep", R"json({"anyOf": [{"type": "integer", "minimum": 1, "maximum": 10000}, {"type": "string"}]})json", "", ""},
    {"id", "", "", ""},
    {"error", "", "", ""},
    {"data", "", "", ""},
    {"message", "", "", ""},
    {"title", "", "", ""},
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
});

/// The shared values: "$defs/v2" entries the other schemas refer to.
constexpr auto definitionTable = std::to_array<KeyReference>({
    {"permissionRule", R"json({"type": "string", "pattern": "^[^() ]+(\\((#|/)[^ ]*\\))?$",
		"description": "\"author\" or \"author(pattern)\", e.g. \"agent(#board)\""})json",
	 "", ""},
    {"characterPairs", R"json({"type": "array", "maxItems": 32, "items": {"type": "array", "minItems": 2, "maxItems": 2,
		"items": {"type": "string", "minLength": 1, "maxLength": 1}}})json",
	 "", ""},
    {"hexColor", R"json({"type": "string", "pattern": "^#([0-9a-fA-F]{3}|[0-9a-fA-F]{6}|[0-9a-fA-F]{8})$"})json", "",
	 ""},
    {"stateType",
	 R"json({"description": "A type, written as JSON Schema (the subset the runtime reads), or true (anything) or false (nothing)",
		"type": ["object", "boolean"], "additionalProperties": false,
		"properties": {"type": {"oneOf": [{"$ref": "#/$defs/v2/stateTypeName"}, {"type": "array",
		"minItems": 1, "items": {"$ref": "#/$defs/v2/stateTypeName"}}]}, "enum": {"type": "array",
		"minItems": 1}, "const": {}, "properties": {"type": "object",
		"additionalProperties": {"$ref": "#/$defs/v2/stateType"}}, "required": {"type": "array",
		"items": {"type": "string"}}, "additionalProperties": {"$ref": "#/$defs/v2/stateType"},
		"items": {"$ref": "#/$defs/v2/stateType"}, "minimum": {"type": "number"},
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
		{"$ref": "#/$defs/v2/dynamic"}]})json",
	 "", ""},
    {"length", R"json({"description": "Pixels (120 or \"120px\") or a percentage of the parent (\"50%\")",
		"anyOf": [{"type": "number"}, {"type": "string", "pattern": "^-?[0-9]+(\\.[0-9]+)?(px|%)?$"},
		{"$ref": "#/$defs/v2/dynamic"}]})json",
	 "", ""},
    {"edge", R"json({"description": "A length from the parent's edge, or another node's edge: \"#header.bottom + 8\"",
		"anyOf": [{"$ref": "#/$defs/v2/length"}, {"type": "string",
		"pattern": "^#[A-Za-z0-9_-]+\\.(top|bottom|left|right)( *[-+] *[0-9]+(\\.[0-9]+)?)?$"}]})json",
	 "", ""},
    {"align", R"json({"anyOf": [{"enum": ["start", "center", "end"]}, {"$ref": "#/$defs/v2/dynamic"}]})json", "", ""},
    {"crossAlign",
	 R"json({"anyOf": [{"enum": ["start", "center", "end", "stretch"]}, {"$ref": "#/$defs/v2/dynamic"}]})json", "", ""},
    {"track",
	 R"json({"description": "A grid column or row: pixels (120), a percentage of the grid (\"25%\"), a fraction of the room left (\"1fr\"), \"auto\" (its largest child) or \"minmax(minimum, maximum)\"",
		"anyOf": [{"type": "number", "minimum": 0}, {"type": "string",
		"pattern": "^(auto|[0-9]+(\\.[0-9]+)?(px|%|fr)?|minmax\\( *(auto|[0-9]+(\\.[0-9]+)?(px|%|fr)?) *, *(auto|[0-9]+(\\.[0-9]+)?(px|%|fr)?) *\\))$"},
		{"$ref": "#/$defs/v2/dynamic"}]})json",
	 "", ""},
    {"stateBlock", R"json({"type": "object",
		"description": "Properties shown in that state (any visual property, or a style)"})json",
	 "", ""},
    {"dynamic",
	 R"json({"description": "A {{binding or expression}}, an operator ({\"$clamp\": [...]}), or a \"@token\"",
		"anyOf": [{"type": "string", "pattern": "\\{\\{.*\\}\\}"}, {"$ref": "#/$defs/v2/expression"},
		{"$ref": "#/$defs/v2/tokenName"}]})json",
	 "", ""},
    {"tokenName", R"json({"description": "\"@name\" of a token of \"tokens\"", "type": "string",
		"pattern": "^@[A-Za-z_][A-Za-z0-9_.-]*$"})json",
	 "", ""},
    {"tokenValue", R"json({"anyOf": [{"type": "number"}, {"$ref": "#/$defs/v2/hexColor"}, {"const": "transparent"},
		{"$ref": "#/$defs/v2/tokenName"}]})json",
	 "", ""},
    {"pixels", R"json({"description": "A number of pixels, 0 or more", "anyOf": [{"type": "number", "minimum": 0},
		{"$ref": "#/$defs/v2/dynamic"}]})json",
	 "", ""},
    {"flag", R"json({"anyOf": [{"type": "boolean"}, {"$ref": "#/$defs/v2/dynamic"}]})json", "", ""},
    {"handler", R"json({"description": "An action name, a step, or steps", "anyOf": [{"type": "string"},
		{"$ref": "#/$defs/v2/steps"}]})json",
	 "", ""},
    {"command", R"json({"type": "object", "additionalProperties": false,
		"description": "An action that describes itself; \"enablement\" is checked for keys, menus and palettes",
		"properties": {"title": {"type": "string"}, "category": {"type": "string"}, "icon": {"type": "string"},
		"enablement": {}, "description": {"type": "string"}, "do": {"$ref": "#/$defs/v2/handler"}}})json",
	 "", ""},
    {"binding", R"json({"type": "object", "required": ["do"], "additionalProperties": false,
		"description": "A key binding: runs \"do\" (an action name or steps) when \"when\" is true",
		"properties": {"when": {}, "do": {"$ref": "#/$defs/v2/handler"}, "with": {"type": "object"}}})json",
	 "", ""},
    {"keyHandler", R"json({"description": "A handler, a binding, or bindings (the first that applies runs)",
		"anyOf": [{"$ref": "#/$defs/v2/handler"}, {"$ref": "#/$defs/v2/binding"},
		{"type": "array", "minItems": 1, "items": {"$ref": "#/$defs/v2/binding"}}]})json",
	 "", ""},
    {"steps", R"json({"anyOf": [{"$ref": "#/$defs/v2/step"}, {"type": "array",
		"items": {"$ref": "#/$defs/v2/step"}}]})json",
	 "", ""},
});

} // namespace

std::span<const KeyReference> sectionKeys() { return sectionTable; }

std::span<const KeyReference> stepKeys() { return stepTable; }

std::span<const KeyReference> definitions() { return definitionTable; }

} // namespace affresco::format
