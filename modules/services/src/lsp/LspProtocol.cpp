/// @file
/// @brief The Language Server Protocol as the `lsp` service reads it (LspProtocol.hpp): results normalised and bounded.

#include "lsp/LspProtocol.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

namespace affresco::services::lsp {

namespace {

namespace fs = std::filesystem;

constexpr std::size_t largestPosition = std::size_t{1} << 30; ///< Lines and columns past this are taken as this.

/// @brief A number of a JSON object, or 0.
/// @param object The object.
/// @param key The key.
/// @return Its value if it is a non-negative integer, at most largestPosition.
std::size_t number(const json& object, const char* key) {
	if(!object.is_object()) return 0;
	const auto found = object.find(key);
	if(found == object.end() || !found->is_number_integer() || found->get<long long>() < 0) return 0;
	return std::min(found->get<std::size_t>(), largestPosition);
}

/// @brief A text of a JSON object, cut.
/// @param object The object.
/// @param key The key.
/// @param bytes At most this many bytes.
/// @return It, or "" if it isn't text.
std::string text(const json& object, const char* key, std::size_t bytes) {
	if(!object.is_object()) return {};
	const auto found = object.find(key);
	return found != object.end() && found->is_string() ? cut(found->get_ref<const std::string&>(), bytes) : std::string();
}

/// @brief A member of a JSON object, or null.
/// @param object The object.
/// @param key The key.
/// @return It, or a null value.
const json& member(const json& object, const char* key) {
	static const json none;
	if(!object.is_object()) return none;
	const auto found = object.find(key);
	return found == object.end() ? none : *found;
}

/// @brief A range's ends, as a screen counts them (lines from 0, columns in code points).
struct Span {
	std::size_t line = 0;      ///< The first line.
	std::size_t column = 0;    ///< Its column.
	std::size_t endLine = 0;   ///< The last line.
	std::size_t endColumn = 0; ///< Its column.
};

/// @brief Reads an LSP range.
/// @param range `{ "start": { "line", "character" }, "end": ... }`.
/// @param path The file, for the mapper.
/// @param columns The mapper, or empty.
/// @return The span (the end never before the start).
Span spanOf(const json& range, const std::string& path, const ColumnMapper& columns) {
	const json& start = member(range, "start");
	const json& end = member(range, "end");
	Span span{number(start, "line"), number(start, "character"), number(end, "line"), number(end, "character")};
	if(columns) {
		span.column = columns(path, span.line, span.column);
		span.endColumn = columns(path, span.endLine, span.endColumn);
	}
	if(std::tie(span.endLine, span.endColumn) < std::tie(span.line, span.column)) {
		span.endLine = span.line;
		span.endColumn = span.column;
	}
	return span;
}

/// @brief Adds a span to an object.
/// @param span The span.
/// @param into The object to add `line`, `column`, `endLine`, `endColumn` to.
void putSpan(const Span& span, json& into) {
	into["line"] = span.line;
	into["column"] = span.column;
	into["endLine"] = span.endLine;
	into["endColumn"] = span.endColumn;
}

/// @brief The names of LSP's symbol kinds (SymbolKind, from 1).
constexpr std::array<std::string_view, 26> symbolKinds = {
    "file",     "module",   "namespace", "package",  "class",     "method",   "property",      "field",    "constructor",
    "enum",     "interface", "function", "variable", "constant",  "string",   "number",        "boolean",  "array",
    "object",   "key",      "null",      "enumMember", "struct",  "event",    "operator",      "typeParameter"};

/// @brief The names of LSP's completion item kinds (CompletionItemKind, from 1).
constexpr std::array<std::string_view, 25> completionKinds = {
    "text",     "method",   "function",  "constructor", "field",      "variable", "class",  "interface", "module",
    "property", "unit",     "value",     "enum",        "keyword",    "snippet",  "color",  "file",      "reference",
    "folder",   "enumMember", "constant", "struct",     "event",      "operator", "typeParameter"};

/// @brief A kind's name from its LSP number.
/// @param names The names, from 1.
/// @param kind The kind.
/// @return Its name, or "" for an unknown one.
template <std::size_t N> std::string kindName(const std::array<std::string_view, N>& names, const json& kind) {
	if(!kind.is_number_integer()) return {};
	const long long value = kind.get<long long>();
	return value >= 1 && std::cmp_less_equal(value, N) ? std::string(names[static_cast<std::size_t>(value - 1)])
	                                                   : std::string();
}

/// @brief Markdown or plain text from a hover's or an item's documentation.
/// @param value A string, `{ "kind", "value" }`, `{ "language", "value" }`, or a list of them.
/// @param bytes At most this many bytes.
/// @return The text.
std::string documentationOf(const json& value, std::size_t bytes) {
	std::string out;
	const auto add = [&out](const json& one) {
		std::string piece;
		if(one.is_string()) piece = one.get<std::string>();
		else if(one.is_object() && member(one, "value").is_string()) {
			const std::string language = member(one, "language").is_string() ? one["language"].get<std::string>() : "";
			piece = one.contains("language") ? "```" + language + "\n" + one["value"].get<std::string>() + "\n```"
			                                 : one["value"].get<std::string>();
		}
		if(piece.empty()) return;
		if(!out.empty()) out += "\n\n";
		out += piece;
	};
	if(value.is_array())
		for(const json& one : value) {
			add(one);
			if(out.size() > bytes) break;
		}
	else add(value);
	return cut(out, bytes);
}


/// @brief Reads the edits of one file into the flat list.
/// @param path The file.
/// @param edits Its TextEdits.
/// @param limits The limits.
/// @param columns The mapper.
/// @param out The flat list.
/// @throws std::runtime_error If they aren't edits, or are too many or too large.
void addEdits(const std::string& path, const json& edits, const Limits& limits, const ColumnMapper& columns,
              json& out) {
	if(!edits.is_array()) throw std::runtime_error("an edit list isn't a list");
	for(const json& edit : edits) {
		if(out.size() >= limits.edits) throw std::runtime_error("more than " + std::to_string(limits.edits) + " edits");
		const json& newText = member(edit, "newText");
		if(!newText.is_string()) throw std::runtime_error("an edit has no text");
		if(newText.get_ref<const std::string&>().size() > limits.editBytes)
			throw std::runtime_error("an edit's text is too large");
		json flat = {{"path", path}};
		putSpan(spanOf(member(edit, "range"), path, columns), flat);
		flat["text"] = newText;
		out.push_back(std::move(flat));
	}
}

/// @brief The edits of a list, and how many files they change.
/// @param edits The flat list.
/// @return `{ "edits", "files", "truncated" }`.
json editList(json edits) {
	std::vector<std::string> files;
	for(const json& one : edits) files.push_back(one["path"].get<std::string>());
	std::ranges::sort(files);
	const auto [first, last] = std::ranges::unique(files);
	files.erase(first, last);
	return {{"edits", std::move(edits)}, {"files", files.size()}, {"truncated", false}};
}

/// @brief A WorkspaceEdit as one list of edits.
/// @param edit The WorkspaceEdit.
/// @param root The folder.
/// @param limits The limits.
/// @param columns The mapper.
/// @return `{ "edits", "files", "truncated" }`.
/// @throws std::runtime_error If it is refused whole.
json workspaceEdit(const json& edit, const fs::path& root, const Limits& limits, const ColumnMapper& columns) {
	json edits = json::array();
	const auto fileOf = [&root](const json& uri) {
		return uri.is_string() ? pathOfUri(root, uri.get_ref<const std::string&>()) : std::nullopt;
	};
	const json& documentChanges = member(edit, "documentChanges");
	if(documentChanges.is_array()) {
		for(const json& change : documentChanges) {
			if(change.contains("kind")) throw std::runtime_error("the edit creates, renames or deletes files");
			const std::optional<std::string> path = fileOf(member(member(change, "textDocument"), "uri"));
			if(!path) throw std::runtime_error("the edit reaches outside the folder");
			addEdits(*path, member(change, "edits"), limits, columns, edits);
		}
	} else if(const json& changes = member(edit, "changes"); changes.is_object()) {
		for(const auto& [uri, list] : changes.items()) {
			const std::optional<std::string> path = pathOfUri(root, uri);
			if(!path) throw std::runtime_error("the edit reaches outside the folder");
			addEdits(*path, list, limits, columns, edits);
		}
	}
	return editList(std::move(edits));
}

/// @brief Locations, LocationLinks, or one Location, as a list.
/// @param result The result.
/// @param root The folder.
/// @param limits The limits.
/// @param columns The mapper.
/// @return `{ "locations", "truncated" }`.
json locations(const json& result, const fs::path& root, const Limits& limits, const ColumnMapper& columns) {
	json rows = json::array();
	bool truncated = false;
	const auto add = [&](const json& location) {
		const bool link = location.contains("targetUri");
		const json& uri = member(location, link ? "targetUri" : "uri");
		const std::optional<std::string> path =
		    uri.is_string() ? pathOfUri(root, uri.get_ref<const std::string&>()) : std::nullopt;
		if(!path) return;
		if(rows.size() >= limits.locations) {
			truncated = true;
			return;
		}
		const json& range = link ? (location.contains("targetSelectionRange") ? location["targetSelectionRange"]
		                                                                      : member(location, "targetRange"))
		                         : member(location, "range");
		json row = {{"path", *path}};
		putSpan(spanOf(range, *path, columns), row);
		rows.push_back(std::move(row));
	};
	if(result.is_array())
		for(const json& one : result) add(one);
	else if(result.is_object()) add(result);
	return {{"locations", std::move(rows)}, {"truncated", truncated}};
}

/// @brief A symbol as a list's row.
/// @param symbol The symbol.
/// @param detail The key of its detail ("detail", or "containerName").
/// @param path Its file.
/// @param span Where it is.
/// @param depth How deep it is.
/// @param limits The limits.
/// @return `{ "name", "detail", "kind", "path", "line", "column", "depth" }`.
json symbolRow(const json& symbol, const char* detail, const std::string& path, const Span& span, int depth,
               const Limits& limits) {
	return {{"name", text(symbol, "name", limits.text)},
	        {"detail", text(symbol, detail, limits.text)},
	        {"kind", kindName(symbolKinds, member(symbol, "kind"))},
	        {"path", path},
	        {"line", span.line},
	        {"column", span.column},
	        {"depth", depth}};
}

/// @brief Adds DocumentSymbols, and their children, to the rows.
/// @param symbols The symbols.
/// @param path The file.
/// @param depth How deep these are.
/// @param limits The limits (depth, locations).
/// @param columns The mapper.
/// @param rows The rows.
/// @return false once the rows are full.
bool documentSymbols(const json& symbols, const std::string& path, int depth, const Limits& limits,
                     const ColumnMapper& columns, json& rows) {
	if(!symbols.is_array() || depth >= limits.depth) return true;
	for(const json& symbol : symbols) {
		if(rows.size() >= limits.locations) return false;
		const json& range = symbol.contains("selectionRange") ? symbol["selectionRange"] : member(symbol, "range");
		rows.push_back(symbolRow(symbol, "detail", path, spanOf(range, path, columns), depth, limits));
		if(!documentSymbols(member(symbol, "children"), path, depth + 1, limits, columns, rows)) return false;
	}
	return true;
}

/// @brief Document or workspace symbols as a list.
/// @param result DocumentSymbol[], SymbolInformation[] or WorkspaceSymbol[].
/// @param root The folder.
/// @param path The file a document's symbols are of.
/// @param limits The limits.
/// @param columns The mapper.
/// @return `{ "symbols", "truncated" }`.
json symbols(const json& result, const fs::path& root, const std::string& path, const Limits& limits,
             const ColumnMapper& columns) {
	json rows = json::array();
	if(!result.is_array()) return {{"symbols", std::move(rows)}, {"truncated", false}};
	if(!result.empty() && !result.front().contains("location")) {
		const bool complete = documentSymbols(result, path, 0, limits, columns, rows);
		return {{"symbols", std::move(rows)}, {"truncated", !complete}};
	}
	bool truncated = false;
	for(const json& symbol : result) {
		const json& location = member(symbol, "location");
		const json& uri = member(location, "uri");
		const std::optional<std::string> file =
		    uri.is_string() ? pathOfUri(root, uri.get_ref<const std::string&>()) : std::nullopt;
		if(!file) continue;
		if(rows.size() >= limits.locations) {
			truncated = true;
			break;
		}
		rows.push_back(
		    symbolRow(symbol, "containerName", *file, spanOf(member(location, "range"), *file, columns), 0, limits));
	}
	return {{"symbols", std::move(rows)}, {"truncated", truncated}};
}

/// @brief Completion items as a list.
/// @param result CompletionItem[] or a CompletionList.
/// @param limits The limits.
/// @return `{ "items", "incomplete", "truncated" }`.
json completion(const json& result, const Limits& limits) {
	const json& list = result.is_array() ? result : member(result, "items");
	json items = json::array();
	bool truncated = false;
	if(list.is_array())
		for(const json& item : list) {
			if(items.size() >= limits.completions) {
				truncated = true;
				break;
			}
			const std::string label = text(item, "label", limits.text);
			if(label.empty()) continue;
			const json& edit = member(item, "textEdit");
			std::string insert = member(edit, "newText").is_string()       ? edit["newText"].get<std::string>()
			                     : member(item, "insertText").is_string() ? item["insertText"].get<std::string>()
			                                                              : label;
			if(member(item, "insertTextFormat") == 2) insert = plainSnippet(insert);
			const std::string filter = text(item, "filterText", limits.text);
			items.push_back({{"label", label},
			                 {"detail", text(item, "detail", limits.text)},
			                 {"insert", cut(insert, limits.text)},
			                 {"filter", filter.empty() ? label : filter},
			                 {"kind", kindName(completionKinds, member(item, "kind"))}});
		}
	return {{"items", std::move(items)}, {"incomplete", member(result, "isIncomplete") == true}, {"truncated", truncated}};
}

/// @brief The active signature.
/// @param result A SignatureHelp, or null.
/// @param limits The limits.
/// @return `{ "label", "documentation", "parameter", "active", "count" }`, or null.
json signature(const json& result, const Limits& limits) {
	const json& signatures = member(result, "signatures");
	if(!signatures.is_array() || signatures.empty()) return nullptr;
	const std::size_t active = std::min(number(result, "activeSignature"), signatures.size() - 1);
	const json& chosen = signatures[active];
	const json& parameters = member(chosen, "parameters");
	const std::size_t which = chosen.contains("activeParameter") ? number(chosen, "activeParameter")
	                                                             : number(result, "activeParameter");
	std::string parameter;
	const std::string label = text(chosen, "label", limits.text);
	if(parameters.is_array() && which < parameters.size()) {
		const json& name = member(parameters[which], "label");
		if(name.is_string()) parameter = cut(name.get<std::string>(), limits.text);
		else if(name.is_array() && name.size() == 2 && name[0].is_number_unsigned() && name[1].is_number_unsigned()) {
			// Offsets in UTF-16 units, taken to bytes through code points, and kept on character boundaries
			const auto byteOf = [&label](std::size_t units) {
				std::size_t points = codePointsOf(label, units), at = 0;
				for(; at < label.size() && points > 0; --points)
					do ++at;
					while(at < label.size() && (static_cast<unsigned char>(label[at]) & 0xC0) == 0x80);
				return at;
			};
			const std::size_t from = byteOf(name[0].get<std::size_t>());
			const std::size_t to = std::max(byteOf(name[1].get<std::size_t>()), from);
			parameter = label.substr(from, to - from);
		}
	}
	return {{"label", label},
	        {"documentation", documentationOf(member(chosen, "documentation"), limits.documentation)},
	        {"parameter", parameter},
	        {"active", active},
	        {"count", signatures.size()}};
}

/// @brief One number of a semantic token.
/// @param value The number.
/// @return It, at most largestPosition, or 0 if it isn't a whole number from 0.
std::size_t tokenField(const json& value) {
	return value.is_number_integer() && value.get<long long>() >= 0 // whichever way it was read
	           ? std::min(value.get<std::size_t>(), largestPosition)
	           : 0;
}

/// @brief Decodes semantic tokens with their legend.
/// @param result `{ "data": [5 numbers per token], "legend": { "tokenTypes" } }`.
/// @param path The file.
/// @param limits The limits.
/// @param columns The mapper.
/// @return `{ "tokens": [{ "line", "column", "length", "style" }], "truncated" }`.
json tokens(const json& result, const std::string& path, const Limits& limits, const ColumnMapper& columns) {
	const json& data = member(result, "data");
	const json& types = member(member(result, "legend"), "tokenTypes");
	json runs = json::array();
	if(!data.is_array() || !types.is_array()) return {{"tokens", std::move(runs)}, {"truncated", false}};
	std::vector<std::string> styles;
	for(const json& type : types) // a type the server names, cut as its other texts are
		styles.push_back(type.is_string() ? cut(styleOfToken(type.get<std::string>()), limits.text) : "");
	std::size_t line = 0, start = 0, i = 0;
	for(; i + 5 <= data.size() && runs.size() < limits.tokens; i += 5) {
		const auto [deltaLine, deltaStart, length, type] = std::array{
		    tokenField(data[i]), tokenField(data[i + 1]), tokenField(data[i + 2]), tokenField(data[i + 3])};
		line = std::min(line + deltaLine, largestPosition);
		start = deltaLine > 0 ? deltaStart : std::min(start + deltaStart, largestPosition);
		if(type >= styles.size() || styles[type].empty() || length == 0) continue;
		std::size_t column = start, end = start + length;
		if(columns) {
			column = columns(path, line, start);
			end = columns(path, line, start + length);
		}
		if(end > column)
			runs.push_back({{"line", line}, {"column", column}, {"length", end - column}, {"style", styles[type]}});
	}
	return {{"tokens", std::move(runs)}, {"truncated", i + 5 <= data.size()}};
}

/// @brief A diagnostic's severity.
/// @param diagnostic The diagnostic: its severity is 1 for an error, 2 (or none) for a warning, 3 for information
/// and 4 for a hint.
/// @return Its name.
const char* severityOf(const json& diagnostic) {
	switch(number(diagnostic, "severity")) {
	case 1: return "error";
	case 3: return "info";
	case 4: return "hint";
	default: return "warning";
	}
}

/// @brief A file's diagnostics as a list.
/// @param params publishDiagnostics' params.
/// @param root The folder.
/// @param limits The limits.
/// @param columns The mapper.
/// @return `{ "path", "diagnostics", "truncated" }`.
/// @throws std::runtime_error For a file outside the folder.
json diagnostics(const json& params, const fs::path& root, const Limits& limits, const ColumnMapper& columns) {
	const json& uri = member(params, "uri");
	const std::optional<std::string> path =
	    uri.is_string() ? pathOfUri(root, uri.get_ref<const std::string&>()) : std::nullopt;
	if(!path) throw std::runtime_error("the diagnostics are for a file outside the folder");
	json items = json::array();
	const json& list = member(params, "diagnostics");
	const bool truncated = list.is_array() && list.size() > limits.diagnostics;
	if(list.is_array())
		for(const json& diagnostic : list) {
			if(items.size() >= limits.diagnostics) break;
			const json& code = member(diagnostic, "code");
			json item = {{"severity", severityOf(diagnostic)},
			             {"message", text(diagnostic, "message", limits.text)},
			             {"source", text(diagnostic, "source", 64)},
			             {"code", code.is_number_integer() ? std::to_string(code.get<long long>()) : text(diagnostic, "code", 64)}};
			putSpan(spanOf(member(diagnostic, "range"), *path, columns), item);
			items.push_back(std::move(item));
		}
	return {{"path", *path}, {"diagnostics", std::move(items)}, {"truncated", truncated}};
}

/// @brief A formatting's edits, all of one file.
/// @param result textDocument/formatting's result: TextEdit[] or null.
/// @param path The file.
/// @param limits The limits.
/// @param columns The mapper.
/// @return `{ "edits", "files", "truncated" }`.
/// @throws std::runtime_error If they are refused.
json formatting(const json& result, const std::string& path, const Limits& limits, const ColumnMapper& columns) {
	json edits = json::array();
	if(!result.is_null()) addEdits(path, result, limits, columns, edits);
	return editList(std::move(edits));
}

} // namespace

json normalize(std::string_view kind, const json& result, const fs::path& root, const std::string& path,
               const Limits& limits, const ColumnMapper& columns) {
	if(kind == "hover") return {{"text", documentationOf(member(result, "contents"), limits.documentation)}};
	if(kind == "completion") return completion(result, limits);
	if(kind == "signature") return signature(result, limits);
	if(kind == "definition" || kind == "references") return locations(result, root, limits, columns);
	if(kind == "rename") return workspaceEdit(result, root, limits, columns);
	if(kind == "format") return formatting(result, path, limits, columns);
	if(kind == "symbols" || kind == "workspaceSymbols") return symbols(result, root, path, limits, columns);
	if(kind == "tokens") return tokens(result, path, limits, columns);
	if(kind == "diagnostics") return diagnostics(result, root, limits, columns);
	throw std::invalid_argument("no result kind '" + std::string(kind) + "'");
}

} // namespace affresco::services::lsp
