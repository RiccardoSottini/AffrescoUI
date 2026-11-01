#pragma once

/// @file
/// @brief The notations a document can be written in: JSON, and a Lisp notation (S-expressions) that is only another
/// spelling of the same JSON tree. Reading turns text into the tree, writing turns the tree into text; nothing is ever
/// evaluated.

#include <affresco/core/Json.hpp>

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace affresco::notation {

/// @brief How a document is written.
enum class Notation {
	Json, ///< JSON, as every document was written before.
	Lisp  ///< S-expressions: `(column #list :gap 4 (label #title :text Hello))` (README.md).
};

/// Longest text read in the Lisp notation, in bytes: four times the largest file a screen includes (16 MiB), far more
/// than any screen, small enough that what it builds stays bounded.
inline constexpr std::size_t maxLispBytes = std::size_t{64} << 20;

/// @brief Text that isn't a document in the Lisp notation, and where.
///
/// what() names the place: "AffrescoUI screen: board.screen.lisp: line 3, column 8: expected ')' to close the node
/// opened at line 1, column 1" (the file when it is known).
class NotationError : public DocumentError {
public:
	/// @brief A problem at a place in the text.
	/// @param file The file the text is from, or "" if it isn't known.
	/// @param line Its line, from 1.
	/// @param column Its column, from 1, in characters.
	/// @param message What is wrong.
	NotationError(const std::string& file, std::size_t line, std::size_t column, const std::string& message);

	/// @brief The line of the problem, from 1. @return It.
	[[nodiscard]] std::size_t line() const noexcept { return line_; }
	/// @brief The column of the problem, from 1, in characters. @return It.
	[[nodiscard]] std::size_t column() const noexcept { return column_; }
	/// @brief What is wrong, without the place. @return It.
	[[nodiscard]] const std::string& message() const noexcept { return message_; }

private:
	std::size_t line_;    ///< See line().
	std::size_t column_;  ///< See column().
	std::string message_; ///< See message().
};

/// @brief The notation a file is written in, by its name: `.lisp` (e.g. `board.screen.lisp`) is the Lisp notation,
/// anything else JSON.
/// @param file The file.
/// @return Its notation.
[[nodiscard]] Notation notationOf(const std::filesystem::path& file);

/// @brief The file of a document that is found by its name (an app's `app`, a feature's `feature`), in a folder:
/// `<name>.json`, or `<name>.lisp` when only that one is there.
/// @param folder The folder.
/// @param name The document's name, without its extension.
/// @return The file; `<name>.json` if neither is there, so that the problem names what was looked for.
[[nodiscard]] std::filesystem::path namedDocument(const std::filesystem::path& folder, std::string_view name);

/// @brief Whether a file is a document found by this name, in either notation.
/// @param file The file.
/// @param name The document's name, without its extension.
/// @return Whether it is.
[[nodiscard]] bool isNamedDocument(const std::filesystem::path& file, std::string_view name);

/// @brief A notation by its name, as the protocol and the command line give it.
/// @param name "json" or "lisp".
/// @return The notation, or std::nullopt for any other name.
[[nodiscard]] std::optional<Notation> notationNamed(std::string_view name) noexcept;

/// @brief The name of a notation.
/// @param notation The notation.
/// @return "json" or "lisp".
[[nodiscard]] std::string_view nameOf(Notation notation) noexcept;

/// @brief Reads a document, or any value, written in a notation.
/// @param text The text.
/// @param notation Its notation: JSON is read by parseUntrusted(), exactly as before.
/// @return The value.
/// @throws json::parse_error If JSON text isn't JSON.
/// @throws DocumentError If JSON text nests deeper than maxNesting or holds an overlong number; a NotationError (a
/// DocumentError) with the line and column if Lisp text isn't a value, nests deeper than maxNesting or is longer than
/// maxLispBytes.
[[nodiscard]] json read(std::string_view text, Notation notation);

/// @brief Reads a document file's text in the notation its name says (notationOf()). The one function every reader
/// of documents on disk goes through (a screen, its includes, an app's entry and parts, the command-line tool), so
/// that each can be written in either notation.
/// @param text The file's text.
/// @param file The file.
/// @return The value: JSON is read by parseUntrusted(), exactly as before.
/// @throws json::parse_error If JSON text isn't JSON.
/// @throws DocumentError As read() throws it; a NotationError names the file.
[[nodiscard]] json readDocument(std::string_view text, const std::filesystem::path& file);

/// @brief A document file's text as JSON, for the readers that take JSON text (Screen::fromString(), which also
/// recognises other formats in JSON files): JSON text as it is, Lisp text as the JSON of the tree it spells.
/// @param text The file's text.
/// @param file The file, for its notation.
/// @return The JSON text.
/// @throws DocumentError If Lisp text isn't a value (a NotationError, naming the file).
[[nodiscard]] std::string jsonText(std::string_view text, const std::filesystem::path& file);

/// @brief Writes a value in a notation.
///
/// JSON is written compactly, as the protocol and the history write it. Lisp is written canonically: the same value
/// always gives the same text, and reading it back gives the value, key order included.
/// @param value The value.
/// @param notation The notation.
/// @return The text.
/// @throws DocumentError If the value nests deeper than maxNesting.
[[nodiscard]] std::string write(const json& value, Notation notation);

} // namespace affresco::notation
