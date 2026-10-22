/// @file
/// @brief The language server protocol as Forge's lsp feature reads it (LspProtocol.hpp).

#include "features/lsp/LspProtocol.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

namespace forge::lsp {

namespace {

using affresco::json;
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

/// @brief A range's ends, as the editor counts (lines from 0, columns in code points).
struct Span {
	std::size_t line = 0;      ///< The first line.
	std::size_t column = 0;    ///< Its column.
	std::size_t endLine = 0;   ///< The last line.
	std::size_t endColumn = 0; ///< Its column.
};

/// @brief Reads an LSP range.
/// @param range `{ "start": { "line", "character" }, "end": ... }`.
/// @param id The file, for the mapper.
/// @param columns The mapper, or empty.
/// @return The span (the end never before the start).
Span spanOf(const json& range, const std::string& id, const ColumnMapper& columns) {
	const json& start = member(range, "start");
	const json& end = member(range, "end");
	Span span{number(start, "line"), number(start, "character"), number(end, "line"), number(end, "character")};
	if(columns) {
		span.column = columns(id, span.line, span.column);
		span.endColumn = columns(id, span.endLine, span.endColumn);
	}
	if(std::tie(span.endLine, span.endColumn) < std::tie(span.line, span.column)) {
		span.endLine = span.line;
		span.endColumn = span.column;
	}
	return span;
}

/// @brief A span as the screen's lists take it, from 1.
/// @param span The span.
/// @param into The object to add `line`, `column`, `endLine`, `endColumn` to.
void putFromOne(const Span& span, json& into) {
	into["line"] = span.line + 1;
	into["column"] = span.column + 1;
	into["endLine"] = span.endLine + 1;
	into["endColumn"] = span.endColumn + 1;
}

/// @brief The name of a file, for a list's label.
/// @param id The file's id.
/// @return What follows its last `/`.
std::string fileName(const std::string& id) {
	const std::size_t slash = id.rfind('/');
	return slash == std::string::npos ? id : id.substr(slash + 1);
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
	return value >= 1 && value <= static_cast<long long>(N) ? std::string(names[static_cast<std::size_t>(value - 1)])
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
/// @param id The file.
/// @param edits Its TextEdits.
/// @param limits The limits.
/// @param columns The mapper.
/// @param out The flat list.
/// @return An error, or std::nullopt.
std::optional<std::string> addEdits(const std::string& id, const json& edits, const Limits& limits,
                                    const ColumnMapper& columns, json& out) {
	if(!edits.is_array()) return "an edit list isn't a list";
	for(const json& edit : edits) {
		if(out.size() >= limits.edits) return "more than " + std::to_string(limits.edits) + " edits";
		const json& newText = member(edit, "newText");
		if(!newText.is_string()) return "an edit has no text";
		if(newText.get_ref<const std::string&>().size() > limits.editBytes) return "an edit's text is too large";
		json flat = {{"id", id}, {"text", newText}};
		putFromOne(spanOf(member(edit, "range"), id, columns), flat);
		out.push_back(std::move(flat));
	}
	return std::nullopt;
}

/// @brief A WorkspaceEdit as one list of edits.
/// @param edit The WorkspaceEdit.
/// @param root The open folder.
/// @param limits The limits.
/// @param columns The mapper.
/// @return `{ "edits", "files" }`, or `{ "error" }`.
json workspaceEdit(const json& edit, const fs::path& root, const Limits& limits, const ColumnMapper& columns) {
	json edits = json::array();
	const auto fileOf = [&root](const json& uri) {
		return uri.is_string() ? idOfUri(root, uri.get_ref<const std::string&>()) : std::nullopt;
	};
	const json& documentChanges = member(edit, "documentChanges");
	if(documentChanges.is_array()) {
		for(const json& change : documentChanges) {
			if(change.contains("kind")) return {{"error", "the edit creates, renames or deletes files"}};
			const std::optional<std::string> id = fileOf(member(member(change, "textDocument"), "uri"));
			if(!id) return {{"error", "the edit reaches outside the open folder"}};
			if(auto problem = addEdits(*id, member(change, "edits"), limits, columns, edits)) return {{"error", *problem}};
		}
	} else if(const json& changes = member(edit, "changes"); changes.is_object()) {
		for(const auto& [uri, list] : changes.items()) {
			const std::optional<std::string> id = idOfUri(root, uri);
			if(!id) return {{"error", "the edit reaches outside the open folder"}};
			if(auto problem = addEdits(*id, list, limits, columns, edits)) return {{"error", *problem}};
		}
	}
	std::vector<std::string> files;
	for(const json& one : edits) files.push_back(one["id"].get<std::string>());
	std::ranges::sort(files);
	const auto [first, last] = std::ranges::unique(files);
	files.erase(first, last);
	return {{"edits", std::move(edits)}, {"files", files.size()}, {"truncated", false}};
}

/// @brief Locations, LocationLinks, or one Location, as the list's rows.
/// @param result The result.
/// @param root The open folder.
/// @param limits The limits.
/// @param columns The mapper.
/// @return `{ "locations", "truncated" }`.
json locations(const json& result, const fs::path& root, const Limits& limits, const ColumnMapper& columns) {
	json rows = json::array();
	bool truncated = false;
	const auto add = [&](const json& location) {
		const bool link = location.contains("targetUri");
		const json& uri = member(location, link ? "targetUri" : "uri");
		const std::optional<std::string> id = uri.is_string() ? idOfUri(root, uri.get_ref<const std::string&>()) : std::nullopt;
		if(!id) return;
		if(rows.size() >= limits.locations) {
			truncated = true;
			return;
		}
		const json& range = link ? (location.contains("targetSelectionRange") ? location["targetSelectionRange"]
		                                                                      : member(location, "targetRange"))
		                         : member(location, "range");
		json row = {{"id", *id}};
		const Span span = spanOf(range, *id, columns);
		putFromOne(span, row);
		row["label"] = fileName(*id) + ":" + std::to_string(span.line + 1);
		row["detail"] = *id;
		rows.push_back(std::move(row));
	};
	if(result.is_array())
		for(const json& one : result) add(one);
	else if(result.is_object()) add(result);
	return {{"locations", std::move(rows)}, {"truncated", truncated}};
}

/// @brief Adds DocumentSymbols, and their children, to the rows.
/// @param symbols The symbols.
/// @param id The file.
/// @param depth How deep these are.
/// @param limits The limits (depth, locations).
/// @param columns The mapper.
/// @param rows The rows.
/// @return false once the rows are full.
bool documentSymbols(const json& symbols, const std::string& id, int depth, const Limits& limits,
                     const ColumnMapper& columns, json& rows) {
	if(!symbols.is_array() || depth >= limits.depth) return true;
	for(const json& symbol : symbols) {
		if(rows.size() >= limits.locations) return false;
		const json& range = symbol.contains("selectionRange") ? symbol["selectionRange"] : member(symbol, "range");
		const Span span = spanOf(range, id, columns);
		rows.push_back({{"name", text(symbol, "name", limits.text)},
		                {"detail", text(symbol, "detail", limits.text)},
		                {"kind", kindName(symbolKinds, member(symbol, "kind"))},
		                {"id", id},
		                {"line", span.line + 1},
		                {"column", span.column + 1},
		                {"depth", depth}});
		if(!documentSymbols(member(symbol, "children"), id, depth + 1, limits, columns, rows)) return false;
	}
	return true;
}

/// @brief Document or workspace symbols as the list's rows.
/// @param result DocumentSymbol[], SymbolInformation[] or WorkspaceSymbol[].
/// @param root The open folder.
/// @param id The file a document's symbols are of.
/// @param limits The limits.
/// @param columns The mapper.
/// @return `{ "symbols", "truncated" }`.
json symbols(const json& result, const fs::path& root, const std::string& id, const Limits& limits,
             const ColumnMapper& columns) {
	json rows = json::array();
	if(!result.is_array()) return {{"symbols", std::move(rows)}, {"truncated", false}};
	if(!result.empty() && !result.front().contains("location")) {
		const bool complete = documentSymbols(result, id, 0, limits, columns, rows);
		return {{"symbols", std::move(rows)}, {"truncated", !complete}};
	}
	bool truncated = false;
	for(const json& symbol : result) {
		const json& location = member(symbol, "location");
		const json& uri = member(location, "uri");
		const std::optional<std::string> file = uri.is_string() ? idOfUri(root, uri.get_ref<const std::string&>()) : std::nullopt;
		if(!file) continue;
		if(rows.size() >= limits.locations) {
			truncated = true;
			break;
		}
		const Span span = spanOf(member(location, "range"), *file, columns);
		rows.push_back({{"name", text(symbol, "name", limits.text)},
		                {"detail", text(symbol, "containerName", limits.text)},
		                {"kind", kindName(symbolKinds, member(symbol, "kind"))},
		                {"id", *file},
		                {"line", span.line + 1},
		                {"column", span.column + 1},
		                {"depth", 0}});
	}
	return {{"symbols", std::move(rows)}, {"truncated", truncated}};
}

/// @brief Completion items as the list's rows.
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
			const auto from = std::min(name[0].get<std::size_t>(), label.size());
			const auto to = std::clamp(name[1].get<std::size_t>(), from, label.size());
			parameter = label.substr(from, to - from); // offsets in UTF-16 units: exact for ASCII labels
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
/// @param id The file.
/// @param limits The limits.
/// @param columns The mapper.
/// @return `[{ "line", "column", "length", "style" }]`, from 0.
json tokens(const json& result, const std::string& id, const Limits& limits, const ColumnMapper& columns) {
	const json& data = member(result, "data");
	const json& types = member(member(result, "legend"), "tokenTypes");
	json runs = json::array();
	if(!data.is_array() || !types.is_array()) return runs;
	std::vector<std::string> styles;
	for(const json& type : types) styles.push_back(type.is_string() ? styleOfToken(type.get<std::string>()) : "");
	std::size_t line = 0, start = 0;
	for(std::size_t i = 0; i + 5 <= data.size() && runs.size() < limits.tokens; i += 5) {
		const auto [deltaLine, deltaStart, length, type] = std::array{
		    tokenField(data[i]), tokenField(data[i + 1]), tokenField(data[i + 2]), tokenField(data[i + 3])};
		line = std::min(line + deltaLine, largestPosition);
		start = deltaLine > 0 ? deltaStart : std::min(start + deltaStart, largestPosition);
		if(type >= styles.size() || styles[type].empty() || length == 0) continue;
		std::size_t column = start, end = start + length;
		if(columns) {
			column = columns(id, line, start);
			end = columns(id, line, start + length);
		}
		if(end > column)
			runs.push_back({{"line", line}, {"column", column}, {"length", end - column}, {"style", styles[type]}});
	}
	return runs;
}

/// @brief A diagnostic's severity and its squiggle's color.
/// @param diagnostic The diagnostic: its severity is 1 for an error, 2 (or none) for a warning, else information.
/// @return Its Problems severity and its color.
std::pair<const char*, const char*> severityOf(const json& diagnostic) {
	const std::size_t level = number(diagnostic, "severity");
	if(level == 1) return {"error", "#f85149"};
	if(level == 2 || level == 0) return {"warning", "#cca700"};
	return {"info", "#3794ff"};
}

/// @brief A file's diagnostics as Problems rows and squiggles.
/// @param params publishDiagnostics' params.
/// @param root The open folder.
/// @param limits The limits.
/// @param columns The mapper.
/// @return `{ "id", "items", "decorations", "truncated" }`, or `{ "error" }`.
json diagnostics(const json& params, const fs::path& root, const Limits& limits, const ColumnMapper& columns) {
	const json& uri = member(params, "uri");
	const std::optional<std::string> id = uri.is_string() ? idOfUri(root, uri.get_ref<const std::string&>()) : std::nullopt;
	if(!id) return {{"error", "the diagnostics are for a file outside the open folder"}};
	json items = json::array(), decorations = json::array();
	const json& list = member(params, "diagnostics");
	const bool truncated = list.is_array() && list.size() > limits.diagnostics;
	if(list.is_array())
		for(const json& diagnostic : list) {
			if(items.size() >= limits.diagnostics) break;
			const auto [severity, color] = severityOf(diagnostic);
			Span span = spanOf(member(diagnostic, "range"), *id, columns);
			const std::string message = text(diagnostic, "message", limits.text);
			const std::string origin = text(diagnostic, "source", 64);
			items.push_back({{"severity", severity},
			                 {"message", origin.empty() ? message : message + " (" + origin + ")"},
			                 {"file", *id},
			                 {"label", fileName(*id)},
			                 {"line", span.line + 1},
			                 {"column", span.column + 1},
			                 {"source", "lsp"}});
			if(span.endLine == span.line && span.endColumn <= span.column) span.endColumn = span.column + 1;
			decorations.push_back({{"line", span.line},
			                       {"column", span.column},
			                       {"endLine", span.endLine},
			                       {"endColumn", span.endColumn},
			                       {"kind", "squiggle"},
			                       {"color", color},
			                       {"text", message}});
		}
	return {{"id", *id}, {"items", std::move(items)}, {"decorations", std::move(decorations)}, {"truncated", truncated}};
}

/// @brief A formatting's edits, all of one file.
/// @param result textDocument/formatting's result: TextEdit[] or null.
/// @param id The file.
/// @param limits The limits.
/// @param columns The mapper.
/// @return `{ "edits", "files", "truncated" }`, or `{ "error" }`.
json formatting(const json& result, const std::string& id, const Limits& limits, const ColumnMapper& columns) {
	json edits = json::array();
	if(!result.is_null())
		if(auto problem = addEdits(id, result, limits, columns, edits)) return {{"error", *problem}};
	const bool none = edits.empty();
	return {{"edits", std::move(edits)}, {"files", none ? 0 : 1}, {"truncated", false}};
}

/// @brief The byte offset of a position (from 1, columns in code points) in a text.
/// @param lineStarts Where each line starts.
/// @param text The text.
/// @param line The line, from 1.
/// @param column The column, from 1.
/// @return The offset, or std::nullopt past the text.
std::optional<std::size_t> offsetIn(const std::vector<std::size_t>& lineStarts, std::string_view text, std::size_t line,
                                    std::size_t column) {
	if(line == 0 || column == 0 || line > lineStarts.size()) return std::nullopt;
	std::size_t at = lineStarts[line - 1];
	const std::size_t end = line < lineStarts.size() ? lineStarts[line] - 1 : text.size();
	for(std::size_t step = 1; step < column; ++step) {
		if(at >= end) return std::nullopt;
		++at;
		while(at < end && (static_cast<unsigned char>(text[at]) & 0xC0) == 0x80) ++at;
	}
	return at;
}

} // namespace

json serverFor(std::string_view language) {
	std::string name(language);
	std::ranges::transform(name, name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	if(name == "c") return {{"languageId", "c"}, {"service", "lsp_cpp"}};
	if(name == "c++" || name == "cpp") return {{"languageId", "cpp"}, {"service", "lsp_cpp"}};
	if(name == "python" || name == "py") return {{"languageId", "python"}, {"service", "lsp_python"}};
	if(name == "javascript" || name == "js") return {{"languageId", "javascript"}, {"service", "lsp_typescript"}};
	if(name == "typescript" || name == "ts") return {{"languageId", "typescript"}, {"service", "lsp_typescript"}};
	return nullptr;
}

json normalize(std::string_view kind, const json& result, const fs::path& root, const std::string& id,
               const Limits& limits, const ColumnMapper& columns) {
	if(kind == "hover") return {{"text", documentationOf(member(result, "contents"), limits.documentation)}};
	if(kind == "completion") return completion(result, limits);
	if(kind == "signature") return signature(result, limits);
	if(kind == "definition" || kind == "references") return locations(result, root, limits, columns);
	if(kind == "rename") return workspaceEdit(result, root, limits, columns);
	if(kind == "format") return formatting(result, id, limits, columns);
	if(kind == "symbols" || kind == "workspaceSymbols") return symbols(result, root, id, limits, columns);
	if(kind == "tokens") return tokens(result, id, limits, columns);
	if(kind == "diagnostics") return diagnostics(result, root, limits, columns);
	throw std::invalid_argument("no result kind '" + std::string(kind) + "'");
}

std::optional<std::string> applyEdits(std::string& text, const json& edits) {
	if(!edits.is_array()) return "the edits aren't a list";
	std::vector<std::size_t> lineStarts{0};
	for(std::size_t i = 0; i < text.size(); ++i)
		if(text[i] == '\n') lineStarts.push_back(i + 1);
	struct Edit {
		std::size_t from, to;
		std::string text;
	};
	std::vector<Edit> ranges;
	for(const json& edit : edits) {
		const auto from = offsetIn(lineStarts, text, number(edit, "line"), number(edit, "column"));
		const auto to = offsetIn(lineStarts, text, number(edit, "endLine"), number(edit, "endColumn"));
		if(!from || !to || *to < *from) return "an edit is past the end of the text";
		ranges.push_back({*from, *to, member(edit, "text").is_string() ? edit["text"].get<std::string>() : ""});
	}
	std::ranges::stable_sort(ranges, {}, &Edit::from);
	for(std::size_t i = 1; i < ranges.size(); ++i)
		if(ranges[i].from < ranges[i - 1].to) return "two edits overlap";
	std::string out;
	std::size_t at = 0;
	for(const Edit& edit : ranges) {
		out.append(text, at, edit.from - at);
		out += edit.text;
		at = edit.to;
	}
	out.append(text, at);
	text = std::move(out);
	return std::nullopt;
}

} // namespace forge::lsp
