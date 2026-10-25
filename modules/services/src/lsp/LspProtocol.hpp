#pragma once

/// @file
/// @brief The Language Server Protocol as the `lsp` service reads it: file URIs, positions in UTF-16 units or code
/// points, and a server's results turned into the small, bounded shapes a screen gets. Plain functions with no process,
/// so the unit tests (tests/unit/lsp.cpp) and the fuzzer (tests/fuzz/lsp.cpp) use them too.
///
/// A language server is untrusted: every list is cut to its limit (`truncated` says so), every text to its length,
/// nesting to a depth, and a location outside the folder is dropped, or refuses a whole edit. Every position a screen
/// gets counts from 0, its columns in code points (characters), as the `editor` node counts them.

#include "affresco/core/Json.hpp"

#include <cstddef>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace affresco::services::lsp {

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

/// @brief Turns a server's column into code points, e.g. from UTF-16 units with the line's text.
/// Arguments: the file (relative to the folder), the line (from 0), the server's column. Empty: the columns are code
/// points already.
using ColumnMapper = std::function<std::size_t(const std::string& path, std::size_t line, std::size_t column)>;

/// @brief The `file://` URI of a file of the folder, its path percent-encoded.
/// @param root The folder, absolute.
/// @param path The file, relative to it with `/`; "" for the folder itself.
/// @return The URI.
[[nodiscard]] std::string uriOf(const std::filesystem::path& root, std::string_view path);

/// @brief The file of the folder a URI names.
/// @param root The folder, absolute.
/// @param uri A URI from a server.
/// @return Its path (relative, with `/`), or std::nullopt if it isn't a `file://` URI of a file inside the folder.
[[nodiscard]] std::optional<std::string> pathOfUri(const std::filesystem::path& root, std::string_view uri);

/// @brief A server's result, or a notification's params, as a screen gets it.
/// @param kind What it answers:
///   - "hover" → `{ "text" }` (markdown or plain, cut to `documentation`);
///   - "completion" → `{ "items": [{ "label", "detail", "insert", "filter", "kind" }], "incomplete", "truncated" }`
///     (snippet placeholders become their default text);
///   - "signature" → `{ "label", "documentation", "parameter", "active", "count" }`, or null;
///   - "definition", "references" → `{ "locations": [{ "path", "line", "column", "endLine", "endColumn" }],
///     "truncated" }` (outside the folder dropped);
///   - "rename" → `{ "edits": [{ "path", "line", "column", "endLine", "endColumn", "text" }], "files", "truncated" }`;
///   - "format" → like "rename", all of the file @p path;
///   - "symbols", "workspaceSymbols" → `{ "symbols": [{ "name", "detail", "kind", "path", "line", "column",
///     "depth" }], "truncated" }`;
///   - "tokens" → `{ "tokens": [{ "line", "column", "length", "style" }], "truncated" }` from `{ "data", "legend" }`;
///   - "diagnostics" → `{ "path", "diagnostics": [{ "severity", "message", "source", "code", "line", "column",
///     "endLine", "endColumn" }], "truncated" }`, the severity "error", "warning", "info" or "hint".
/// @param result What the server sent.
/// @param root The folder, absolute.
/// @param path The file the request was about (for "format", "symbols" and "tokens"; "" otherwise).
/// @param limits How much is kept.
/// @param columns Maps the server's columns; empty for code points.
/// @return The shape above.
/// @throws std::invalid_argument For an unknown kind.
/// @throws std::runtime_error For an edit that is refused whole (one leaves the folder, creates, renames or deletes a
/// file, or they are over `edits`), or diagnostics of a file outside the folder.
[[nodiscard]] json normalize(std::string_view kind, const json& result, const std::filesystem::path& root,
                             const std::string& path = {}, const Limits& limits = {}, const ColumnMapper& columns = {});

/// @brief A text cut to a number of bytes, at a character boundary.
/// @param text The text.
/// @param bytes At most this many bytes.
/// @return It, cut with "…" if it was longer.
[[nodiscard]] std::string cut(std::string_view text, std::size_t bytes);

/// @brief A snippet's text without its placeholders: `${1:name}` becomes `name`, `$1` and `$0` nothing, `\$` `$`.
/// @param snippet The snippet.
/// @return The plain text.
[[nodiscard]] std::string plainSnippet(std::string_view snippet);

/// @brief The style a semantic token type colours as, among a grammar's ("type", "keyword", "string", ...).
/// @param type The token type, e.g. "class", "macro".
/// @return The style.
[[nodiscard]] std::string styleOfToken(std::string_view type);

/// @brief UTF-16 units of a line's first code points.
/// @param line The line, UTF-8.
/// @param codePoints How many code points.
/// @return Their UTF-16 units (past the line's end, one unit per code point).
[[nodiscard]] std::size_t utf16Units(std::string_view line, std::size_t codePoints) noexcept;

/// @brief Code points of a line's first UTF-16 units.
/// @param line The line, UTF-8.
/// @param units How many UTF-16 units.
/// @return Their code points (a unit in the middle of a pair counts its code point; past the end, one per unit).
[[nodiscard]] std::size_t codePointsOf(std::string_view line, std::size_t units) noexcept;

} // namespace affresco::services::lsp
