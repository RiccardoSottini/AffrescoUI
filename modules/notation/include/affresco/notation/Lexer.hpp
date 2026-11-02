#pragma once

/// @file
/// @brief The tokens of Lisp text, which the notation's reader and the language's (modules/lisp) share, so that the two
/// read every word, string and number the same.

#include <affresco/core/Json.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace affresco::notation {

/// @brief What a token is.
enum class TokenKind {
	OpenNode,  ///< `(`
	CloseNode, ///< `)`
	OpenList,  ///< `[`
	CloseList, ///< `]`
	OpenMap,   ///< `{`
	CloseMap,  ///< `}`
	String,    ///< `"text"`, with JSON's escapes
	Key,       ///< `:name` or `:"name"`
	Id,        ///< `#name` or `#"name"`
	Word,      ///< anything else between delimiters: a bare word, a number, true, false or nil
	Quote,     ///< `'`, in language text only
	Backquote, ///< a backquote, in language text only
	Unquote,   ///< `~`, in language text only
	Splice,    ///< `~@`, in language text only
	End        ///< the end of the text
};

/// @brief A token: what it is, its text, and where it starts.
struct Token {
	TokenKind kind = TokenKind::End; ///< What it is.
	std::string_view text;           ///< Its text, as written (with its ':' or '#').
	std::size_t offset = 0;          ///< Where it starts in the text, in bytes.
};

/// @brief A comment, or a form discarded with `#_`: text that isn't part of the tree.
struct SourceComment {
	std::string_view text; ///< Its text, as written.
	bool sameLine = false; ///< It ends a line that a token is on.
};

/// @brief Tells whether a character ends a bare word: a space, a comma, a bracket, a quote or a comment.
/// @param c The character.
/// @return true if it does.
[[nodiscard]] constexpr bool isDelimiter(char c) noexcept { return std::string_view(" \t\r\n,()[]{}\";").contains(c); }

/// @brief Tells whether a text is a JSON number: `-?(0|[1-9][0-9]*)(.[0-9]+)?([eE][+-]?[0-9]+)?`.
/// @param text The text.
/// @return true if it is.
[[nodiscard]] bool isJsonNumber(std::string_view text) noexcept;

/// @brief Reads Lisp text a token at a time, and says where a problem is.
///
/// Spaces, commas and comments are skipped. `#_` discards the next form, without recursion, and isn't a token. Only a
/// `#_` followed by a word character is an id, as it always was: `#_x` is the id `_x` and `#_ x` discards x.
class Lexer {
public:
	/// @brief Reads a text.
	/// @param text The text; it must outlive the lexer.
	/// @param language Whether `'`, the backquote, `~` and `~@` are tokens (the language) or words (the notation).
	/// @throws NotationError If the text is longer than maxLispBytes.
	explicit Lexer(std::string_view text, bool language = false);

	/// @brief Keeps the comments skipped from now on, for takeComments().
	void keepComments() noexcept { keepComments_ = true; }

	/// @brief The comments skipped since this was last called, in order.
	/// @return Them.
	[[nodiscard]] std::vector<SourceComment> takeComments() { return std::exchange(comments_, {}); }

	/// @brief Reads the next token, past spaces, comments and discarded forms.
	/// @return It.
	/// @throws NotationError If a string or a quoted name never ends, or a discarded form is unfinished.
	Token next();

	/// @brief Where the last token read ends.
	/// @return The place, in bytes.
	[[nodiscard]] std::size_t lastEnd() const noexcept { return lastEnd_; }

	/// @brief The text.
	/// @return It.
	[[nodiscard]] std::string_view text() const noexcept { return text_; }

	/// @brief The name a key or an id token gives.
	/// @param token The token: `:name`, `:"name"`, `#name` or `#"name"`.
	/// @return The name.
	/// @throws NotationError If the name is neither a plain word nor a string in quotes.
	[[nodiscard]] std::string name(const Token& token) const;

	/// @brief Reads a string in quotes, with JSON's escapes.
	/// @param written Its text, quotes included.
	/// @param offset Where it starts, for the error.
	/// @return The string.
	/// @throws NotationError If it isn't a JSON string (an unknown escape, a control character, bytes that aren't
	/// UTF-8).
	[[nodiscard]] std::string unquote(std::string_view written, std::size_t offset) const;

	/// @brief The value of a word that stands for a value: true, false, nil or a number.
	/// @param token The word.
	/// @return Its value, or std::nullopt if it is a word that stands for itself.
	/// @throws NotationError If it is a number that is too long or too large.
	[[nodiscard]] std::optional<json> constant(const Token& token) const;

	/// @brief The start of a text an error quotes: its first 40 bytes, cut before a character rather than inside one,
	/// and "..." if there is more.
	/// @param text The text.
	/// @return Its start.
	[[nodiscard]] static std::string excerpt(std::string_view text);

	/// @brief A token as an error shows it: its excerpt(), in quotes.
	/// @param token The token.
	/// @return The text.
	[[nodiscard]] static std::string shown(const Token& token);

	/// @brief The line and column of a place in the text.
	/// @param offset The place, in bytes.
	/// @return Its line and its column (in characters), from 1.
	[[nodiscard]] std::pair<std::size_t, std::size_t> lineAndColumn(std::size_t offset) const;

	/// @brief A place, as an error names it.
	/// @param offset The place, in bytes.
	/// @return "line 3, column 8".
	[[nodiscard]] std::string placeOf(std::size_t offset) const;

	/// @brief Reports a problem at a place in the text.
	/// @param offset The place, in bytes.
	/// @param message What is wrong.
	/// @throws NotationError Always.
	[[noreturn]] void fail(std::size_t offset, const std::string& message) const;

private:
	/// @brief Reads the next token, past spaces and comments: a `#_` is a token here.
	/// @param discard Set if the token is a `#_`.
	/// @return It.
	Token raw(bool& discard);
	/// @brief Moves past spaces, commas and comments.
	void skipSpace();
	/// @brief Moves past a string in quotes, its escapes included.
	/// @param start Where its opening quote is.
	void quoted(std::size_t start);
	/// @brief Moves past a bare word: everything up to a delimiter.
	/// @param start Where it starts.
	void wordAt(std::size_t start) noexcept;
	/// @brief Moves past one discarded form, after its `#_`, one token at a time.
	/// @param start Where the `#_` is.
	void discardForm(std::size_t start);

	std::string_view text_;               ///< See the constructor.
	std::size_t at_ = 0;                  ///< Where the next token is looked for.
	std::size_t lastEnd_ = 0;             ///< See lastEnd().
	bool language_ = false;               ///< See the constructor.
	bool keepComments_ = false;           ///< See keepComments().
	std::vector<SourceComment> comments_; ///< See takeComments().
};

} // namespace affresco::notation
