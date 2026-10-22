/// @file
/// @brief CodeEditor: syntax colours (tree-sitter grammars, and CMake's own tokens), and the commands of common code editors, on
/// the library's TextEditor; registerCodeEditor() adds it to a screen as the "code" type.

#include "CodeEditor.hpp"
#include "CodeSyntax.hpp"

#include <affresco/format/SyntaxHighlighter.hpp>

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

using syntax::isBlank;
using syntax::isWord;

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

/// @brief The name a popup gets for one of its keys (CodeEditor::setPopupKeys()).
/// @param key The key.
/// @return The name, or "" for a key a popup doesn't take.
std::string_view popupKeyName(Key key) noexcept {
	switch(key) {
	case Key::Up:
		return "up";
	case Key::Down:
		return "down";
	case Key::PageUp:
		return "pageUp";
	case Key::PageDown:
		return "pageDown";
	case Key::Enter:
	case Key::KeypadEnter:
		return "enter";
	case Key::Tab:
		return "tab";
	case Key::Escape:
		return "escape";
	default:
		return {};
	}
}

/// Names of the kinds of text in "syntax", indexed by CodeEditor::Token ("plain" is the editor's "textColor").
constexpr std::array<std::string_view, 7> tokenNames = {"plain",  "keyword", "type",   "string",
                                                        "number", "comment", "heading"};

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

/// @brief The editor's language for a grammar: the C-like grammars are Language::Cpp, and CMake (which has no
/// grammar) goes by its name.
/// @param grammar The grammar's canonical name, "" if there is none.
/// @param name The language's name, in lower case.
CodeEditor::Language languageOf(std::string_view grammar, std::string_view name) noexcept {
	using Language = CodeEditor::Language;
	if(grammar == "c" || grammar == "cpp" || grammar == "javascript" || grammar == "typescript") return Language::Cpp;
	if(grammar == "json") return Language::Json;
	if(grammar == "python") return Language::Python;
	if(grammar == "markdown") return Language::Markdown;
	return name == "cmake" ? Language::CMake : Language::Plain;
}

/// @brief Whether an open popup takes a key: one of its keys (popupKeyName()) without modifiers, which it handled.
/// @param popupKeys The popup's handler (CodeEditor::setPopupKeys()), empty while there is no popup.
/// @param event A key press or repeat.
bool popupTakes(const std::function<bool(std::string_view)>& popupKeys, const affresco::KeyEvent& event) {
	const affresco::Mods m = event.mods;
	if(!popupKeys || m.super() || m.control() || m.alt() || m.shift()) return false;
	const std::string_view key = popupKeyName(event.key);
	return !key.empty() && popupKeys(key);
}

} // namespace

CodeEditor::CodeEditor() { resetTokenColors(); }

