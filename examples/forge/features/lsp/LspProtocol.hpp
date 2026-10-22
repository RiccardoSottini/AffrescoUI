#pragma once

/// @file
/// @brief The language server protocol as Forge's lsp feature reads it: file URIs, and the servers' results turned into
/// the small, bounded shapes the screen shows (lists from 1, decorations and tokens from 0). Plain functions with no
/// window or process, so the unit tests (tests/unit/lsp.cpp) and the scenarios (`forge.lsp.normalize`) use them too.
///
/// A language server is untrusted: every list is cut to its limit (`truncated` says so), every text to its length,
/// nesting to a depth, and a location outside the open folder is dropped, or refuses a whole edit.

#include <affresco/core/Json.hpp>

#include <cstddef>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace forge::lsp {

/// @brief How much of a server's answer is kept.
struct Limits {
	std::size_t diagnostics = 500;     ///< Diagnostics per file.
	std::size_t completions = 1000;    ///< Completion items.
	std::size_t edits = 5000;          ///< Edits of a rename or a formatting.
	std::size_t locations = 2000;      ///< Definitions, references and symbols.
	std::size_t tokens = 100'000;      ///< Semantic tokens per file.
	std::size_t text = 4000;           ///< Bytes of a label, a detail or a message.
	std::size_t documentation = 8000;  ///< Bytes of a hover or an item's documentation.
	std::size_t editBytes = 1 << 20;   ///< Bytes of one edit's new text.
	int depth = 32;                    ///< Nesting of document symbols.
};

/// @brief Turns a server's column into the editor's (code points), e.g. from UTF-16 units with the line's text.
/// Arguments: the file's id, the line (from 0), the server's column. Empty: the columns are code points already.
using ColumnMapper = std::function<std::size_t(const std::string& id, std::size_t line, std::size_t column)>;

/// @brief The `file://` URI of a file of the folder, its path percent-encoded.
/// @param root The open folder, absolute.
/// @param id The file, relative to it with `/`; "" for the folder itself.
/// @return The URI.
[[nodiscard]] std::string uriOf(const std::filesystem::path& root, std::string_view id);

/// @brief The file of the folder a URI names.
/// @param root The open folder, absolute.
/// @param uri A URI from a server.
/// @return Its id (relative, with `/`), or std::nullopt if it isn't a `file://` URI of a file inside the folder.
[[nodiscard]] std::optional<std::string> idOfUri(const std::filesystem::path& root, std::string_view uri);

/// @brief The LSP language id and the Forge service for an editor language.
/// @param language The editor's language ("c++", "python", "typescript", ...).
/// @return `{ "languageId", "service" }`, or null for a language no server is granted for.
[[nodiscard]] affresco::json serverFor(std::string_view language);

/// @brief A server's result, or a notification's params, as the screen shows it.
/// @param kind What it answers:
///   - "hover" → `{ "text" }` (markdown or plain, cut to `documentation`);
///   - "completion" → `{ "items": [{ "label", "detail", "insert", "kind" }], "incomplete", "truncated" }` (snippet
///     placeholders become their default text);
///   - "signature" → `{ "label", "documentation", "active", "parameter" }`, or null;
///   - "definition", "references" → `{ "locations": [{ "id", "line", "column", "endLine", "endColumn", "label" }],
///     "truncated" }` (from 1; outside the folder dropped);
///   - "rename" → `{ "edits": [{ "id", "line", "column", "endLine", "endColumn", "text" }], "files", "truncated" }`
///     (from 1), or `{ "error" }` if one edit leaves the folder, creates, renames or deletes a file, or they are over
///     `edits`;
///   - "format" → like "rename", for one file: the file's id is `id`;
///   - "symbols", "workspaceSymbols" → `{ "symbols": [{ "name", "detail", "kind", "id", "line", "column", "depth"
///     }], "truncated" }` (from 1);
///   - "tokens" → `[{ "line", "column", "length", "style" }]` (from 0) from `{ "data", "legend" }`;
///   - "diagnostics" → `{ "id", "items": [{ "severity", "message", "file", "label", "line", "column", "source" }],
///     "decorations": [{ "line", "column", "endLine", "endColumn", "kind": "squiggle", "color", "text" }] }`, or
///     `{ "error" }` for a file outside the folder.
/// @param result What the server sent.
/// @param root The open folder, absolute.
/// @param id The file the request was about (for "format" and "tokens"; "" otherwise).
/// @param limits How much is kept.
/// @param columns Maps the server's columns; empty for code points.
/// @return The shape above.
/// @throws std::invalid_argument For an unknown kind.
[[nodiscard]] affresco::json normalize(std::string_view kind, const affresco::json& result,
                                       const std::filesystem::path& root, const std::string& id = {},
                                       const Limits& limits = {}, const ColumnMapper& columns = {});

/// @brief Applies edits to a text, as "rename" and "format" give them (from 1, columns in code points).
/// @param text The text; unchanged if an error is returned.
/// @param edits The edits of one file: `[{ "line", "column", "endLine", "endColumn", "text" }]`.
/// @return An error (an edit past the text, or two that overlap), or std::nullopt.
[[nodiscard]] std::optional<std::string> applyEdits(std::string& text, const affresco::json& edits);

/// @brief A text cut to a number of bytes, at a character boundary.
/// @param text The text.
/// @param bytes At most this many bytes.
/// @return It, cut with "…" if it was longer.
[[nodiscard]] std::string cut(std::string_view text, std::size_t bytes);

/// @brief A snippet's text without its placeholders: `${1:name}` becomes `name`, `$1` and `$0` nothing, `\$` `$`.
/// @param snippet The snippet.
/// @return The plain text.
[[nodiscard]] std::string plainSnippet(std::string_view snippet);

/// @brief The style a semantic token type colours as, among the grammar's ("type", "keyword", "string", ...).
/// @param type The token type, e.g. "class", "macro".
/// @return The style.
[[nodiscard]] std::string styleOfToken(std::string_view type);

} // namespace forge::lsp
