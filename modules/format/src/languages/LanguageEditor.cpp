/// @file
/// @brief LanguageEditor's typing: the language's pairs and indentation rules, the keys bound to code commands, a
/// popup's keys, and the events it reports.

#include "LanguageEditor.hpp"

#include "Shortcuts.hpp"

#include "affresco/render/Window.hpp"

#include <algorithm>
#include <string>

namespace affresco::format {

namespace {

constexpr std::size_t maxRuleBytes = 16 * 1024; ///< Lines longer than this aren't matched to the indentation rules.
constexpr std::size_t maxLookBack = 256;        ///< Lines looked back for the one an outdent compares with.
constexpr std::uint32_t stepsPerRule = 20'000;  ///< PCRE2 steps of one indentation or word find, at most.

/// @brief A code point in UTF-8.
/// @param c The code point.
/// @return Its bytes.
std::string utf8Of(char32_t c) {
	std::string out;
	if(c < 0x80) out += static_cast<char>(c);
	else if(c < 0x800) out += {static_cast<char>(0xC0 | (c >> 6)), static_cast<char>(0x80 | (c & 0x3F))};
	else if(c < 0x10000)
		out += {static_cast<char>(0xE0 | (c >> 12)), static_cast<char>(0x80 | ((c >> 6) & 0x3F)),
		        static_cast<char>(0x80 | (c & 0x3F))};
	else
		out += {static_cast<char>(0xF0 | (c >> 18)), static_cast<char>(0x80 | ((c >> 12) & 0x3F)),
		        static_cast<char>(0x80 | ((c >> 6) & 0x3F)), static_cast<char>(0x80 | (c & 0x3F))};
	return out;
}

/// @brief The name a popup gets for one of its keys.
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

/// @brief Whether a pair typed just before this character closes itself: the line's end, a blank, a closing
/// bracket, a comma or a semicolon (in front of a word, an opener is just typed).
/// @param next The character after the caret, 0 at the line's end.
bool isPairBoundary(char32_t next) noexcept {
	return next == 0 || next == U' ' || next == U'\t' || next == U')' || next == U']' || next == U'}' || next == U',' ||
	       next == U';';
}

/// @brief Whether a character is part of a word by default: a letter or digit of ASCII, '_', or beyond ASCII.
/// @param c The character.
bool isWordCharacter(char32_t c) noexcept {
	return c == U'_' || (c >= U'0' && c <= U'9') || (c >= U'a' && c <= U'z') || (c >= U'A' && c <= U'Z') || c > 0x7F;
}

} // namespace

LanguageEditor::LanguageEditor() {
	groups_.setStepLimit(stepsPerRule);
	onHoverPosition.connect([this](std::optional<Position> at) {
		json event = at ? placeEvent(*at) : json::object();
		event["type"] = "textHover";
		event["over"] = at.has_value();
		onCode("onTextHover", event);
	});
}

void LanguageEditor::keyEvent(const KeyEvent& event) {
	if(event.action == Action::Release) return TextEditor::keyEvent(event);
	const std::uint64_t before = model().version();
	const bool plainBackspace =
	    event.key == Key::Backspace && !event.mods.super() && !event.mods.control() && !event.mods.alt() && !readOnly();
	const bool handled = popupKey(event) || runBoundCommand(event) || (plainBackspace && deleteEmptyPair());
	// NOLINTNEXTLINE(bugprone-parent-virtual-call): skips TextEditor's handling, and keeps the signals firing
	if(handled) Panel::keyEvent(event);
	else TextEditor::keyEvent(event);
	if((event.key == Key::Backspace || event.key == Key::Delete) && model().version() != before) reportTyping({});
}

void LanguageEditor::charEvent(const CharEvent& event) {
	const std::uint64_t before = model().version();
	if(!language_ || event.codepoint < 0x20 || readOnly() || !cursors().empty()) TextEditor::charEvent(event);
	else typeCharacter(event);
	if(model().version() != before) reportTyping(utf8Of(event.codepoint));
}

bool LanguageEditor::popupKey(const KeyEvent& event) {
	const Mods m = event.mods;
	if(!popup_ || m.super() || m.control() || m.alt() || m.shift()) return false;
	const std::string_view key = popupKeyName(event.key);
	if(key.empty()) return false;
	onCode("onPopupKey", json{{"type", "popupKey"}, {"key", key}});
	return true;
}

bool LanguageEditor::runBoundCommand(const KeyEvent& event) {
	if(commands_.empty()) return false;
	const auto found = commands_.find(shortcut(event));
	if(found == commands_.end() || !runCodeCommand(found->second)) return false;
	if(window()) window()->takeKey(); // its character, if any, isn't typed
	return true;
}

void LanguageEditor::reportTyping(std::string_view text) {
	json event = placeEvent(caret());
	event["type"] = "typed";
	event["text"] = text;
	event["word"] = wordBeforeCaret();
	onCode("onType", event);
}

/* Pairs */

char32_t LanguageEditor::closerOf(char32_t c) const noexcept {
	if(!language_) return 0;
	const auto found = std::ranges::find(language_->pairs, c, &std::pair<char32_t, char32_t>::first);
	return found == language_->pairs.end() ? 0 : found->second;
}

bool LanguageEditor::isCloser(char32_t c) const noexcept {
	return language_ &&
	       std::ranges::find(language_->pairs, c, &std::pair<char32_t, char32_t>::second) != language_->pairs.end();
}

LanguageEditor::PairTyping LanguageEditor::pairTypingFor(char32_t typed) const {
	const Position at = caret();
	const std::u32string& text = lines()[at.line];
	const char32_t next = at.column < text.size() ? text[at.column] : 0;
	const char32_t previous = at.column > 0 ? text[at.column - 1] : 0;
	const char32_t closer = closerOf(typed);
	if(!hasSelection() && isCloser(typed) && next == typed) return PairTyping::StepOver;
	if(closer != 0 && hasSelection()) return PairTyping::Wrap;
	// A quote right after a word most likely ends a string (or is an apostrophe): it is typed alone
	if(closer != 0 && isPairBoundary(next) && (closer != typed || !isWordCharacter(previous))) return PairTyping::Close;
	return PairTyping::None;
}

void LanguageEditor::typeCharacter(const CharEvent& event) {
	const char32_t c = event.codepoint;
	const PairTyping typing = pairTypingFor(c);
	if(typing == PairTyping::None) {
		TextEditor::charEvent(event);
		outdentIfDecreasing();
		return;
	}
	const Position at = caret();
	// NOLINTNEXTLINE(bugprone-parent-virtual-call): the editor types the pair itself, and the signals still fire
	Panel::charEvent(event);
	if(typing == PairTyping::StepOver) return moveCaret({at.line, at.column + 1}, false);
	if(typing == PairTyping::Wrap) {
		const auto [from, to] = selection();
		const std::u32string inner = textIn(from, to);
		beginEdit(Edit::Other);
		erase(from, to);
		const Position start = insert(from, std::u32string(1, c));
		const Position end = insert(start, inner);
		insert(end, std::u32string(1, closerOf(c)));
		setSelection(start, end);
		return endEdit();
	}
	beginEdit(Edit::Typing);
	replaceSelection(std::u32string{c, closerOf(c)});
	const Position inside{at.line, at.column + 1};
	setSelection(inside, inside);
	endEdit();
}

bool LanguageEditor::deleteEmptyPair() {
	const Position c = caret();
	const std::u32string& text = lines()[c.line];
	if(!language_ || hasSelection() || !cursors().empty() || c.column == 0 || c.column >= text.size() ||
	   closerOf(text[c.column - 1]) != text[c.column])
		return false;
	beginEdit(Edit::Deleting);
	erase({c.line, c.column - 1}, {c.line, c.column + 1});
	setSelection({c.line, c.column - 1}, {c.line, c.column - 1});
	endEdit();
	return true;
}

/* Indentation */

bool LanguageEditor::lineMatches(const std::shared_ptr<const TextPattern>& pattern, std::size_t line,
                                 std::size_t upTo) {
	if(!pattern || line >= lines().size() || model().lineLength(line) > maxRuleBytes) return false;
	const std::string text = model().textIn({line, 0}, {line, std::min(upTo, lines()[line].size())});
	return pattern->find(text, 0, groups_).match.has_value();
}

void LanguageEditor::newLine() {
	if(!language_) return TextEditor::newLine();
	const auto [from, to] = selection();
	const std::u32string& text = lines()[from.line];
	const char32_t before = from.column > 0 ? text[from.column - 1] : 0;
	const char32_t after = to.line == from.line && to.column < text.size() ? text[to.column] : 0;
	const bool between =
	    before != 0 && std::ranges::find(language_->brackets, std::pair{before, after}) != language_->brackets.end();
	if(!between && !lineMatches(language_->indentIncrease, from.line, from.column)) return TextEditor::newLine();

	const std::size_t indent = std::min(indentOf(from.line), from.column);
	beginEdit(Edit::Other);
	replaceSelection(U"\n" + std::u32string(indent + tabSize(), U' '));
	const Position inside = caret();
	if(between) {
		// Between a pair of brackets: the closing one goes on its own line, the caret stays on the indented one
		insert(inside, U"\n" + std::u32string(indent, U' '));
		setSelection(inside, inside);
	}
	endEdit();
}

void LanguageEditor::outdentIfDecreasing() {
	const Position at = caret();
	const std::size_t indent = indentOf(at.line);
	if(hasSelection() || indent < tabSize() || !lineMatches(language_->indentDecrease, at.line, at.column)) return;
	// It goes back to a level less than the line before it implies: that line's, or one deeper after an increase
	std::size_t expected = 0;
	for(std::size_t line = at.line, looked = 0; line > 0 && looked < maxLookBack; ++looked) {
		--line;
		const std::size_t before = indentOf(line);
		if(before == lines()[line].size()) continue; // blank
		expected = lineMatches(language_->indentIncrease, line, lines()[line].size()) ? before + tabSize() : before;
		break;
	}
	if(indent < expected) return; // already back
	beginEdit(Edit::Typing);
	erase({at.line, 0}, {at.line, tabSize()});
	const Position moved{at.line, at.column >= tabSize() ? at.column - tabSize() : 0};
	setSelection(moved, moved);
	endEdit();
}

} // namespace affresco::format
