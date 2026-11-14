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
#include <vector>

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

	/// @brief The parameters a function literal's body has used so far, `%` to `%20` and `%&`.
	struct LambdaParameters {
		int most = 0;      ///< The highest number used, `%` being 1.
		bool rest = false; ///< `%&` is used.
	};

	/// @brief Reads a form that starts with a token. A reader conditional that chooses nothing is skipped, as `#_`
	/// skips a form, and the form is the next one.
	/// @param token The token.
	/// @param depth How many forms hold it.
	/// @return The form.
	Value form(const Token& token, int depth);
	/// @brief Reads a form that starts with a token into the items of the list or the vector it is in: one item; none
	/// for a reader conditional that chooses nothing; a splicing one's items.
	/// @param token The token.
	/// @param depth How many forms hold it.
	/// @param into Gets the items.
	void elements(const Token& token, int depth, std::vector<Value>& into);
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
	/// @brief Reads the form a prefix stands before: a quote's, or a reader form's after a '#'.
	/// @param token The prefix.
	/// @param depth How many forms hold it.
	/// @return The form.
	/// @throws LispError Where the language doesn't read it.
	Value prefix(const Token& token, int depth);
	/// @brief Reads a prefix's form: `'x` is `(quote x)`.
	/// @param prefix The prefix.
	/// @param name The form it stands for.
	/// @param depth How many forms hold it.
	/// @return The list.
	Value prefixed(const Token& prefix, std::string_view name, int depth);
	/// @brief Reads a form after a '#' (ReaderForms.cpp): one of the four, by its token's kind.
	/// @param token The token, its bracket still to read.
	/// @param depth How many forms hold it.
	/// @return The form.
	/// @throws LispError Where the language doesn't read it.
	Value hashForm(const Token& token, int depth);
	/// @brief Reads a function literal, `#(...)` (ReaderForms.cpp): `(fn [% %2 ... & %&] (...))`, as many parameters
	/// as the highest one its body uses.
	/// @param token The `#(` token, its bracket still to read.
	/// @param depth How many forms hold it.
	/// @return The `fn` form, at the place of its '#'.
	/// @throws LispError Inside another, or with a parameter past %20.
	Value lambda(const Token& token, int depth);
	/// @brief The name a function literal's parameter is read as: `%1.price` is `%.price`.
	/// @param token A word that starts with '%', inside a function literal.
	/// @return The name, or std::nullopt if the word is no parameter (`%map`).
	/// @throws LispError For `%0`, or a number past 20.
	std::optional<std::string> lambdaParameter(const Token& token);
	/// @brief Reads a set literal, `#{...}` (ReaderForms.cpp).
	/// @param token The `#{` token, its bracket still to read.
	/// @param depth How many forms hold it.
	/// @return The form.
	/// @throws LispError Always: a set is a vector in the language.
	Value setOf(const Token& token, int depth);
	/// @brief Reads a reader conditional, `#?(...)` or `#?@(...)` (ReaderForms.cpp): pairs of a platform's keyword and a
	/// form, every form read and the first after `:affresco` or `:default` chosen.
	/// @param token The `#?` or `#?@` token, its bracket still to read.
	/// @param depth How many forms hold it.
	/// @return The form chosen, or std::nullopt if none is.
	/// @throws LispError If it isn't pairs of a keyword and a form.
	std::optional<Value> conditional(const Token& token, int depth);
	/// @brief Reads the form a reader conditional chooses, or the next form if it chooses none, past as many as choose
	/// none (ReaderForms.cpp).
	/// @param token The first token: a `#?`, or the token of a form.
	/// @param depth How many forms hold it.
	/// @return The form.
	Value chosenOrNext(Token token, int depth);
	/// @brief Reads a character, `\a` or `\newline`, as a string of it (ReaderForms.cpp).
	/// @param token The character's token.
	/// @return The string.
	/// @throws LispError For a name it doesn't know.
	Value character(const Token& token);
	/// @brief Reads Clojure's ways to write a number as the nearest JSON number (ReaderForms.cpp): `12N` is 12, `1.5M`
	/// 1.5, `1/2` 0.5, `0x1F` 31.
	/// @param token The word.
	/// @return The number, or std::nullopt if the word isn't written so.
	/// @throws LispError If it doesn't fit 64 bits, or divides by zero.
	std::optional<json> clojureNumber(const Token& token) const;
	/// @brief Reads a keyword, refusing one with a namespace (`::name`, `:a/b`).
	/// @param token The keyword's token.
	/// @return The keyword.
	Value keyword(const Token& token);
	/// @brief Refuses the forms after '#' a Clojure writer may type that read as an id: `##Inf`, `#'name`.
	/// @param token An id's token.
	void refuseHashWord(const Token& token) const;
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

	Heap& heap_;              ///< See the constructor.
	notation::Lexer lexer_;   ///< The tokens.
	const std::string* file_; ///< See the constructor.
	std::size_t firstLine_;   ///< See the constructor.
	std::size_t firstColumn_; ///< See the constructor.
	std::size_t scanned_ = 0; ///< How far placeAt() has looked.
	std::size_t line_ = 0;    ///< Line breaks before scanned_.
	std::size_t column_ = 1;  ///< The column of scanned_, in characters.
	LambdaParameters* lambda_ = nullptr; ///< The function literal being read, if any; it lives on lambda()'s stack.
};

/// @brief Tells whether a word can be a symbol: ASCII letters, digits and `_ . @ $ - + * / < > = ! ? & %`, not starting
/// with a digit, or with a sign or a dot and a digit.
/// @param text The word.
/// @return true if it can.
[[nodiscard]] bool isSymbolWord(std::string_view text) noexcept;

} // namespace affresco::lisp