void CodeEditor::setLanguage(std::string_view name) {
	std::string lower(name);
	std::ranges::transform(lower, lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	// A grammar colours (and folds) where one exists; the language still picks the comment marker
	const std::string grammar = affresco::format::SyntaxHighlighter::canonical(lower);
	const Language language = languageOf(grammar, lower);
	const auto* current = dynamic_cast<affresco::format::SyntaxHighlighter*>(highlighter());
	if(language == language_ && (current ? current->language() : std::string()) == grammar) return;
	language_ = language;
	if(grammar.empty()) setHighlighter(nullptr);
	else {
		setHighlighter(affresco::format::SyntaxHighlighter::create(grammar));
		refreshPalette();
	}
	recolor();
}

void CodeEditor::setTokenColor(Token token, Color color) {
	tokenColors_[static_cast<std::size_t>(token)] = color;
	refreshPalette();
	recolor();
}

void CodeEditor::refreshPalette() {
	auto* grammar = dynamic_cast<affresco::format::SyntaxHighlighter*>(highlighter());
	if(!grammar) return;
	// The grammar's styles (a style without a token takes its parent's, e.g. "type.builtin" is "type")
	static constexpr std::array<std::pair<std::string_view, Token>, 10> tokens = {{{"keyword", Token::Keyword},
	                                                                               {"constant.builtin", Token::Keyword},
	                                                                               {"type", Token::Type},
	                                                                               {"constructor", Token::Type},
	                                                                               {"string", Token::String},
	                                                                               {"text.literal", Token::String},
	                                                                               {"number", Token::Number},
	                                                                               {"comment", Token::Comment},
	                                                                               {"text.title", Token::Heading},
	                                                                               {"punctuation.special", Token::Heading}}};
	grammar->setPalette([colors = tokenColors_](std::string_view style) -> std::optional<Color> {
		const auto found = std::ranges::find(tokens, style, &std::pair<std::string_view, Token>::first);
		if(found == tokens.end()) return std::nullopt;
		return colors[static_cast<std::size_t>(found->second)];
	});
}

void CodeEditor::resetTokenColors() {
	tokenColors_ = {Color{},
	                Color::hex(0xc49bff),
	                Color::hex(0x5fd0dd),
	                Color::hex(0xe6b071),
	                Color::hex(0xb5cea8),
	                Color::hex(0x6f8f6b),
	                Color::hex(0x7aa7ff)};
	refreshPalette();
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
	if(language_ == Language::Plain || language_ == Language::Json || language_ == Language::Markdown)
		return; // no line comments in JSON or Markdown
	const std::u32string_view marker = language_ == Language::CMake || language_ == Language::Python ? U"# " : U"// ";
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
	if(popupTakes(popupKeys_, event)) {
		// NOLINTNEXTLINE(bugprone-parent-virtual-call): the popup took the key, and the signals still fire
		Panel::keyEvent(event);
		return;
	}
	const std::uint64_t before = model().version();
	const bool handled =
	    runShortcut(event) || (event.key == Key::Backspace && !command && !m.alt() && deleteEmptyPair());
	// NOLINTNEXTLINE(bugprone-parent-virtual-call): skips TextEditor's handling, and keeps the signals firing
	if(handled) Panel::keyEvent(event);
	else TextEditor::keyEvent(event);
	if((event.key == Key::Backspace || event.key == Key::Delete) && model().version() != before) onTyped(std::string());
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
	const std::uint64_t before = model().version();
	typeCodepoint(event);
	if(model().version() != before) onTyped(utf8(std::u32string_view(&c, 1)));
}

void CodeEditor::typeCodepoint(const affresco::CharEvent& event) {
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
	return syntax::tokenize(language_, line, inComment);
}

void CodeEditor::goToLine(std::size_t line, long long request, std::size_t column) {
	if(request == goToRequest_ || lines().size() == 0) return;
	goToRequest_ = request;
	const std::size_t at = std::min(line == 0 ? 0 : line - 1, lines().size() - 1);
	moveCaret({at, std::min(column == 0 ? 0 : column - 1, lines()[at].size())}, false);
}

void CodeEditor::insertAtCaret(std::u32string_view text, std::size_t before, long long request) {
	if(request == insertRequest_ || readOnly()) return;
	insertRequest_ = request;
	beginEdit(Edit::Other);
	const auto [start, end] = selection();
	const Position from{start.line, start.column - std::min(before, start.column)};
	erase(from, end);
	const Position after = insert(from, text);
	setSelection(after, after);
	endEdit();
}

void CodeEditor::setOverlay(std::vector<affresco::format::SyntaxHighlighter::Overlay> runs) {
	auto* grammar = dynamic_cast<affresco::format::SyntaxHighlighter*>(highlighter());
	if(!grammar) return;
	grammar->setOverlay(std::move(runs));
	recolor();
}

std::u32string CodeEditor::wordBeforeCaret() const {
	const Position at = caret();
	const std::u32string& text = lines()[at.line];
	std::size_t start = std::min(at.column, text.size());
	while(start > 0 && isWord(text[start - 1])) --start;
	return text.substr(start, at.column - start);
}

/* Registration */

namespace {

/// @brief The "code" type's editor: a CodeEditor and the actions its properties name.
struct ScreenCodeEditor : CodeEditor {
	std::string hoverAction; ///< "onHover".
	std::string typeAction;  ///< "onType".
	affresco::json tokens = affresco::json::array(); ///< The "tokens" last given.
};

/// @brief The event of a place in the text, for "onHover" and "onType": its line and column from 1, and its box.
/// @param editor The editor.
/// @param at The place.
affresco::json caretEvent(const CodeEditor& editor, Position at) {
	affresco::json event{{"line", at.line + 1}, {"column", at.column + 1}};
	if(const std::optional<affresco::Rect> box = editor.rectAt(at)) {
		event["x"] = box->left;
		event["y"] = box->bottom;
		event["top"] = box->top;
	}
	return event;
}

/// @brief Makes a "code" node's editor, whose hovers and typing run the actions its properties name.
/// @param screen The screen that runs them.
std::unique_ptr<ScreenCodeEditor> makeScreenCodeEditor(affresco::Screen& screen) {
	auto editor = std::make_unique<ScreenCodeEditor>();
	ScreenCodeEditor& self = *editor;
	self.onHoverPosition.connect([&screen, &self](std::optional<Position> at) {
		if(self.hoverAction.empty()) return;
		affresco::json event = at ? caretEvent(self, *at) : affresco::json::object();
		event["over"] = at.has_value();
		screen.runAction(self.hoverAction, self, event.dump());
	});
	self.onTyped.connect([&screen, &self](const std::string& text) {
		if(self.typeAction.empty()) return;
		affresco::json event = caretEvent(self, self.caret());
		event["text"] = text;
		event["word"] = utf8(self.wordBeforeCaret());
		screen.runAction(self.typeAction, self, event.dump());
	});
	return editor;
}

/// @brief The action a property names.
/// @param properties The node's properties.
/// @param key The property.
/// @return Its text, or "" if it isn't a string.
std::string actionOf(const affresco::json& properties, const char* key) {
	const auto found = properties.find(key);
	return found != properties.end() && found->is_string() ? found->get<std::string>() : std::string();
}

/// @brief Applies "popup": while it is true, the popup's keys go to "onPopupKey".
/// @param screen The screen that runs the action.
/// @param editor The editor.
/// @param properties The node's properties.
void applyPopup(affresco::Screen& screen, ScreenCodeEditor& editor, const affresco::json& properties) {
	if(const std::string action = actionOf(properties, "onPopupKey");
	   properties.value("popup", false) && !action.empty())
		editor.setPopupKeys([&screen, &editor, action](std::string_view key) {
			screen.runAction(action, editor, affresco::json{{"key", key}}.dump());
			return true;
		});
	else editor.setPopupKeys({});
}

/// @brief Applies "insert": `{ "text", "before", "request" }` replaces the selection and the code points before the
/// caret with the text, once per request (a completion).
/// @param editor The editor.
/// @param properties The node's properties.
void applyInsert(ScreenCodeEditor& editor, const affresco::json& properties) {
	const auto insert = properties.find("insert");
	if(insert == properties.end() || !insert->is_object() || !(*insert)["text"].is_string()) return;
	editor.insertAtCaret(affresco::codepointsOf((*insert)["text"].get<std::string>()),
	                     static_cast<std::size_t>(std::max(0.0, insert->value("before", 0.0))),
	                     static_cast<long long>(insert->value("request", 0.0)));
}

/// @brief Applies "buffer": the application's text for that id, shared with every editor showing it.
/// @param editor The editor.
/// @param properties The node's properties.
/// @param buffers The application's texts by id, or an empty function.
void applyBuffer(ScreenCodeEditor& editor, const affresco::json& properties, const BufferLookup& buffers) {
	const auto buffer = properties.find("buffer");
	if(!buffers || buffer == properties.end() || !buffer->is_string()) return;
	if(auto text = buffers(buffer->get<std::string>()); text && text != editor.sharedModel())
		editor.setModel(std::move(text));
}

/// @brief The grammar overlay runs of a "tokens" value: `{ "line", "column", "length", "style" }` each; runs that
/// aren't objects with a style are skipped.
/// @param tokens The value.
std::vector<affresco::format::SyntaxHighlighter::Overlay> overlayRuns(const affresco::json& tokens) {
	std::vector<affresco::format::SyntaxHighlighter::Overlay> runs;
	for(const affresco::json& run : tokens.is_array() ? tokens : affresco::json::array()) {
		if(!run.is_object() || !run.value("style", affresco::json()).is_string()) continue;
		const auto whole = [&run](const char* key) {
			return static_cast<std::size_t>(std::max(0.0, run.value(key, 0.0)));
		};
		runs.push_back({whole("line"), whole("column"), whole("length"), run["style"].get<std::string>()});
	}
	return runs;
}

/// @brief Applies "tokens": runs over the grammar's colours, given again only when they change (edits move them
/// meanwhile).
/// @param editor The editor.
/// @param properties The node's properties.
void applyTokens(ScreenCodeEditor& editor, const affresco::json& properties) {
	affresco::json tokens = properties.value("tokens", affresco::json::array());
	if(tokens == editor.tokens) return;
	std::vector<affresco::format::SyntaxHighlighter::Overlay> runs = overlayRuns(tokens);
	editor.tokens = std::move(tokens);
	editor.setOverlay(std::move(runs));
}

/// @brief Applies "goTo": `{ "line": 12, "request": 3 }` moves the caret, once per request (Forge's go to line).
/// @param editor The editor.
/// @param properties The node's properties.
void applyGoTo(ScreenCodeEditor& editor, const affresco::json& properties) {
	const auto goTo = properties.find("goTo");
	if(goTo == properties.end() || !goTo->is_object()) return;
	editor.goToLine(static_cast<std::size_t>(std::max(0.0, goTo->value("line", 1.0))),
	                static_cast<long long>(goTo->value("request", 0.0)),
	                static_cast<std::size_t>(std::max(0.0, goTo->value("column", 1.0))));
}

/// @brief Applies "syntax": a colour for each kind of text.
/// @param screen The screen, which resolves the colours.
/// @param editor The editor.
/// @param properties The node's properties.
/// @throws std::invalid_argument For a kind it doesn't know, or a value that isn't a colour.
void applySyntax(const affresco::Screen& screen, ScreenCodeEditor& editor, const affresco::json& properties) {
	if(!properties.contains("syntax")) return;
	for(const auto& [name, value] : properties["syntax"].items()) {
		const auto token = static_cast<std::size_t>(std::ranges::find(tokenNames, name) - tokenNames.begin());
		if(token == 0 || token == tokenNames.size() || !value.is_string()) // "plain" is "textColor"
			throw std::invalid_argument("syntax." + name +
			                            ": expected keyword, type, string, number, comment or heading, with a colour");
		editor.setTokenColor(static_cast<CodeEditor::Token>(token), screen.resolveColor(value.get<std::string>()));
	}
}

/// @brief Applies a "code" node's properties to its editor, in an order that keeps the language before its colours.
/// @param screen The screen.
/// @param editor The editor.
/// @param properties The node's properties.
/// @param buffers The application's texts by id, or an empty function.
void applyProperties(affresco::Screen& screen, ScreenCodeEditor& editor, const affresco::json& properties,
                     const BufferLookup& buffers) {
	editor.hoverAction = actionOf(properties, "onHover");
	editor.typeAction = actionOf(properties, "onType");
	applyPopup(screen, editor, properties);
	applyInsert(editor, properties);
	applyBuffer(editor, properties, buffers);
	editor.setLanguage(properties.contains("language") ? properties["language"].get<std::string>() : "");
	editor.resetTokenColors();
	applyTokens(editor, properties);
	applyGoTo(editor, properties);
	applySyntax(screen, editor, properties);
}

} // namespace

void registerCodeEditor(affresco::Screen& screen, BufferLookup buffers) {
	screen.registerType(
	    "code", [&screen] { return makeScreenCodeEditor(screen); },
	    {"language", "syntax", "goTo", "buffer", "onHover", "onType", "popup", "onPopupKey", "insert", "tokens"},
	    [&screen, buffers = std::move(buffers)](affresco::Panel& panel, const std::string& propertiesJson) {
		    auto& editor = dynamic_cast<ScreenCodeEditor&>(panel); // the factory above made it
		    applyProperties(screen, editor, affresco::json::parse(propertiesJson), buffers);
	    });
}

} // namespace examples
