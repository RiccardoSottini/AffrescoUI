#include "CodeEditor.hpp"

#include <algorithm>
#include <cctype>
#include <memory>
#include <stdexcept>
#include <string>

namespace examples {

using affresco::Color;
using affresco::Key;

namespace {

/// @brief Whether a code point is part of a word (letters, digits, underscore).
/// @param c The code point.
bool isWord(char32_t c) noexcept {
	return (c >= U'a' && c <= U'z') || (c >= U'A' && c <= U'Z') || (c >= U'0' && c <= U'9') || c == U'_' ||
	       (c >= 0xC0 && c <= 0x24F && c != 0xD7 && c != 0xF7);
}

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
constexpr std::string_view tokenNames[] = {"plain", "keyword", "type", "string", "number", "comment", "heading"};

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
	invalidate();
}

void CodeEditor::setTokenColor(Token token, Color color) {
	tokenColors_[static_cast<std::size_t>(token)] = color;
	invalidate();
}

void CodeEditor::resetTokenColors() {
	tokenColors_ = {Color{},
	                Color::hex(0xc49bff),
	                Color::hex(0x5fd0dd),
	                Color::hex(0xe6b071),
	                Color::hex(0xb5cea8),
	                Color::hex(0x6f8f6b),
	                Color::hex(0x7aa7ff)};
	invalidate();
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

void CodeEditor::toggleComment() {
	if(language_ != Language::Cpp && language_ != Language::CMake) return; // no line comments in JSON or Markdown
	const std::u32string_view marker = language_ == Language::CMake ? U"# " : U"// ";
	const std::u32string_view bare = marker.substr(0, marker.size() - 1);
	const auto [first, last] = selectedLines();

	// Comment out unless every non-blank line already is; the markers line up at the smallest indentation
	bool allCommented = true;
	std::size_t column = std::u32string::npos;
	for(std::size_t line = first; line <= last; ++line) {
		const std::u32string& text = lines()[line];
		const std::size_t indent = indentOf(line);
		if(indent == text.size()) continue;
		column = std::min(column, indent);
		if(text.compare(indent, bare.size(), bare) != 0) allCommented = false;
	}
	if(column == std::u32string::npos) return;

	beginEdit(Edit::Other);
	Position caret = this->caret(), anchor = this->anchor();
	for(std::size_t line = first; line <= last; ++line) {
		const std::u32string& text = lines()[line];
		const std::size_t indent = indentOf(line);
		if(indent == text.size()) continue;
		const std::size_t at = allCommented ? indent : column;
		long change = 0;
		if(allCommented) {
			const std::size_t length = text.compare(indent, marker.size(), marker) == 0 ? marker.size() : bare.size();
			erase({line, indent}, {line, indent + length});
			change = -static_cast<long>(length);
		} else {
			insert({line, column}, marker);
			change = static_cast<long>(marker.size());
		}
		for(Position* p : {&caret, &anchor})
			if(p->line == line && p->column >= at)
				p->column =
				    static_cast<std::size_t>(std::max(static_cast<long>(at), static_cast<long>(p->column) + change));
	}
	setSelection(anchor, caret);
	endEdit();
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
	bool handled = true;

	if(event.key == Key::Up && m.alt()) moveLines(false);
	else if(event.key == Key::Down && m.alt()) moveLines(true);
	else if(command && event.key == Key::Slash) toggleComment();
	else if(command && m.shift() && event.key == Key::K) deleteLines();
	else if(command && (event.key == Key::LeftBracket || event.key == Key::RightBracket))
		indentLines(event.key == Key::LeftBracket);
	else if(command && (event.key == Key::C || event.key == Key::X) && !hasSelection() && window()) {
		// No selection: the whole line, with its line break
		window()->setClipboardText(utf8(lines()[caret().line]) + "\n");
		if(event.key == Key::X) deleteLines();
	} else if(event.key == Key::Backspace && !hasSelection() && !command && !m.alt()) {
		// Inside an empty pair, e.g. "(|)": both go
		const Position c = caret();
		const std::u32string& text = lines()[c.line];
		handled = c.column > 0 && c.column < text.size() && closerOf(text[c.column - 1]) == text[c.column];
		if(handled) {
			beginEdit(Edit::Deleting);
			erase({c.line, c.column - 1}, {c.line, c.column + 1});
			setSelection({c.line, c.column - 1}, {c.line, c.column - 1});
			endEdit();
		}
	} else {
		handled = false;
	}

	if(handled) Panel::keyEvent(event); // keep the signals firing
	else TextEditor::keyEvent(event);
}

void CodeEditor::charEvent(const affresco::CharEvent& event) {
	const char32_t c = event.codepoint;
	const Position at = caret();
	const std::u32string& text = lines()[at.line];
	const char32_t next = at.column < text.size() ? text[at.column] : 0;
	const char32_t previous = at.column > 0 ? text[at.column - 1] : 0;
	const char32_t closer = closerOf(c);

	if(!hasSelection() && (c == U')' || c == U']' || c == U'}' || c == U'"') && next == c) {
		Panel::charEvent(event);
		moveCaret({at.line, at.column + 1}, false); // typing a closer that is already there steps over it
		return;
	}
	if(closer && hasSelection()) {
		// Wrap the selection in the pair
		Panel::charEvent(event);
		const auto [from, to] = selection();
		const std::u32string inner = textIn(from, to);
		beginEdit(Edit::Other);
		erase(from, to);
		const Position start = insert(from, std::u32string(1, c));
		const Position end = insert(start, inner);
		insert(end, std::u32string(1, closer));
		setSelection(start, end);
		endEdit();
		return;
	}
	const bool closes =
	    closer &&
	    (next == 0 || isBlank(next) || next == U')' || next == U']' || next == U'}' || next == U',' || next == U';') &&
	    !(c == U'"' && isWord(previous));
	if(closes) {
		Panel::charEvent(event);
		beginEdit(Edit::Typing);
		replaceSelection(std::u32string{c, closer});
		const Position inside{at.line, at.column + 1};
		setSelection(inside, inside);
		endEdit();
		return;
	}

	TextEditor::charEvent(event);
	// A closing brace typed on a blank line goes back one level
	const Position after = caret();
	if(c == U'}' && indentOf(after.line) + 1 == after.column && after.column > tabSize) {
		beginEdit(Edit::Typing);
		erase({after.line, 0}, {after.line, tabSize});
		const Position moved{after.line, after.column - tabSize};
		setSelection(moved, moved);
		endEdit();
	}
}

/* Syntax */

std::vector<CodeEditor::Token> CodeEditor::tokenize(std::u32string_view line, bool& inComment) const {
	std::vector<Token> tokens(line.size(), Token::Plain);
	const std::size_t n = line.size();
	auto fill = [&](std::size_t from, std::size_t to, Token token) {
		std::fill(tokens.begin() + static_cast<std::ptrdiff_t>(from),
		          tokens.begin() + static_cast<std::ptrdiff_t>(std::min(to, n)), token);
	};
	// A quoted string from an opening quote: returns the index after the closing one
	auto quoted = [&](std::size_t i) {
		const char32_t quote = line[i];
		std::size_t j = i + 1;
		while(j < n && line[j] != quote) j += line[j] == U'\\' ? 2u : 1u;
		return std::min(j + 1, n);
	};
	auto number = [&](std::size_t i) {
		std::size_t j = i;
		while(j < n && (isWord(line[j]) || line[j] == U'.' || line[j] == U'\'')) ++j;
		return j;
	};

	switch(language_) {
	case Language::Plain:
		break;

	case Language::Cpp: {
		std::size_t i = 0;
		if(inComment) {
			const std::size_t end = line.find(U"*/");
			i = end == std::u32string_view::npos ? n : end + 2;
			fill(0, i, Token::Comment);
			if(end != std::u32string_view::npos) inComment = false;
		}
		const std::size_t firstChar = line.find_first_not_of(U' ');
		if(i == 0 && firstChar != std::u32string_view::npos && line[firstChar] == U'#') {
			// A preprocessor line: the directive, then <header> or "header" as a string
			std::size_t j = firstChar + 1;
			while(j < n && isWord(line[j])) ++j;
			fill(firstChar, j, Token::Keyword);
			i = j;
			const std::size_t open = line.find_first_of(U"<\"", i);
			if(open != std::u32string_view::npos) {
				const std::size_t close = line.find(line[open] == U'<' ? U'>' : U'"', open + 1);
				fill(open, close == std::u32string_view::npos ? n : close + 1, Token::String);
				i = close == std::u32string_view::npos ? n : close + 1;
			}
		}
		while(i < n) {
			const char32_t c = line[i];
			if(c == U'/' && i + 1 < n && line[i + 1] == U'/') {
				fill(i, n, Token::Comment);
				break;
			}
			if(c == U'/' && i + 1 < n && line[i + 1] == U'*') {
				const std::size_t end = line.find(U"*/", i + 2);
				const std::size_t stop = end == std::u32string_view::npos ? n : end + 2;
				fill(i, stop, Token::Comment);
				if(end == std::u32string_view::npos) inComment = true;
				i = stop;
			} else if(c == U'R' && i + 2 < n && line[i + 1] == U'"' && line[i + 2] == U'(') {
				const std::size_t end = line.find(U")\"", i + 3);
				const std::size_t stop = end == std::u32string_view::npos ? n : end + 2;
				fill(i, stop, Token::String);
				i = stop;
			} else if(c == U'"' || c == U'\'') {
				const std::size_t stop = quoted(i);
				fill(i, stop, Token::String);
				i = stop;
			} else if(c >= U'0' && c <= U'9') {
				const std::size_t stop = number(i);
				fill(i, stop, Token::Number);
				i = stop;
			} else if(isWord(c)) {
				std::size_t j = i;
				while(j < n && isWord(line[j])) ++j;
				const std::u32string_view w = line.substr(i, j - i);
				if(contains(cppKeywords, w)) fill(i, j, Token::Keyword);
				else if(contains(cppTypes, w) || (w.front() >= U'A' && w.front() <= U'Z') || w == U"std")
					fill(i, j, Token::Type);
				i = j;
			} else {
				++i;
			}
		}
		break;
	}

	case Language::Json: {
		std::size_t i = 0;
		while(i < n) {
			const char32_t c = line[i];
			if(c == U'"') {
				const std::size_t stop = quoted(i);
				std::size_t k = stop;
				while(k < n && isBlank(line[k])) ++k;
				fill(i, stop, k < n && line[k] == U':' ? Token::Type : Token::String); // keys and values differ
				i = stop;
			} else if((c >= U'0' && c <= U'9') || c == U'-') {
				const std::size_t stop = number(i + 1);
				fill(i, stop, Token::Number);
				i = stop;
			} else if(isWord(c)) {
				std::size_t j = i;
				while(j < n && isWord(line[j])) ++j;
				const std::u32string_view w = line.substr(i, j - i);
				if(w == U"true" || w == U"false" || w == U"null") fill(i, j, Token::Keyword);
				i = j;
			} else {
				++i;
			}
		}
		break;
	}

	case Language::CMake: {
		std::size_t i = 0;
		while(i < n) {
			const char32_t c = line[i];
			if(c == U'#') {
				fill(i, n, Token::Comment);
				break;
			}
			if(c == U'"') {
				const std::size_t stop = quoted(i);
				fill(i, stop, Token::String);
				i = stop;
			} else if(c == U'$' && i + 1 < n && line[i + 1] == U'{') {
				const std::size_t end = line.find(U'}', i);
				const std::size_t stop = end == std::u32string_view::npos ? n : end + 1;
				fill(i, stop, Token::Type);
				i = stop;
			} else if(c >= U'0' && c <= U'9') {
				const std::size_t stop = number(i);
				fill(i, stop, Token::Number);
				i = stop;
			} else if(isWord(c)) {
				std::size_t j = i;
				while(j < n && (isWord(line[j]) || line[j] == U':')) ++j;
				const std::u32string_view w = line.substr(i, j - i);
				std::size_t k = j;
				while(k < n && isBlank(line[k])) ++k;
				if(k < n && line[k] == U'(') fill(i, j, Token::Keyword); // a command
				else if(std::ranges::all_of(w, [](char32_t ch) { return !(ch >= U'a' && ch <= U'z'); }))
					fill(i, j, Token::Type); // an option such as PRIVATE
				i = j;
			} else {
				++i;
			}
		}
		break;
	}

	case Language::Markdown: {
		const std::size_t firstChar = line.find_first_not_of(U' ');
		if(firstChar == std::u32string_view::npos) break;
		if(firstChar >= 4) {
			fill(0, n, Token::String); // an indented code block
			break;
		}
		if(line[firstChar] == U'#') {
			fill(0, n, Token::Heading);
			break;
		}
		if((line[firstChar] == U'-' || line[firstChar] == U'*') && firstChar + 1 < n && line[firstChar + 1] == U' ')
			fill(firstChar, firstChar + 1, Token::Keyword);
		for(std::size_t i = firstChar; i < n; ++i) {
			if(line[i] != U'`') continue;
			const std::size_t end = line.find(U'`', i + 1);
			const std::size_t stop = end == std::u32string_view::npos ? n : end + 1;
			fill(i, stop, Token::String);
			i = stop - 1;
		}
		break;
	}
	}
	return tokens;
}

/* Registration */

void registerCodeEditor(affresco::Screen& screen) {
	screen.registerType(
	    "code", [] { return std::make_unique<CodeEditor>(); }, {"language", "syntax"},
	    [&screen](affresco::Panel& panel, const std::string& propertiesJson) {
		    auto& editor = static_cast<CodeEditor&>(panel);
		    const affresco::json properties = affresco::json::parse(propertiesJson);
		    editor.setLanguage(properties.contains("language") ? properties["language"].get<std::string>() : "");
		    editor.resetTokenColors();
		    if(!properties.contains("syntax")) return;
		    for(const auto& [name, value] : properties["syntax"].items()) {
			    const auto it = std::ranges::find(tokenNames, name);
			    if(it == std::end(tokenNames) || it == std::begin(tokenNames) || !value.is_string())
				    throw std::invalid_argument(
				        "syntax." + name +
				        ": expected keyword, type, string, number, comment or heading, with a colour");
			    editor.setTokenColor(static_cast<CodeEditor::Token>(it - std::begin(tokenNames)),
				                     screen.resolveColor(value.get<std::string>()));
		    }
	    });
}

} // namespace examples
