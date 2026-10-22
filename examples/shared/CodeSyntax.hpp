#pragma once

/// @file
/// @brief The CodeEditor's own lexers: C++ (and the C-like languages), JSON, CMake and Markdown, for the editors that
/// have no tree-sitter grammar for them. Only CodeEditor.cpp includes it: it is a header so that every target that
/// compiles CodeEditor.cpp by name builds it unchanged.

#include "CodeEditor.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string_view>
#include <vector>

namespace examples::syntax {

using Token = CodeEditor::Token;

/// @brief Whether a code point is part of a word (letters, digits, underscore).
/// @param c The code point.
inline bool isWord(char32_t c) noexcept {
	return (c >= U'a' && c <= U'z') || (c >= U'A' && c <= U'Z') || (c >= U'0' && c <= U'9') || c == U'_' ||
	       (c >= 0xC0 && c <= 0x24F && c != 0xD7 && c != 0xF7);
}

/// @brief Whether a code point is a decimal digit.
/// @param c The code point.
inline bool isDigit(char32_t c) noexcept { return c >= U'0' && c <= U'9'; }

/// @brief Whether a code point is a space or a tab.
/// @param c The code point.
inline bool isBlank(char32_t c) noexcept { return c == U' ' || c == U'\t'; }

/// @brief Whether a word is in a sorted list.
/// @param list The list, sorted.
/// @param word The word.
template <std::size_t N> bool contains(const std::array<std::u32string_view, N>& list, std::u32string_view word) {
	return std::ranges::binary_search(list, word);
}

inline constexpr std::array<std::u32string_view, 55> cppKeywords = {
    U"alignas",   U"alignof",  U"auto",     U"break",    U"case",      U"catch",     U"class",     U"co_await",
    U"co_return", U"co_yield", U"concept",  U"const",    U"consteval", U"constexpr", U"constinit", U"continue",
    U"decltype",  U"default",  U"delete",   U"do",       U"else",      U"enum",      U"explicit",  U"export",
    U"extern",    U"false",    U"final",    U"for",      U"friend",    U"if",        U"inline",    U"mutable",
    U"namespace", U"new",      U"noexcept", U"nullptr",  U"operator",  U"override",  U"private",   U"protected",
    U"public",    U"requires", U"return",   U"static",   U"struct",    U"switch",    U"template",  U"this",
    U"throw",     U"true",     U"try",      U"typename", U"using",     U"virtual",   U"while"}; ///< C++ keywords,
	                                                                                            ///< sorted.
static_assert(std::ranges::is_sorted(cppKeywords));

inline constexpr std::array<std::u32string_view, 11> cppTypes = {
    U"bool",  U"char",   U"double", U"float",    U"int", U"long",
    U"short", U"signed", U"size_t", U"unsigned", U"void"}; ///< Built-in types, sorted.
static_assert(std::ranges::is_sorted(cppTypes));

/* Scanning a line: each function marks what it reads in `tokens` (one per code point) and returns where it stopped */

/// @brief Marks a part of a line as one kind of text.
/// @param tokens The line's tokens.
/// @param from Its first code point.
/// @param to Just after its last one (past the line's end means up to the end).
/// @param token The kind.
/// @return @p to: where the next part starts.
inline std::size_t mark(std::vector<Token>& tokens, std::size_t from, std::size_t to, Token token) {
	std::fill(tokens.begin() + static_cast<std::ptrdiff_t>(from),
	          tokens.begin() + static_cast<std::ptrdiff_t>(std::min(to, tokens.size())), token);
	return to;
}

/// @brief The end of a quoted string (a backslash escapes the next code point).
/// @param line The line.
/// @param i Its opening quote.
/// @return The index after the closing quote, or the line's end.
inline std::size_t endOfQuoted(std::u32string_view line, std::size_t i) {
	const char32_t quote = line[i];
	std::size_t j = i + 1;
	while(j < line.size() && line[j] != quote) j += line[j] == U'\\' ? 2u : 1u;
	return std::min(j + 1, line.size());
}

/// @brief The end of a number: word characters, dots and digit separators.
/// @param line The line.
/// @param i Where it starts.
/// @return The index after it.
inline std::size_t endOfNumber(std::u32string_view line, std::size_t i) {
	while(i < line.size() && (isWord(line[i]) || line[i] == U'.' || line[i] == U'\'')) ++i;
	return i;
}

/// @brief The end of a word.
/// @param line The line.
/// @param i Where it starts.
/// @return The index after it.
inline std::size_t endOfWord(std::u32string_view line, std::size_t i) {
	while(i < line.size() && isWord(line[i])) ++i;
	return i;
}

/// @brief Skips spaces and tabs.
/// @param line The line.
/// @param i Where to start.
/// @return The first index that isn't blank, or the line's end.
inline std::size_t skipBlanks(std::u32string_view line, std::size_t i) {
	while(i < line.size() && isBlank(line[i])) ++i;
	return i;
}

/// @brief The end of a part that ends with a closing text, such as a block comment's "*/".
/// @param line The line.
/// @param closer The closing text.
/// @param from Where to look for it.
/// @return The index after it, or the line's end if the line doesn't have it.
inline std::size_t endAfter(std::u32string_view line, std::u32string_view closer, std::size_t from) {
	const std::size_t end = line.find(closer, from);
	return end == std::u32string_view::npos ? line.size() : end + closer.size();
}

/// @brief Marks a C++ block comment, which may go on past the line.
/// @param line The line.
/// @param i Its "/*".
/// @param tokens The line's tokens.
/// @param inComment Set to true if it doesn't end on this line.
/// @return Where the next part starts.
inline std::size_t markBlockComment(std::u32string_view line, std::size_t i, std::vector<Token>& tokens,
                                    bool& inComment) {
	const std::size_t end = line.find(U"*/", i + 2);
	if(end == std::u32string_view::npos) inComment = true;
	return mark(tokens, i, end == std::u32string_view::npos ? line.size() : end + 2, Token::Comment);
}

/// @brief Marks a preprocessor line's directive, then a <header> or "header" after it as a string.
/// @param line The line.
/// @param tokens The line's tokens.
/// @return Where the rest of the line starts: 0 if it isn't a preprocessor line.
inline std::size_t markPreprocessor(std::u32string_view line, std::vector<Token>& tokens) {
	const std::size_t hash = line.find_first_not_of(U' ');
	if(hash == std::u32string_view::npos || line[hash] != U'#') return 0;
	const std::size_t directiveEnd = mark(tokens, hash, endOfWord(line, hash + 1), Token::Keyword);
	const std::size_t open = line.find_first_of(U"<\"", directiveEnd);
	if(open == std::u32string_view::npos) return directiveEnd;
	return mark(tokens, open, endAfter(line, line[open] == U'<' ? U">" : U"\"", open + 1), Token::String);
}

/// @brief Marks the start of a C++ line: the rest of a block comment still open from the line before, or a
/// preprocessor directive.
/// @param line The line.
/// @param tokens The line's tokens.
/// @param inComment In: whether a block comment is open; out: whether it still is.
/// @return Where the rest of the line starts.
inline std::size_t markCppLineStart(std::u32string_view line, std::vector<Token>& tokens, bool& inComment) {
	if(!inComment) return markPreprocessor(line, tokens);
	const std::size_t end = line.find(U"*/");
	if(end != std::u32string_view::npos) inComment = false;
	return mark(tokens, 0, end == std::u32string_view::npos ? line.size() : end + 2, Token::Comment);
}

/// @brief Marks a C++ word: a keyword, or a type (a built-in one, a capitalised name or "std").
/// @param line The line.
/// @param i Its first code point.
/// @param tokens The line's tokens.
/// @return The index after it.
inline std::size_t markCppWord(std::u32string_view line, std::size_t i, std::vector<Token>& tokens) {
	const std::size_t end = endOfWord(line, i);
	const std::u32string_view word = line.substr(i, end - i);
	if(contains(cppKeywords, word)) mark(tokens, i, end, Token::Keyword);
	else if(contains(cppTypes, word) || (word.front() >= U'A' && word.front() <= U'Z') || word == U"std")
		mark(tokens, i, end, Token::Type);
	return end;
}

/// @brief Marks the C++ token at an index: a comment, a string (raw ones too), a number or a word.
/// @param line The line.
/// @param i The index.
/// @param tokens The line's tokens.
/// @param inComment Set to true if a block comment starts and doesn't end on this line.
/// @return Where the next token starts (the line's end after a line comment).
inline std::size_t markCppToken(std::u32string_view line, std::size_t i, std::vector<Token>& tokens, bool& inComment) {
	const char32_t c = line[i];
	const char32_t next = i + 1 < line.size() ? line[i + 1] : 0;
	if(c == U'/' && next == U'/') return mark(tokens, i, line.size(), Token::Comment);
	if(c == U'/' && next == U'*') return markBlockComment(line, i, tokens, inComment);
	if(line.substr(i).starts_with(U"R\"(")) return mark(tokens, i, endAfter(line, U")\"", i + 3), Token::String);
	if(c == U'"' || c == U'\'') return mark(tokens, i, endOfQuoted(line, i), Token::String);
	if(isDigit(c)) return mark(tokens, i, endOfNumber(line, i), Token::Number);
	if(isWord(c)) return markCppWord(line, i, tokens);
	return i + 1;
}

/// @brief Marks a line of C++.
/// @param line The line.
/// @param tokens The line's tokens.
/// @param inComment In: whether a block comment is open at its start; out: at its end.
inline void markCpp(std::u32string_view line, std::vector<Token>& tokens, bool& inComment) {
	std::size_t i = markCppLineStart(line, tokens, inComment);
	while(i < line.size()) i = markCppToken(line, i, tokens, inComment);
}

/// @brief Marks the JSON token at an index: a key, a string, a number or a literal.
/// @param line The line.
/// @param i The index.
/// @param tokens The line's tokens.
/// @return Where the next token starts.
inline std::size_t markJsonToken(std::u32string_view line, std::size_t i, std::vector<Token>& tokens) {
	const char32_t c = line[i];
	if(c == U'"') {
		const std::size_t stop = endOfQuoted(line, i);
		const std::size_t after = skipBlanks(line, stop);
		const bool key = after < line.size() && line[after] == U':';
		return mark(tokens, i, stop, key ? Token::Type : Token::String); // keys and values differ
	}
	if(isDigit(c) || c == U'-') return mark(tokens, i, endOfNumber(line, i + 1), Token::Number);
	if(isWord(c)) {
		const std::size_t end = endOfWord(line, i);
		const std::u32string_view word = line.substr(i, end - i);
		if(word == U"true" || word == U"false" || word == U"null") mark(tokens, i, end, Token::Keyword);
		return end;
	}
	return i + 1;
}

/// @brief Marks a CMake word: a command (followed by "("), or an option in capitals such as PRIVATE.
/// @param line The line.
/// @param i Its first code point.
/// @param tokens The line's tokens.
/// @return The index after it.
inline std::size_t markCMakeWord(std::u32string_view line, std::size_t i, std::vector<Token>& tokens) {
	std::size_t end = i;
	while(end < line.size() && (isWord(line[end]) || line[end] == U':')) ++end;
	const std::size_t after = skipBlanks(line, end);
	if(after < line.size() && line[after] == U'(') mark(tokens, i, end, Token::Keyword); // a command
	else if(std::ranges::none_of(line.substr(i, end - i), [](char32_t ch) { return ch >= U'a' && ch <= U'z'; }))
		mark(tokens, i, end, Token::Type); // an option such as PRIVATE
	return end;
}

/// @brief Marks the CMake token at an index: a comment, a string, a variable, a number or a word.
/// @param line The line.
/// @param i The index.
/// @param tokens The line's tokens.
/// @return Where the next token starts (the line's end after a comment).
inline std::size_t markCMakeToken(std::u32string_view line, std::size_t i, std::vector<Token>& tokens) {
	const char32_t c = line[i];
	if(c == U'#') return mark(tokens, i, line.size(), Token::Comment);
	if(c == U'"') return mark(tokens, i, endOfQuoted(line, i), Token::String);
	if(c == U'$' && i + 1 < line.size() && line[i + 1] == U'{')
		return mark(tokens, i, endAfter(line, U"}", i), Token::Type);
	if(isDigit(c)) return mark(tokens, i, endOfNumber(line, i), Token::Number);
	if(isWord(c)) return markCMakeWord(line, i, tokens);
	return i + 1;
}

/// @brief Marks a line of Markdown: headings, indented code, list bullets and `code`.
/// @param line The line.
/// @param tokens The line's tokens.
inline void markMarkdown(std::u32string_view line, std::vector<Token>& tokens) {
	const std::size_t first = line.find_first_not_of(U' ');
	if(first == std::u32string_view::npos) return;
	if(first >= 4) {
		mark(tokens, 0, line.size(), Token::String); // an indented code block
		return;
	}
	if(line[first] == U'#') {
		mark(tokens, 0, line.size(), Token::Heading);
		return;
	}
	if((line[first] == U'-' || line[first] == U'*') && first + 1 < line.size() && line[first + 1] == U' ')
		mark(tokens, first, first + 1, Token::Keyword);
	std::size_t i = first;
	while(i < line.size()) i = line[i] == U'`' ? mark(tokens, i, endAfter(line, U"`", i + 1), Token::String) : i + 1;
}

/// @brief The kind of each code point of a line.
/// @param language The editor's language (Plain and Python mark nothing: a grammar colours Python).
/// @param line The line.
/// @param inComment In: whether a C++ block comment is open at its start; out: at its end.
/// @return One token per code point.
inline std::vector<Token> tokenize(CodeEditor::Language language, std::u32string_view line, bool& inComment) {
	std::vector<Token> tokens(line.size(), Token::Plain);
	switch(language) {
	case CodeEditor::Language::Plain:
	case CodeEditor::Language::Python: // coloured by its grammar
		break;
	case CodeEditor::Language::Cpp:
		markCpp(line, tokens, inComment);
		break;
	case CodeEditor::Language::Json:
		for(std::size_t i = 0; i < line.size();) i = markJsonToken(line, i, tokens);
		break;
	case CodeEditor::Language::CMake:
		for(std::size_t i = 0; i < line.size();) i = markCMakeToken(line, i, tokens);
		break;
	case CodeEditor::Language::Markdown:
		markMarkdown(line, tokens);
		break;
	}
	return tokens;
}

} // namespace examples::syntax
