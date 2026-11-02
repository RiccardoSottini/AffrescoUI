#pragma once

/// @file
/// @brief The language's reader: text into forms, values that know their place. It shares its tokens with the notation
/// (notation/Lexer.hpp), so the two read every word, string and number the same.

#include "Objects.hpp"

#include <affresco/notation/Lexer.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace affresco::lisp {

/// @brief A form read, and where its text ends.
struct ReadForm {
	Value form;            ///< The form.
	std::size_t start = 0; ///< Where its text starts, in bytes.
	std::size_t end = 0;   ///< Where its text ends, in bytes.
};

/// @brief Reads a text's top-level forms, one at a time.
class Reader {
public:
	/// @brief Reads a text.
	/// @param heap Where the forms go.
	/// @param text The text; it must outlive the reader.
	/// @param file Its name.
	/// @param line The line it starts on.
	/// @param column The column it starts on.
	/// @throws LispError If it is too long.
	Reader(Heap& heap, std::string_view text, const std::string& file, std::size_t line = 1, std::size_t column = 1);

	/// @brief Reads the next top-level form.
	/// @return It, or std::nullopt at the end of the text.
	/// @throws LispError If it can't be read, naming the place.
	std::optional<ReadForm> next();

private:
	using Token = notation::Token;
	using TokenKind = notation::TokenKind;

	/// @brief Reads a form that starts with a token.
	/// @param token The token.
	/// @param depth How many forms hold it.
	/// @return The form.
	Value form(const Token& token, int depth);
	/// @brief Reads a list, after its '('.
	/// @param open The '('.
	/// @param depth How many forms hold it.
	/// @return The list.
	Value list(const Token& open, int depth);
	/// @brief Reads a vector, after its '['.
	/// @param open The '['.
	/// @param depth How many forms hold it.
	/// @return The vector.
	Value vector(const Token& open, int depth);
	/// @brief Reads a map, after its '{'.
	/// @param open The '{'.
	/// @param depth How many forms hold it.
	/// @return The map, or a list `(%map k v ...)` if a key isn't a keyword or a string.
	Value map(const Token& open, int depth);
	/// @brief Reads a prefix's form: `'x` is `(quote x)`.
	/// @param prefix The prefix.
	/// @param name The form it stands for.
	/// @param depth How many forms hold it.
	/// @return The list.
	Value prefixed(const Token& prefix, std::string_view name, int depth);
	/// @brief Reads a bare word: a constant or a symbol.
	/// @param token The word.
	/// @return The value.
	Value word(const Token& token);
	/// @brief Reads a list's head, `panel#x` being `panel :id "x"`.
	/// @param token The head.
	/// @param items Gets what it stands for.
	/// @param depth How many forms hold it.
	void head(const Token& token, std::vector<Value>& items, int depth);
	/// @brief Refuses a form that would nest deeper than maxNesting.
	/// @param token Its first token.
	/// @param levels How many levels it would reach.
	void shallowEnough(const Token& token, int levels) const;
	/// @brief The place of an offset; offsets only go forward between calls.
	/// @param offset The offset.
	/// @return The place.
	Place placeAt(std::size_t offset);

	Heap& heap_;                ///< See the constructor.
	notation::Lexer lexer_;     ///< The tokens.
	const std::string* file_;   ///< See the constructor.
	std::size_t firstLine_;     ///< See the constructor.
	std::size_t firstColumn_;   ///< See the constructor.
	std::size_t scanned_ = 0;   ///< How far placeAt() has looked.
	std::size_t line_ = 0;      ///< Line breaks before scanned_.
	std::size_t column_ = 1;    ///< The column of scanned_, in characters.
};

/// @brief Tells whether a word can be a symbol: ASCII letters, digits and `_ . @ $ - + * / < > = ! ? & %`, not starting
/// with a digit, or with a sign or a dot and a digit.
/// @param text The word.
/// @return true if it can.
[[nodiscard]] bool isSymbolWord(std::string_view text) noexcept;

} // namespace affresco::lisp
