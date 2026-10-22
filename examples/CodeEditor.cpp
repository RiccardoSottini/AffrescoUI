/// @file
/// @brief CodeEditor: syntax colours for C++, JSON, CMake and Markdown, and the commands of common code editors, on
/// the library's TextEditor; registerCodeEditor() adds it to a screen as the "code" type.

#include "CodeEditor.hpp"

#include <algorithm>
#include <cctype>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace examples {

using affresco::Color;
using affresco::Key;

namespace {

using Token = CodeEditor::Token;
using Position = CodeEditor::Position;

/// @brief Whether a code point is part of a word (letters, digits, underscore).
/// @param c The code point.
bool isWord(char32_t c) noexcept {
	return (c >= U'a' && c <= U'z') || (c >= U'A' && c <= U'Z') || (c >= U'0' && c <= U'9') || c == U'_' ||
	       (c >= 0xC0 && c <= 0x24F && c != 0xD7 && c != 0xF7);
}

/// @brief Whether a code point is a decimal digit.
/// @param c The code point.
bool isDigit(char32_t c) noexcept { return c >= U'0' && c <= U'9'; }

/// @brief Whether a code point is a space or a tab.
/// @param c The code point.
bool isBlank(char32_t c) noexcept { return c == U' ' || c == U'\t'; }

/// @brief The closing partner of an opening bracket or quote.
/// @param c The code point.
/// @return The partner, or 0 if @p c doesn't open a pair.
char32_t closerOf(char32_t c) noexcept {
	switch(c) {
	case U'(':
		return U')';
	case U'[':
		return U']';
	case U'{':
		return U'}';
	case U'"':
		return U'"';
	default:
		return 0;
	}
}

/// @brief Whether a code point closes a pair: a closing bracket or a double quote.
/// @param c The code point.
bool isCloser(char32_t c) noexcept { return c == U')' || c == U']' || c == U'}' || c == U'"'; }

/// @brief Whether a pair typed just before this code point closes itself: the line's end, a blank, a closing bracket,
/// a comma or a semicolon (in front of a word, an opener is just typed).
/// @param next The code point after the caret, 0 at the line's end.
bool isPairBoundary(char32_t next) noexcept {
	return next == 0 || isBlank(next) || next == U')' || next == U']' || next == U'}' || next == U',' || next == U';';
}

/// @brief Encodes text as UTF-8.
/// @param text Code points.
std::string utf8(std::u32string_view text) {
	std::string out;
	for(const char32_t c : text) {
		if(c < 0x80) out.push_back(static_cast<char>(c));
		else if(c < 0x800) {
			out.push_back(static_cast<char>(0xC0 | (c >> 6)));
			out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
		} else if(c < 0x10000) {
			out.push_back(static_cast<char>(0xE0 | (c >> 12)));
			out.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
			out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
		} else {
			out.push_back(static_cast<char>(0xF0 | (c >> 18)));
			out.push_back(static_cast<char>(0x80 | ((c >> 12) & 0x3F)));
			out.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
			out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
		}
	}
	return out;
}

/// @brief Whether a word is in a sorted list.
/// @param list The list, sorted.
/// @param word The word.
template <std::size_t N> bool contains(const std::array<std::u32string_view, N>& list, std::u32string_view word) {
	return std::ranges::binary_search(list, word);
}

constexpr std::array<std::u32string_view, 55> cppKeywords = {
    U"alignas",   U"alignof",  U"auto",     U"break",    U"case",      U"catch",     U"class",     U"co_await",
    U"co_return", U"co_yield", U"concept",  U"const",    U"consteval", U"constexpr", U"constinit", U"continue",
    U"decltype",  U"default",  U"delete",   U"do",       U"else",      U"enum",      U"explicit",  U"export",
    U"extern",    U"false",    U"final",    U"for",      U"friend",    U"if",        U"inline",    U"mutable",
    U"namespace", U"new",      U"noexcept", U"nullptr",  U"operator",  U"override",  U"private",   U"protected",
    U"public",    U"requires", U"return",   U"static",   U"struct",    U"switch",    U"template",  U"this",
    U"throw",     U"true",     U"try",      U"typename", U"using",     U"virtual",   U"while"}; ///< C++ keywords,
	                                                                                            ///< sorted.
static_assert(std::ranges::is_sorted(cppKeywords));

constexpr std::array<std::u32string_view, 11> cppTypes = {
    U"bool",  U"char",   U"double", U"float",    U"int", U"long",
    U"short", U"signed", U"size_t", U"unsigned", U"void"}; ///< Built-in types, sorted.
static_assert(std::ranges::is_sorted(cppTypes));

/// Names of the kinds of text in "syntax", indexed by CodeEditor::Token ("plain" is the editor's "textColor").
constexpr std::array<std::string_view, 7> tokenNames = {"plain",  "keyword", "type",   "string",
                                                        "number", "comment", "heading"};

/* Scanning a line: each function marks what it reads in `tokens` (one per code point) and returns where it stopped */

/// @brief Marks a part of a line as one kind of text.
/// @param tokens The line's tokens.
/// @param from Its first code point.
/// @param to Just after its last one (past the line's end means up to the end).
/// @param token The kind.
/// @return @p to: where the next part starts.
std::size_t mark(std::vector<Token>& tokens, std::size_t from, std::size_t to, Token token) {
	std::fill(tokens.begin() + static_cast<std::ptrdiff_t>(from),
	          tokens.begin() + static_cast<std::ptrdiff_t>(std::min(to, tokens.size())), token);
	return to;
}

/// @brief The end of a quoted string (a backslash escapes the next code point).
/// @param line The line.
/// @param i Its opening quote.
/// @return The index after the closing quote, or the line's end.
std::size_t endOfQuoted(std::u32string_view line, std::size_t i) {
	const char32_t quote = line[i];
	std::size_t j = i + 1;
	while(j < line.size() && line[j] != quote) j += line[j] == U'\\' ? 2u : 1u;
	return std::min(j + 1, line.size());
}

/// @brief The end of a number: word characters, dots and digit separators.
/// @param line The line.
/// @param i Where it starts.
/// @return The index after it.
std::size_t endOfNumber(std::u32string_view line, std::size_t i) {
	while(i < line.size() && (isWord(line[i]) || line[i] == U'.' || line[i] == U'\'')) ++i;
	return i;
}

/// @brief The end of a word.
/// @param line The line.
/// @param i Where it starts.
/// @return The index after it.
std::size_t endOfWord(std::u32string_view line, std::size_t i) {
	while(i < line.size() && isWord(line[i])) ++i;
	return i;
}

/// @brief Skips spaces and tabs.
/// @param line The line.
/// @param i Where to start.
/// @return The first index that isn't blank, or the line's end.
std::size_t skipBlanks(std::u32string_view line, std::size_t i) {
	while(i < line.size() && isBlank(line[i])) ++i;
	return i;
}

/// @brief The end of a part that ends with a closing text, such as a block comment's "*/".
/// @param line The line.
/// @param closer The closing text.
/// @param from Where to look for it.
/// @return The index after it, or the line's end if the line doesn't have it.
std::size_t endAfter(std::u32string_view line, std::u32string_view closer, std::size_t from) {
	const std::size_t end = line.find(closer, from);
	return end == std::u32string_view::npos ? line.size() : end + closer.size();
}

/// @brief Marks a C++ block comment, which may go on past the line.
/// @param line The line.
/// @param i Its "/*".
/// @param tokens The line's tokens.
/// @param inComment Set to true if it doesn't end on this line.
/// @return Where the next part starts.
std::size_t markBlockComment(std::u32string_view line, std::size_t i, std::vector<Token>& tokens, bool& inComment) {
	const std::size_t end = line.find(U"*/", i + 2);
	if(end == std::u32string_view::npos) inComment = true;
	return mark(tokens, i, end == std::u32string_view::npos ? line.size() : end + 2, Token::Comment);
}

/// @brief Marks a preprocessor line's directive, then a <header> or "header" after it as a string.
/// @param line The line.
/// @param tokens The line's tokens.
/// @return Where the rest of the line starts: 0 if it isn't a preprocessor line.
std::size_t markPreprocessor(std::u32string_view line, std::vector<Token>& tokens) {
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
std::size_t markCppLineStart(std::u32string_view line, std::vector<Token>& tokens, bool& inComment) {
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
std::size_t markCppWord(std::u32string_view line, std::size_t i, std::vector<Token>& tokens) {
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
std::size_t markCppToken(std::u32string_view line, std::size_t i, std::vector<Token>& tokens, bool& inComment) {
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
void markCpp(std::u32string_view line, std::vector<Token>& tokens, bool& inComment) {
	std::size_t i = markCppLineStart(line, tokens, inComment);
	while(i < line.size()) i = markCppToken(line, i, tokens, inComment);
}

/// @brief Marks the JSON token at an index: a key, a string, a number or a literal.
/// @param line The line.
/// @param i The index.
/// @param tokens The line's tokens.
/// @return Where the next token starts.
std::size_t markJsonToken(std::u32string_view line, std::size_t i, std::vector<Token>& tokens) {
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
std::size_t markCMakeWord(std::u32string_view line, std::size_t i, std::vector<Token>& tokens) {
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
std::size_t markCMakeToken(std::u32string_view line, std::size_t i, std::vector<Token>& tokens) {
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
void markMarkdown(std::u32string_view line, std::vector<Token>& tokens) {
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

/// @brief Moves a selection end on a line by the columns that the text after a change moved, never to before the
/// change.
/// @param position The selection end.
/// @param line The line that changed.
/// @param at Where the change happened.
/// @param change How many columns the text after it moved (negative to the left).
void shiftColumn(Position& position, std::size_t line, std::size_t at, long change) {
	if(position.line != line || position.column < at) return;
	position.column =
	    static_cast<std::size_t>(std::max(static_cast<long>(at), static_cast<long>(position.column) + change));
}

} // namespace

CodeEditor::CodeEditor() { resetTokenColors(); }

void CodeEditor::setLanguage(std::string_view name) {
	std::string lower(name);
	std::ranges::transform(lower, lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	const Language language = lower == "c++" || lower == "cpp" || lower == "c" ? Language::Cpp
	                          : lower == "json"                                ? Language::Json
	                          : lower == "cmake"                               ? Language::CMake
	                          : lower == "markdown" || lower == "md"           ? Language::Markdown
	                                                                           : Language::Plain;
	if(language == language_) return;
	language_ = language;
	recolor();
}

void CodeEditor::setTokenColor(Token token, Color color) {
	tokenColors_[static_cast<std::size_t>(token)] = color;
	recolor();
}

void CodeEditor::resetTokenColors() {
	tokenColors_ = {Color{},
	                Color::hex(0xc49bff),
	                Color::hex(0x5fd0dd),
	                Color::hex(0xe6b071),
	                Color::hex(0xb5cea8),
	                Color::hex(0x6f8f6b),
	                Color::hex(0x7aa7ff)};
	recolor();
}

/* Colouring */

void CodeEditor::colorLine(std::u32string_view line, int& state, std::vector<Color>& colors) const {
	bool inComment = state != 0;
	const std::vector<Token> tokens = tokenize(line, inComment);
	state = inComment ? 1 : 0;
	for(std::size_t i = 0; i < line.size(); ++i)
		colors[i] = tokens[i] == Token::Plain ? textColor() : tokenColors_[static_cast<std::size_t>(tokens[i])];
}

/* Commands */

void CodeEditor::newLine() {
	const auto [from, to] = selection();
	const std::u32string& text = lines()[from.line];
	const char32_t before = from.column > 0 ? text[from.column - 1] : 0;
	const bool opens = before == U'{' || before == U'[' || before == U'(' || before == U':';
	if(!opens) return TextEditor::newLine();

	const char32_t after = to.line == from.line && to.column < text.size() ? text[to.column] : 0;
	const std::size_t indent = std::min(indentOf(from.line), from.column);
	beginEdit(Edit::Other);
	replaceSelection(U"\n" + std::u32string(indent + tabSize, U' '));
	const Position inside = caret();
	if(after != 0 && after == closerOf(before)) {
		// Between a pair of brackets: the closing one goes on its own line, the caret stays on the indented one
		insert(inside, U"\n" + std::u32string(indent, U' '));
		setSelection(inside, inside);
	}
	endEdit();
}

// Comment out unless every non-blank line already is; the markers line up at the smallest indentation
void CodeEditor::toggleComment() {
	if(language_ != Language::Cpp && language_ != Language::CMake) return; // no line comments in JSON or Markdown
	const std::u32string_view marker = language_ == Language::CMake ? U"# " : U"// ";
	const auto [first, last] = selectedLines();
	const std::optional<std::size_t> column = commentColumn(first, last);
	if(!column) return;
	const bool uncomment = linesCommented(first, last, marker.substr(0, marker.size() - 1));

	beginEdit(Edit::Other);
	Position caret = this->caret(), anchor = this->anchor();
	for(std::size_t line = first; line <= last; ++line) {
		const std::size_t indent = indentOf(line);
		if(indent == lines()[line].size()) continue;
		const std::size_t at = uncomment ? indent : *column;
		const long change = toggleLineComment(line, at, uncomment, marker);
		shiftColumn(caret, line, at, change);
		shiftColumn(anchor, line, at, change);
	}
	setSelection(anchor, caret);
	endEdit();
}

std::optional<std::size_t> CodeEditor::commentColumn(std::size_t first, std::size_t last) const {
	std::optional<std::size_t> column;
	for(std::size_t line = first; line <= last; ++line)
		if(const std::size_t indent = indentOf(line); indent != lines()[line].size())
			column = std::min(column.value_or(indent), indent);
	return column;
}

bool CodeEditor::linesCommented(std::size_t first, std::size_t last, std::u32string_view bare) const {
	for(std::size_t line = first; line <= last; ++line) {
		const std::u32string& text = lines()[line];
		const std::size_t indent = indentOf(line);
		if(indent != text.size() && text.compare(indent, bare.size(), bare) != 0) return false;
	}
	return true;
}

long CodeEditor::toggleLineComment(std::size_t line, std::size_t column, bool uncomment, std::u32string_view marker) {
	if(!uncomment) {
		insert({line, column}, marker);
		return static_cast<long>(marker.size());
	}
	// The marker's space goes too, if it has one
	const std::size_t length =
	    lines()[line].compare(column, marker.size(), marker) == 0 ? marker.size() : marker.size() - 1;
	erase({line, column}, {line, column + length});
	return -static_cast<long>(length);
}

void CodeEditor::moveLines(bool down) {
	const auto [first, last] = selectedLines();
	if(down ? last + 1 >= lines().size() : first == 0) return;
	beginEdit(Edit::Other);
	if(down) {
		const std::u32string below = lines()[last + 1];
		erase({last, lines()[last].size()}, {last + 1, below.size()});
		insert({first, 0}, below + U"\n");
	} else {
		const std::u32string above = lines()[first - 1];
		erase({first - 1, 0}, {first, 0});
		insert({last - 1, lines()[last - 1].size()}, U"\n" + above);
	}
	Position caret = this->caret(), anchor = this->anchor();
	for(Position* p : {&caret, &anchor}) p->line = down ? p->line + 1 : p->line - 1;
	setSelection(anchor, caret);
	endEdit();
}

void CodeEditor::deleteLines() {
	const auto [first, last] = selectedLines();
	const std::size_t column = caret().column;
	beginEdit(Edit::Other);
	if(last + 1 < lines().size()) erase({first, 0}, {last + 1, 0});
	else if(first > 0) erase({first - 1, lines()[first - 1].size()}, {last, lines()[last].size()});
	else erase({first, 0}, {last, lines()[last].size()});
	const std::size_t line = std::min(first, lines().size() - 1);
	const Position caret{line, std::min(column, lines()[line].size())};
	setSelection(caret, caret);
	endEdit();
}

/* Events */

void CodeEditor::keyEvent(const affresco::KeyEvent& event) {
	if(event.action == affresco::Action::Release) return TextEditor::keyEvent(event);
	const affresco::Mods m = event.mods;
	const bool command = m.super() || m.control();
	const bool handled =
	    runShortcut(event) || (event.key == Key::Backspace && !command && !m.alt() && deleteEmptyPair());
	// NOLINTNEXTLINE(bugprone-parent-virtual-call): skips TextEditor's handling, and keeps the signals firing
	if(handled) Panel::keyEvent(event);
	else TextEditor::keyEvent(event);
}

bool CodeEditor::runShortcut(const affresco::KeyEvent& event) {
	const affresco::Mods m = event.mods;
	if(m.alt() && (event.key == Key::Up || event.key == Key::Down)) {
		moveLines(event.key == Key::Down);
		return true;
	}
	return (m.super() || m.control()) && runCommandShortcut(event.key, m.shift());
}

bool CodeEditor::runCommandShortcut(Key key, bool shift) {
	switch(key) {
	case Key::Slash:
		toggleComment();
		return true;
	case Key::K:
		if(shift) deleteLines();
		return shift;
	case Key::LeftBracket:
	case Key::RightBracket:
		indentLines(key == Key::LeftBracket);
		return true;
	case Key::C:
	case Key::X:
		return copyLine(key == Key::X);
	default:
		return false;
	}
}

bool CodeEditor::copyLine(bool cut) {
	if(hasSelection() || !window()) return false;
	window()->setClipboardText(utf8(lines()[caret().line]) + "\n");
	if(cut) deleteLines();
	return true;
}

bool CodeEditor::deleteEmptyPair() {
	const Position c = caret();
	const std::u32string& text = lines()[c.line];
	if(hasSelection() || c.column == 0 || c.column >= text.size() || closerOf(text[c.column - 1]) != text[c.column])
		return false;
	beginEdit(Edit::Deleting);
	erase({c.line, c.column - 1}, {c.line, c.column + 1});
	setSelection({c.line, c.column - 1}, {c.line, c.column - 1});
	endEdit();
	return true;
}

void CodeEditor::charEvent(const affresco::CharEvent& event) {
	const char32_t c = event.codepoint;
	const PairTyping typing = pairTypingFor(c);
	if(typing == PairTyping::None) {
		TextEditor::charEvent(event);
		outdentClosingBrace(c);
		return;
	}

	const Position at = caret();
	// NOLINTNEXTLINE(bugprone-parent-virtual-call): the editor types the pair itself, and the signals still fire
	Panel::charEvent(event);
	switch(typing) {
	case PairTyping::StepOver:
		moveCaret({at.line, at.column + 1}, false);
		break;
	case PairTyping::Wrap:
		wrapSelection(c);
		break;
	case PairTyping::Close:
		closePair(c, at);
		break;
	case PairTyping::None:
		break;
	}
}

CodeEditor::PairTyping CodeEditor::pairTypingFor(char32_t typed) const {
	const Position at = caret();
	const std::u32string& text = lines()[at.line];
	const char32_t next = at.column < text.size() ? text[at.column] : 0;
	const char32_t previous = at.column > 0 ? text[at.column - 1] : 0;
	const bool opener = closerOf(typed) != 0;
	if(!hasSelection() && isCloser(typed) && next == typed) return PairTyping::StepOver;
	if(opener && hasSelection()) return PairTyping::Wrap;
	// A double quote right after a word most likely ends a string: it is typed alone
	if(opener && isPairBoundary(next) && (typed != U'"' || !isWord(previous))) return PairTyping::Close;
	return PairTyping::None;
}

void CodeEditor::wrapSelection(char32_t opener) {
	const auto [from, to] = selection();
	const std::u32string inner = textIn(from, to);
	beginEdit(Edit::Other);
	erase(from, to);
	const Position start = insert(from, std::u32string(1, opener));
	const Position end = insert(start, inner);
	insert(end, std::u32string(1, closerOf(opener)));
	setSelection(start, end);
	endEdit();
}

void CodeEditor::closePair(char32_t opener, Position at) {
	beginEdit(Edit::Typing);
	replaceSelection(std::u32string{opener, closerOf(opener)});
	const Position inside{at.line, at.column + 1};
	setSelection(inside, inside);
	endEdit();
}

void CodeEditor::outdentClosingBrace(char32_t typed) {
	const Position after = caret();
	if(typed != U'}' || indentOf(after.line) + 1 != after.column || after.column <= tabSize) return;
	beginEdit(Edit::Typing);
	erase({after.line, 0}, {after.line, tabSize});
	const Position moved{after.line, after.column - tabSize};
	setSelection(moved, moved);
	endEdit();
}

/* Syntax */

std::vector<CodeEditor::Token> CodeEditor::tokenize(std::u32string_view line, bool& inComment) const {
	std::vector<Token> tokens(line.size(), Token::Plain);
	switch(language_) {
	case Language::Plain:
		break;
	case Language::Cpp:
		markCpp(line, tokens, inComment);
		break;
	case Language::Json:
		for(std::size_t i = 0; i < line.size();) i = markJsonToken(line, i, tokens);
		break;
	case Language::CMake:
		for(std::size_t i = 0; i < line.size();) i = markCMakeToken(line, i, tokens);
		break;
	case Language::Markdown:
		markMarkdown(line, tokens);
		break;
	}
	return tokens;
}

void CodeEditor::goToLine(std::size_t line, long long request) {
	if(request == goToRequest_ || lines().size() == 0) return;
	goToRequest_ = request;
	moveCaret({std::min(line == 0 ? 0 : line - 1, lines().size() - 1), 0}, false);
}

/* Registration */

void registerCodeEditor(affresco::Screen& screen, BufferLookup buffers) {
	screen.registerType(
	    "code", [] { return std::make_unique<CodeEditor>(); }, {"language", "syntax", "goTo", "buffer"},
	    [&screen, buffers = std::move(buffers)](affresco::Panel& panel, const std::string& propertiesJson) {
		    auto& editor = dynamic_cast<CodeEditor&>(panel); // the factory above made it
		    const affresco::json properties = affresco::json::parse(propertiesJson);
		    // "buffer": the application's text for that id, shared with every editor showing it
		    if(const auto buffer = properties.find("buffer"); buffers && buffer != properties.end() && buffer->is_string())
			    if(auto text = buffers(buffer->get<std::string>()); text && text != editor.sharedModel())
				    editor.setModel(std::move(text));
		    editor.setLanguage(properties.contains("language") ? properties["language"].get<std::string>() : "");
		    editor.resetTokenColors();
		    // "goTo": { "line": 12, "request": 3 } moves the caret, once per request (Forge's go to line)
		    if(const auto goTo = properties.find("goTo"); goTo != properties.end() && goTo->is_object())
			    editor.goToLine(static_cast<std::size_t>(std::max(0.0, goTo->value("line", 1.0))),
			                    static_cast<long long>(goTo->value("request", 0.0)));
		    if(!properties.contains("syntax")) return;
		    for(const auto& [name, value] : properties["syntax"].items()) {
			    const auto token = static_cast<std::size_t>(std::ranges::find(tokenNames, name) - tokenNames.begin());
			    if(token == 0 || token == tokenNames.size() || !value.is_string()) // "plain" is "textColor"
				    throw std::invalid_argument(
				        "syntax." + name +
				        ": expected keyword, type, string, number, comment or heading, with a colour");
			    editor.setTokenColor(static_cast<CodeEditor::Token>(token),
				                     screen.resolveColor(value.get<std::string>()));
		    }
	    });
}

} // namespace examples
